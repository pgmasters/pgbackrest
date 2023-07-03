/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/pack.h"
#include "config/config.h"
#include "protocol/helper.h"
#include "storage/helper.h"
#include "storage/remote/protocol.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Storage driver used for requests

    const StorageRemoteFilterHandler *filterHandler;                // Filter handler list
    unsigned int filterHandlerSize;                                 // Filter handler list size
} storageRemoteProtocolLocal;

/***********************************************************************************************************************************
Set filter handlers
***********************************************************************************************************************************/
FN_EXTERN void
storageRemoteFilterHandlerSet(const StorageRemoteFilterHandler *filterHandler, unsigned int filterHandlerSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, filterHandler);
        FUNCTION_TEST_PARAM(UINT, filterHandlerSize);
    FUNCTION_TEST_END();

    ASSERT(filterHandler != NULL);
    ASSERT(filterHandlerSize > 0);

    storageRemoteProtocolLocal.filterHandler = filterHandler;
    storageRemoteProtocolLocal.filterHandlerSize = filterHandlerSize;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set filter group based on passed filters
***********************************************************************************************************************************/
static void
storageRemoteFilterGroup(IoFilterGroup *const filterGroup, const Pack *const filterPack)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_TEST_PARAM(PACK, filterPack);
    FUNCTION_TEST_END();

    ASSERT(filterGroup != NULL);
    ASSERT(filterPack != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const filterList = pckReadNew(filterPack);

        while (!pckReadNullP(filterList))
        {
            const StringId filterKey = pckReadStrIdP(filterList);
            const Pack *const filterParam = pckReadPackP(filterList);

            // If a compression filter
            IoFilter *filter = compressFilterPack(filterKey, filterParam);

            if (filter != NULL)
            {
                ioFilterGroupAdd(filterGroup, filter);
            }
            // Else a filter handler
            else
            {
                ASSERT(storageRemoteProtocolLocal.filterHandler != NULL);

                // Search for a filter handler
                unsigned int filterIdx = 0;

                for (; filterIdx < storageRemoteProtocolLocal.filterHandlerSize; filterIdx++)
                {
                    // If a match create the filter
                    if (storageRemoteProtocolLocal.filterHandler[filterIdx].type == filterKey)
                    {
                        // Create a filter with parameters
                        if (storageRemoteProtocolLocal.filterHandler[filterIdx].handlerParam != NULL)
                        {
                            ASSERT(filterParam != NULL);

                            ioFilterGroupAdd(
                                filterGroup, storageRemoteProtocolLocal.filterHandler[filterIdx].handlerParam(filterParam));
                        }
                        // Else create a filter without parameters
                        else
                        {
                            ASSERT(storageRemoteProtocolLocal.filterHandler[filterIdx].handlerNoParam != NULL);
                            ASSERT(filterParam == NULL);

                            ioFilterGroupAdd(filterGroup, storageRemoteProtocolLocal.filterHandler[filterIdx].handlerNoParam());
                        }

                        // Break on filter match
                        break;
                    }
                }

                // Error when the filter was not found
                if (filterIdx == storageRemoteProtocolLocal.filterHandlerSize)
                    THROW_FMT(AssertError, "unable to add filter '%s'", strZ(strIdToStr(filterKey)));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemoteFeatureProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get storage based on remote type
        const Storage *storage =
            cfgOptionStrId(cfgOptRemoteType) == protocolStorageTypeRepo ? storageRepoWrite() : storagePgWrite();

        // Store local variables in the server context
        if (storageRemoteProtocolLocal.memContext == NULL)
        {
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                MEM_CONTEXT_NEW_BEGIN(StorageRemoteProtocol, .childQty = MEM_CONTEXT_QTY_MAX)
                {
                    storageRemoteProtocolLocal.memContext = memContextCurrent();
                    storageRemoteProtocolLocal.driver = storageDriver(storage);
                }
                MEM_CONTEXT_NEW_END();
            }
            MEM_CONTEXT_PRIOR_END();
        }

        // Return storage features
        PackWrite *result = protocolPackNew();
        pckWriteStrP(result, storagePathP(storage, NULL));
        pckWriteU64P(result, storageInterface(storage).feature);

        protocolServerDataPut(server, result);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct StorageRemoteInfoProcotolWriteData
{
    time_t timeModifiedLast;                                        // timeModified from last call
    mode_t modeLast;                                                // mode from last call
    uid_t userIdLast;                                               // userId from last call
    gid_t groupIdLast;                                              // groupId from last call
    String *user;                                                   // user from last call
    String *group;                                                  // group from last call
} StorageRemoteInfoProtocolWriteData;

// Helper to write storage info into the protocol. This function is not called unless the info exists so no need to write exists or
// check for level == storageInfoLevelExists.
//
// Fields that do not change from one call to the next are omitted to save bandwidth.
static void
storageRemoteInfoProtocolPut(
    StorageRemoteInfoProtocolWriteData *const data, PackWrite *const write, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(PACK_WRITE, write);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(data != NULL);
    ASSERT(write != NULL);
    ASSERT(info != NULL);

    // Write type and time
    pckWriteU32P(write, info->type);
    pckWriteTimeP(write, info->timeModified - data->timeModifiedLast);

    // Write size for files
    if (info->type == storageTypeFile)
        pckWriteU64P(write, info->size);

    // Write fields needed for detail level
    if (info->level >= storageInfoLevelDetail)
    {
        // Write mode
        pckWriteModeP(write, info->mode, .defaultValue = data->modeLast);

        // Write user id/name
        pckWriteU32P(write, info->userId, .defaultValue = data->userIdLast);

        if (info->user == NULL)                                                                                     // {vm_covered}
            pckWriteBoolP(write, true);                                                                             // {vm_covered}
        else
        {
            pckWriteBoolP(write, false);
            pckWriteStrP(write, info->user, .defaultValue = data->user);
        }

        // Write group id/name
        pckWriteU32P(write, info->groupId, .defaultValue = data->groupIdLast);

        if (info->group == NULL)                                                                                    // {vm_covered}
            pckWriteBoolP(write, true);                                                                             // {vm_covered}
        else
        {
            pckWriteBoolP(write, false);
            pckWriteStrP(write, info->group, .defaultValue = data->group);
        }

        // Write link destination
        if (info->type == storageTypeLink)
            pckWriteStrP(write, info->linkDestination);
    }

    // Store defaults to use for the next call
    data->timeModifiedLast = info->timeModified;
    data->modeLast = info->mode;
    data->userIdLast = info->userId;
    data->groupIdLast = info->groupId;

    if (info->user != NULL && !strEq(info->user, data->user))                                                       // {vm_covered}
    {
        strFree(data->user);
        data->user = strDup(info->user);
    }

    if (info->group != NULL && !strEq(info->group, data->group))                                                    // {vm_covered}
    {
        strFree(data->group);
        data->group = strDup(info->group);
    }

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
storageRemoteInfoProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get file info
        const String *file = pckReadStrP(param);
        StorageInfoLevel level = (StorageInfoLevel)pckReadU32P(param);
        bool followLink = pckReadBoolP(param);

        StorageInfo info = storageInterfaceInfoP(storageRemoteProtocolLocal.driver, file, level, .followLink = followLink);

        // Write file info to protocol
        PackWrite *write = protocolPackNew();
        pckWriteBoolP(write, info.exists, .defaultWrite = true);

        if (info.exists)
            storageRemoteInfoProtocolPut(&(StorageRemoteInfoProtocolWriteData){0}, write, &info);

        protocolServerDataPut(server, write);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemoteLinkCreateProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const target = pckReadStrP(param);
        const String *const linkPath = pckReadStrP(param);
        const StorageLinkType linkType = (StorageLinkType)pckReadU32P(param);

        storageInterfaceLinkCreateP(storageRemoteProtocolLocal.driver, target, linkPath, .linkType = linkType);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemoteListProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const path = pckReadStrP(param);
        const StorageInfoLevel level = (StorageInfoLevel)pckReadU32P(param);
        StorageRemoteInfoProtocolWriteData writeData = {0};
        StorageList *const list = storageInterfaceListP(storageRemoteProtocolLocal.driver, path, level);
        PackWrite *const write = protocolPackNew();

        // Indicate whether or not the path was found
        pckWriteBoolP(write, list != NULL, .defaultWrite = true);

        // Put list
        if (list != NULL)
        {
            for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
            {
                const StorageInfo info = storageLstGet(list, listIdx);

                pckWriteObjBeginP(write);
                pckWriteStrP(write, info.name);
                storageRemoteInfoProtocolPut(&writeData, write, &info);
                pckWriteObjEndP(write);
            }
        }

        protocolServerDataPut(server, write);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static bool
storageRemoteReadInternal(StorageRead *const fileRead, PackWrite *const packWrite)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_READ, fileRead);
        FUNCTION_LOG_PARAM(PACK_WRITE, packWrite);
    FUNCTION_LOG_END();

    ASSERT(fileRead != NULL);
    ASSERT(packWrite != NULL);

    // Read block and send to client
    Buffer *const buffer = bufNew(ioBufferSize());

    ioRead(storageReadIo(fileRead), buffer);
    pckWriteBoolP(packWrite, bufEmpty(buffer));

    if (!bufEmpty(buffer))
        pckWriteBinP(packWrite, buffer);

    // On eof
    bool result = true;

    pckWriteBoolP(packWrite, ioReadEof(storageReadIo(fileRead)));

    if (ioReadEof(storageReadIo(fileRead)))
    {
        // Close file (needed to get filter results)
        ioReadClose(storageReadIo(fileRead));

        // Write filter results
        Pack *const filterData = ioFilterGroupResultAll(ioReadFilterGroup(storageReadIo(fileRead)));

        pckWritePackP(packWrite, filterData);
        pckFree(filterData);

        // Let the server know to close the session
        result = false;
    }

    // Free read buffer
    bufFree(buffer);

    FUNCTION_LOG_RETURN(BOOL, result);
}

FN_EXTERN void *
storageRemoteReadOpenProtocol(PackRead *const param, ProtocolServer *const server, const uint64_t sessionId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(UINT64, sessionId);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(sessionId != 0);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    StorageRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *file = pckReadStrP(param);
        bool ignoreMissing = pckReadBoolP(param);
        const uint64_t offset = pckReadU64P(param);
        const Variant *const limit = pckReadNullP(param) ? NULL : VARUINT64(pckReadU64P(param));
        const Pack *const filter = pckReadPackP(param);

        // Create the read object
        StorageRead *const fileRead = storageInterfaceNewReadP(
            storageRemoteProtocolLocal.driver, file, ignoreMissing, .offset = offset, .limit = limit);

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioReadFilterGroup(storageReadIo(fileRead)), filter);

        // Determine if file exists
        const bool exists = ioReadOpen(storageReadIo(fileRead));
        PackWrite *const packWrite = protocolPackNew();

        pckWriteBoolP(packWrite, exists, .defaultWrite = true);

        // If the file exists
        if (exists)
        {
            // If there is more to read then store IoRead in the session
            if (storageRemoteReadInternal(fileRead, packWrite))
                result = storageReadMove(fileRead, memContextPrior());
        }

        protocolServerDataPut(server, packWrite);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, result);
}

