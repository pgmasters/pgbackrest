/***********************************************************************************************************************************
Sftp Storage
***********************************************************************************************************************************/
#include "build.auto.h"

//#ifdef HAVE_LIBSSH2

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/fd.h"
#include "common/io/socket/client.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/user.h"
#include "common/wait.h"
#include "storage/sftp/read.h"
#include "storage/sftp/storage.intern.h"
#include "storage/sftp/write.h"

/***********************************************************************************************************************************
Define PATH_MAX if it is not defined
***********************************************************************************************************************************/
#ifndef PATH_MAX
    #define PATH_MAX                                                (4 * 1024)
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageSftp
{
    STORAGE_COMMON_MEMBER;

    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SFTP_HANDLE *sftpHandle;
    LIBSSH2_SFTP_ATTRIBUTES *attr;
    TimeMSec timeoutConnect;
    TimeMSec timeoutSession;
    Wait *wait;
};

/**********************************************************************************************************************************/
static StorageInfo
storageSftpInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {.level = level};

    // Stat the file to check if it exists
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    if (param.followLink)
    {
        do
        {
            rc = libssh2_sftp_stat_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file), LIBSSH2_SFTP_STAT, &attrs);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));
    }
    else
    {
        do
        {
            rc = libssh2_sftp_stat_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file), LIBSSH2_SFTP_LSTAT, &attrs);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));
    }

    if (rc)
    {
        //!!! verify/validate the below notes
        //int errval = errno;
        // !!! jrt libssh2 on missing file returns sftp error 2 LIBSSH2_FX_NO_SUCH_FILE but it or something else evidently sets
        // !!! errno to 11 - Resource temporarily unavailable
        // !!! do we override errno and set it 2, or do we update tests to match
//        LOG_DEBUG_FMT("jrt errno %d", errval);
//        LOG_DEBUG_FMT("jrt error on ssh2 stat %d", libssh2_session_last_errno(this->session));
        // If session indicates sftp error, can query for sftp error
        // !!! see also libssh2_session_last_error() - possible to return more detailed error
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
//        LOG_DEBUG_FMT("jrt sftp error %lu", libssh2_sftp_last_error(this->sftpSession));
//            if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
//                errno = 2;
            if (libssh2_sftp_last_error(this->sftpSession) != LIBSSH2_FX_NO_SUCH_FILE)
                THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_INFO, strZ(file));
        }
    }
    // On success the file exists
    else
    {
        result.exists = true;

        // Add type info (no need set file type since it is the default)
        if (result.level >= storageInfoLevelType && !LIBSSH2_SFTP_S_ISREG(attrs.permissions))
        {
            if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions))
                result.type = storageTypePath;
            else if (LIBSSH2_SFTP_S_ISLNK(attrs.permissions))
                result.type = storageTypeLink;
            else
                result.type = storageTypeSpecial;
        }

        // Add basic level info
        if (result.level >= storageInfoLevelBasic)
        {
            if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
                result.timeModified = (time_t)attrs.mtime;

            if (result.type == storageTypeFile)
                if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
                    result.size = (uint64_t)attrs.filesize;
        }

        // Add detail level info
        if (result.level >= storageInfoLevelDetail)
        {
            if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
            {
                result.groupId = (unsigned int)attrs.gid;
                result.group = groupNameFromId(result.groupId);
                result.userId = (unsigned int)attrs.uid;
                result.user = userNameFromId(result.userId);
            }
            //jrt !!! verify this
            if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
                result.mode = attrs.permissions & (LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO);

            if (result.type == storageTypeLink)
            {
                char linkDestination[PATH_MAX + 1] = {};
                ssize_t linkDestinationSize = 0;

                do
                {
                    linkDestinationSize = libssh2_sftp_symlink_ex(
                        this->sftpSession, strZ(file), (unsigned int)strSize(file), linkDestination, PATH_MAX,
                        LIBSSH2_SFTP_READLINK);
                }
                while (linkDestinationSize == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

                THROW_ON_SYS_ERROR_FMT(
                    linkDestinationSize < 0, FileReadError, "unable to get destination for link '%s'", strZ(file));

                result.linkDestination = strNewZN(linkDestination, (size_t)linkDestinationSize);
            }
        }
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
// Helper function to get info for a file if it exists.  This logic can't live directly in storageSftpInfoList() because there is
// a race condition where a file might exist while listing the directory but it is gone before stat() can be called.  In order to
// get complete test coverage this function must be split out.

static void
storageSftpInfoListEntry(
    StorageSftp *this, const String *path, const String *name, StorageInfoLevel level, StorageInfoListCallback callback,
    void *callbackData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_SFTP, this);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ENUM, level);
        FUNCTION_TEST_PARAM(FUNCTIONP, callback);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(name != NULL);
    ASSERT(callback != NULL);

    StorageInfo storageInfo = storageInterfaceInfoP(
        this, strEq(name, DOT_STR) ? strDup(path) : strNewFmt("%s/%s", strZ(path), strZ(name)), level);

    if (storageInfo.exists)
    {
        storageInfo.name = name;
        callback(callbackData, &storageInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageSftpInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(callback != NULL);

    bool result = false;

    // Open the directory for read
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        this->sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(path), (unsigned int)strSize(path), 0, 0, LIBSSH2_SFTP_OPENDIR);
    }
    while (this->sftpHandle == NULL && libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    // If the directory could not be opened process errors and report missing directories
    if (this->sftpHandle == NULL)
    {
        // If session indicates sftp error, can query for sftp error
        // !!! see also libssh2_session_last_error() - possible to return more detailed error
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            //jrt ??? should check be against LIBSSH2_FX_NO_SUCH_FILE and LIBSSH2_FX_NO_SUCH_PATH
            //verify PATH or FILE or both
            if (libssh2_sftp_last_error(this->sftpSession) != LIBSSH2_FX_NO_SUCH_FILE)
                THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_LIST_INFO_MISSING, strZ(path));
        }
    }
    else
    {
        // Directory was found
        result = true;

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                char filename[PATH_MAX + 1] = {};
                int len = 0;
                LIBSSH2_SFTP_ATTRIBUTES attrs;

                this->wait = waitNew(this->timeoutConnect);

                do
                {
                    len = libssh2_sftp_readdir_ex(this->sftpHandle, filename, PATH_MAX, NULL, 0, &attrs);

                    if (len > 0)
                    {
                        filename[len] = '\0';
                        const String *name = STR(filename);

                        // Always skip ..
                        if (!strEq(name, DOTDOT_STR))
                        {
                            if (level == storageInfoLevelExists)
                            {
                                callback(
                                    callbackData, &(StorageInfo){.name = name, .level = storageInfoLevelExists, .exists = true});
                            }
                            else
                                storageSftpInfoListEntry(this, path, name, level, callback, callbackData);
                        }
                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);

                        // jrt  ?? is this needed
                        this->wait = waitNew(this->timeoutConnect);
                    }
                }
                while ((len == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait)) || len > 0);
            }
            MEM_CONTEXT_TEMP_END();
        }
        FINALLY()
        {
            int rc = 0;
            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_closedir(this->sftpHandle);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            if (rc != 0)
                THROW_SYS_ERROR_FMT(PathCloseError, "unable to close path %s after listing", strZ(path));
        }
        TRY_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static void
storageSftpRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to unlink the file
    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(file), (unsigned int)strSize(file));
    } while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            //jrt verify PATH or FILE or both
            if (libssh2_sftp_last_error(this->sftpSession) != LIBSSH2_FX_NO_SUCH_FILE)
            {
                if (param.errorOnMissing)
                    THROW_SYS_ERROR_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));
            }
        }
        // ssh error
        else
        {
            if (param.errorOnMissing)
                THROW_SYS_ERROR_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static StorageRead *
storageSftpNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadSftpNew(
            this, file, ignoreMissing, this->session, this->sftpSession, this->sftpHandle, this->timeoutSession,
            this->timeoutConnect, param.offset, param.limit));
}

/**********************************************************************************************************************************/
static StorageWrite *
storageSftpNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.group);
        FUNCTION_LOG_PARAM(TIME, param.timeModified);
        FUNCTION_LOG_PARAM(BOOL, param.createPath);
        FUNCTION_LOG_PARAM(BOOL, param.syncFile);
        FUNCTION_LOG_PARAM(BOOL, param.syncPath);
        FUNCTION_LOG_PARAM(BOOL, param.atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWriteSftpNew(
            this, file, this->session, this->sftpSession, this->sftpHandle, this->attr, this->timeoutConnect, this->timeoutSession,
            param.modeFile, param.modePath, param.user, param.group, param.timeModified, param.createPath, param.syncFile,
            this->interface.pathSync != NULL ? param.syncPath : false, param.atomic));
}