FN_EXTERN bool
storageRemoteReadProtocol(PackRead *const param, ProtocolServer *const server, void *const fileRead)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(STORAGE_READ, fileRead);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);
    ASSERT(fileRead != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    PackWrite *const packWrite = pckWriteNewP(.size = ioBufferSize() + PROTOCOL_PACK_DEFAULT_SIZE);
    const bool result = storageRemoteReadInternal(fileRead, packWrite);

    protocolServerDataPut(server, packWrite);
    protocolServerDataEndPut(server);
    pckWriteFree(packWrite);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void *
storageRemoteWriteOpenProtocol(PackRead *const param, ProtocolServer *const server, const uint64_t sessionId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(UINT64, sessionId);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(sessionId != 0);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    StorageWrite *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the write object
        const String *const file = pckReadStrP(param);
        const mode_t modeFile = pckReadModeP(param);
        const mode_t modePath = pckReadModeP(param);
        const String *const user = pckReadStrP(param);
        const String *const group = pckReadStrP(param);
        const time_t timeModified = pckReadTimeP(param);
        const bool createPath = pckReadBoolP(param);
        const bool syncFile = pckReadBoolP(param);
        const bool syncPath = pckReadBoolP(param);
        const bool atomic = pckReadBoolP(param);
        const Pack *const filter = pckReadPackP(param);

        StorageWrite *const fileWrite = storageInterfaceNewWriteP(
            storageRemoteProtocolLocal.driver, file, .modeFile = modeFile, .modePath = modePath, .user = user, .group = group,
            .timeModified = timeModified, .createPath = createPath, .syncFile = syncFile, .syncPath = syncPath, .atomic = atomic,
            .truncate = true);

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioWriteFilterGroup(storageWriteIo(fileWrite)), filter);

        // Open file
        ioWriteOpen(storageWriteIo(fileWrite));

        // Notify client that open is complete
        protocolServerDataEndPut(server);

        // Move file to calling context
        result = storageWriteMove(fileWrite, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, result);
}

FN_EXTERN bool
storageRemoteWriteProtocol(PackRead *const param, ProtocolServer *const server, void *const fileWrite)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(STORAGE_READ, fileWrite);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(fileWrite != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    Buffer *const buffer = pckReadBinP(param);

    ioWrite(storageWriteIo(fileWrite), buffer);
    bufFree(buffer);

    protocolServerDataEndPut(server);

    FUNCTION_LOG_RETURN(BOOL, true);
}

FN_EXTERN void
storageRemoteWriteCloseProtocol(PackRead *const param, ProtocolServer *const server, void *const fileWrite)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, fileWrite);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);
    ASSERT(fileWrite != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioWriteClose(storageWriteIo(fileWrite));

        // Send filter results
        protocolServerDataPut(
            server, pckWritePackP(protocolPackNew(), ioFilterGroupResultAll(ioWriteFilterGroup(storageWriteIo(fileWrite)))));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemotePathCreateProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *path = pckReadStrP(param);
        bool errorOnExists = pckReadBoolP(param);
        bool noParentCreate = pckReadBoolP(param);
        mode_t mode = pckReadModeP(param);

        storageInterfacePathCreateP(storageRemoteProtocolLocal.driver, path, errorOnExists, noParentCreate, mode);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemotePathRemoveProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *path = pckReadStrP(param);
        bool recurse = pckReadBoolP(param);

        const bool result = storageInterfacePathRemoveP(storageRemoteProtocolLocal.driver, path, recurse);

        protocolServerDataPut(server, pckWriteBoolP(protocolPackNew(), result, .defaultWrite = true));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemotePathSyncProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *path = pckReadStrP(param);

        storageInterfacePathSyncP(storageRemoteProtocolLocal.driver, path);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageRemoteRemoveProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *file = pckReadStrP(param);
        bool errorOnMissing = pckReadBoolP(param);

        storageInterfaceRemoveP(storageRemoteProtocolLocal.driver, file, .errorOnMissing = errorOnMissing);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