/**********************************************************************************************************************************/
void
storageSftpPathCreate(
    THIS_VOID, const String *const path, const bool errorOnExists, const bool noParentCreate, const mode_t mode,
    const StorageInterfacePathCreateParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    // jrt ?? is mode coming in as sftp mode flags??
    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    // Attempt to create the directory
    do
    {
        rc = libssh2_sftp_mkdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path), (int)mode);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
         // If session indicates sftp error, can query for sftp error
         // !!! see also libssh2_session_last_error() - possible to return more detailed error
         if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
         {
             // libssh2 may return LIBSSH2_FX_FAILURE if the directory already exists
            // need to find out if we can determine server version
             if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_FAILURE)
             {
                 // Check if the directory already exists
                 LIBSSH2_SFTP_ATTRIBUTES attrs;
                 this->wait = waitNew(this->timeoutConnect);

                 do
                 {
                     rc = libssh2_sftp_stat_ex(
                        this->sftpSession, strZ(path), (unsigned int)strSize(path), LIBSSH2_SFTP_STAT, &attrs);
                 }
                 while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

                 if (rc)
                 {
                     THROW_SYS_ERROR_FMT(
                        PathCreateError, "unable to create path '%s' '%lu'", strZ(path),
                        libssh2_sftp_last_error(this->sftpSession));
                 }
                 // Otherwise it already exists
                 // ??? Make an explicit call here to set the mode in case it's different from the existing directory's mode ???
             }
             // If the parent path does not exist then create it if allowed
             //jrt FILE && PATH??
             else if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE && !noParentCreate)
             {
                 String *const pathParent = strPath(path);

                 storageInterfacePathCreateP(this, pathParent, errorOnExists, noParentCreate, mode);
                 storageInterfacePathCreateP(this, path, errorOnExists, noParentCreate, mode);

                 strFree(pathParent);
             }
             // Ignore path exists if allowed
             else if (libssh2_sftp_last_error(this->sftpSession) != LIBSSH2_FX_FILE_ALREADY_EXISTS || errorOnExists)
             {
                 THROW_SYS_ERROR_FMT(
                    PathCreateError, "unable to create path '%s' '%lu'", strZ(path), libssh2_sftp_last_error(this->sftpSession));
             }
         }
         // ssh error
         else
             THROW_SYS_ERROR_FMT(PathCreateError, "unable to create path '%s'", strZ(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct StorageSftpPathRemoveData
{
    StorageSftp *driver;                                            // Driver
    const String *path;                                             // Path
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SESSION *session;
    TimeMSec timeoutConnect;
    Wait *wait;
} StorageSftpPathRemoveData;

static void
storageSftpPathRemoveCallback(void *const callbackData, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM_P(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(info != NULL);

    if (!strEqZ(info->name, "."))
    {
        StorageSftpPathRemoveData *const data = callbackData;
        String *const file = strNewFmt("%s/%s", strZ(data->path), strZ(info->name));

        // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
        int rc = 0;
        data->wait = waitNew(data->timeoutConnect);

        do
        {
            rc = libssh2_sftp_unlink_ex(data->sftpSession, strZ(file), (unsigned int)strSize(file));
        } while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(data->wait));

        if (rc != 0)
        {
            // If session indicates sftp error, can query for sftp error
            // !!! see also libssh2_session_last_error() - possible to return more detailed error
            if (libssh2_session_last_errno(data->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
            {
                // Determine which file/path is missing
                //if (errno == ENOENT)
                //jrt verify PATH or FILE or both
                if (libssh2_sftp_last_error(data->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
                {
                    //jrt check for session/sftp error
                    // These errors indicate that the entry is actually a path so we'll try to delete it that way
                    // !!! jrt will have to determine which LIBSSH2_FX responses are equivalent
                    if (errno == EPERM || errno == EISDIR)              // {uncovered_branch - no EPERM on tested systems}
                    {
                        storageInterfacePathRemoveP(data->driver, file, true);
                    }
                    // Else error
                    else
                        THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE, strZ(file));
                }
            }
            // ssh error
            else
                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE, strZ(file));
        }

        strFree(file);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageSftpPathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    THIS(StorageSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Recurse if requested
        if (recurse)
        {
            StorageSftpPathRemoveData data =
            {
                .driver = this,
                .path = path,
                .session = this->session,
                .sftpSession = this->sftpSession,
                .timeoutConnect = this->timeoutConnect,
                .wait = this->wait,
            };

            // Remove all sub paths/files
            storageInterfaceInfoListP(this, path, storageInfoLevelExists, storageSftpPathRemoveCallback, &data);
        }

        // Delete the path
        // !!! jrt need to validate this logic matches other drivers
        int rc = 0;
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            rc = libssh2_sftp_rmdir_ex(this->sftpSession, strZ(path), (unsigned int)strSize(path));
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        if (rc != 0)
        {
            // If session indicates sftp error, can query for sftp error
            // !!! see also libssh2_session_last_error() - possible to return more detailed error
            if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
            {
                //jrt verify PATH or FILE or both
                if (libssh2_sftp_last_error(this->sftpSession) != LIBSSH2_FX_NO_SUCH_FILE)
                {
                    THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(path));

                    // Path does not exist
                    result = false;
                }
            }
            else
            {
                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(path));
                // Path does not exist

                // ssh error
                result = false;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceSftp =
{
    .feature = 1 << storageFeaturePath,

    .info = storageSftpInfo,
    .infoList = storageSftpInfoList,
    .newRead = storageSftpNewRead,
    .newWrite = storageSftpNewWrite,
    .pathCreate = storageSftpPathCreate,
    .pathRemove = storageSftpPathRemove,
    .remove = storageSftpRemove,
};

Storage *
storageSftpNewInternal(
    StringId type, const String *path, const String *host, unsigned int port, TimeMSec timeoutConnect, TimeMSec timeoutSession,
    const String *user, const String *password, const String *keyPub, const String *keyPriv, mode_t modeFile, mode_t modePath,
    bool write, StoragePathExpressionCallback pathExpressionFunction, bool pathSync)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, password);
        FUNCTION_LOG_PARAM(STRING, keyPub);
        FUNCTION_LOG_PARAM(STRING, keyPriv);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(BOOL, pathSync);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Initialize user module
    userInit();

    // Create the object
    Storage *this = NULL;

    OBJ_NEW_BEGIN(StorageSftp, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StorageSftp *driver = OBJ_NEW_ALLOC();

        //jrt !!! error handling

        if (libssh2_init(0) != 0)
            THROW_SYS_ERROR_FMT(ServiceError, "unable to init libssh2");

        IoSession *ioSession = ioClientOpen(sckClientNew(host, port, timeoutConnect, timeoutSession));

        *driver = (StorageSftp)
        {
            .interface = storageInterfaceSftp,
            .session = libssh2_session_init(),
            // jrt !!! find a single location for timeouts that will be accesible everywhere
            .timeoutConnect = timeoutConnect,
            .timeoutSession = timeoutSession,
        };

        if (driver->session == NULL)
            THROW_SYS_ERROR_FMT(ServiceError, "unable to init libssh2 session");

        //!!! jrt add error handling
        libssh2_session_set_blocking(driver->session, 0);

        int rc = 0;
        Wait *wait = waitNew(timeoutConnect);

        do
        {
            rc = libssh2_session_handshake(driver->session, ioSessionFd(ioSession));
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

        //!!! jrt allow for stronger hash sha - newer versions have sha256 i think
        libssh2_hostkey_hash(driver->session, LIBSSH2_HOSTKEY_HASH_SHA1);

        rc = 0;

        if (!strEmpty(user) && !strEmpty(password))
        {
            wait = waitNew(timeoutConnect);

            do
            {
                rc = libssh2_userauth_password(driver->session, strZ(user), strZ(password));
            } while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(wait));

            if (rc)
                THROW_SYS_ERROR_FMT(ServiceError, "timeout password authenticate %d", rc);
        }
        else if (!strEmpty(user) && !strEmpty(keyPub) && !strEmpty(keyPriv))
        {
            // !!! jrt need to handle keyfile passphrase
            libssh2_userauth_publickey_fromfile(driver->session, strZ(user), strZ(keyPub), strZ(keyPriv), "");
        }

        wait = waitNew(timeoutConnect);

        do
        {
            driver->sftpSession = libssh2_sftp_init(driver->session);
        }
        while (driver->sftpSession == NULL && waitMore(wait));

        if (driver->sftpSession == NULL)
            THROW_SYS_ERROR_FMT(ServiceError, "unable to init libssh2_sftp session");

        // Disable path sync when not supported
        // !!! jrt libssh2 doesn't appear to support path sync -- test/verify and set appropriately
        if (!pathSync)
            driver->interface.pathSync = NULL;

        // If this is a sftp driver then add link features
        // jrt !!! verify if these are supported
        if (type == STORAGE_SFTP_TYPE)
            driver->interface.feature |=
                1 << storageFeatureHardLink | 1 << storageFeatureSymLink | 1 << storageFeaturePathSync |
                1 << storageFeatureInfoDetail;

        this = storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}

Storage *
storageSftpNew(const String *path, const String *host, unsigned int port, TimeMSec timeoutConnect, TimeMSec timeoutSession,
    StorageSftpNewParam param)
{
// jrt update all FUNCTION_LOG* for new params in updated functs
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.password);
        FUNCTION_LOG_PARAM(STRING, param.keyPub);
        FUNCTION_LOG_PARAM(STRING, param.keyPriv);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(BOOL, param.write);
        FUNCTION_LOG_PARAM(FUNCTIONP, param.pathExpressionFunction);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        STORAGE,
        storageSftpNewInternal(
            STORAGE_SFTP_TYPE, path, host, port, timeoutConnect, timeoutSession, param.user, param.password, param.keyPub,
            param.keyPriv, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile, param.modePath == 0 ?
            STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, param.pathExpressionFunction, false));
}

//#endif // HAVE_LIBSSH2
