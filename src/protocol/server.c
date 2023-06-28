/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/error/retry.h"
#include "common/log.h"
#include "common/time.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "version.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolServer
{
    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
    const String *name;                                             // Name displayed in logging
    List *sessionList;                                              // List of active sessions
    uint64_t sessionTotal;                                          // Total sessions (used to generate session ids);
};

// Track server sessions
typedef struct ProtocolServerSession
{
    uint64_t id;                                                    // Session id
    void *data;                                                     // Data for the session
} ProtocolServerSession;

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServer *
protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(STRING, service);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);
    ASSERT(read != NULL);
    ASSERT(write != NULL);

    OBJ_NEW_BEGIN(ProtocolServer, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (ProtocolServer)
        {
            .read = read,
            .write = write,
            .name = strDup(name),
            .sessionList = lstNewP(sizeof(ProtocolServerSession)),
        };

        // Send the protocol greeting
        MEM_CONTEXT_TEMP_BEGIN()
        {
            JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

            jsonWriteZ(jsonWriteKeyStrId(json, PROTOCOL_GREETING_NAME), PROJECT_NAME);
            jsonWriteStr(jsonWriteKeyStrId(json, PROTOCOL_GREETING_SERVICE), service);
            jsonWriteZ(jsonWriteKeyStrId(json, PROTOCOL_GREETING_VERSION), PROJECT_VERSION);

            ioWriteStrLine(this->write, jsonWriteResult(jsonWriteObjectEnd(json)));
            ioWriteFlush(this->write);
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
        FUNCTION_LOG_PARAM(STRING, stack);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(code != 0);
    ASSERT(message != NULL);
    ASSERT(stack != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write the error and flush to be sure it gets sent immediately
        PackWrite *error = pckWriteNewIo(this->write);
        pckWriteU32P(error, protocolMessageTypeError);
        pckWriteI32P(error, code);
        pckWriteStrP(error, message);
        pckWriteStrP(error, stack);
        pckWriteEndP(error);

        ioWriteFlush(this->write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerCommandGetResult
protocolServerCommandGet(ProtocolServer *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ProtocolServerCommandGetResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const command = pckReadNewIo(this->read);
        ProtocolMessageType type = (ProtocolMessageType)pckReadU32P(command);

        CHECK(FormatError, type == protocolMessageTypeCommand, "expected command message");

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.id = pckReadStrIdP(command);
            result.type = (ProtocolCommandType)pckReadStrIdP(command);
            result.sessionId = pckReadU64P(command);
            result.param = pckReadPackP(command);
        }
        MEM_CONTEXT_PRIOR_END();

        pckReadEndP(command);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *const handlerList,
    const unsigned int handlerListSize)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(VARIANT_LIST, retryInterval);
        FUNCTION_LOG_PARAM_P(VOID, handlerList);
        FUNCTION_LOG_PARAM(UINT, handlerListSize);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(handlerList != NULL);
    ASSERT(handlerListSize > 0);

    // Loop until exit command is received
    bool exit = false;

    do
    {
        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Get command
                ProtocolServerCommandGetResult command = protocolServerCommandGet(this);

                // Find the handler
                const ProtocolServerHandler *handler = NULL;

                for (unsigned int handlerIdx = 0; handlerIdx < handlerListSize; handlerIdx++)
                {
                    if (command.id == handlerList[handlerIdx].command)
                    {
                        handler = &handlerList[handlerIdx];
                        break;
                    }
                }

                // If handler was found then process
                if (handler != NULL)
                {
                    // Send the command to the handler
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        // Variables to store first error message and retry messages
                        ErrorRetry *const errRetry = errRetryNew();
                        const String *errStackTrace = NULL;

                        // Initialize retries in case of command failure
                        bool retry = false;
                        unsigned int retryRemaining = retryInterval != NULL ? varLstSize(retryInterval) : 0;
                        TimeMSec retrySleepMs = 0;

                        // Handler retry loop
                        do
                        {
                            retry = false;

                            TRY_BEGIN()
                            {
                                // Process command type
                                switch (command.type)
                                {
                                    // Open a protocol session
                                    case protocolCommandTypeOpen:
                                    {
                                        ASSERT(command.sessionId == 0);
                                        ASSERT(handler->open != NULL);

                                        ProtocolServerSession session = {.id = ++this->sessionTotal};

                                        // Return session id to client
                                        protocolServerDataPut(this, pckWriteU64P(protocolPackNew(), session.id));
//                                        protocolServerDataEndPut(this); //

                                        // Call open handler
                                        MEM_CONTEXT_OBJ_BEGIN(this->sessionList)
                                        {
                                            session.data = handler->open(pckReadNew(command.param), this, session.id);
                                        }
                                        MEM_CONTEXT_OBJ_END();

                                        // Create session if open returned session data
                                        if (session.data != NULL)
                                            lstAdd(this->sessionList, &session);

                                        break;
                                    }

                                    // Process or close protocol session
                                    default:
                                    {
                                        ASSERT(command.type == protocolCommandTypeProcess || protocolCommandTypeClose);

                                        // Find session data
                                        void *sessionData = NULL;
                                        unsigned int sessionListIdx = 0;

                                        if (command.sessionId != 0)
                                        {
                                            ASSERT(handler->processSession != NULL);
                                            ASSERT(handler->process == NULL);

                                            for (; sessionListIdx < lstSize(this->sessionList); sessionListIdx++)
                                            {
                                                ProtocolServerSession *const session = lstGet(this->sessionList, sessionListIdx);

                                                if (session->id == command.sessionId)
                                                {
                                                    sessionData = session->data;
                                                    break;
                                                }
                                            }

                                            // Error when session not found
                                            if (sessionData == NULL)
                                            {
                                                THROW_FMT(
                                                    ProtocolError, "unable to find session id %" PRIu64 " for command %s:%s",
                                                    command.sessionId, strZ(strIdToStr(command.id)),
                                                    strZ(strIdToStr(command.type)));
                                            }
                                        }

                                        // Process command type
                                        switch (command.type)
                                        {
                                            // Process command
                                            case protocolCommandTypeProcess:
                                            {
                                                // Process session
                                                if (handler->processSession != NULL)
                                                {
                                                    ASSERT(handler->process == NULL);
                                                    CHECK_FMT(ProtocolError, command.sessionId != 0, "command is %s:%s", strZ(strIdToStr(command.id)),
                                                              strZ(strIdToStr(command.type)));

                                                    if (!handler->processSession(pckReadNew(command.param), this, sessionData))
                                                    {
                                                        objFree(sessionData);
                                                        lstRemoveIdx(this->sessionList, sessionListIdx);
                                                    }
                                                }
                                                // Standalone process
                                                else
                                                {
                                                    ASSERT(handler->process != NULL);
                                                    ASSERT(command.sessionId == 0);

                                                    handler->process(pckReadNew(command.param), this);
                                                }

                                                break;
                                            }

                                            // Close protocol session
                                            case protocolCommandTypeClose:
                                            {
                                                // If there is a close handler then call it
                                                if (handler->close != NULL)
                                                {
                                                    handler->close(pckReadNew(command.param), this, sessionData);
                                                }
                                                // Else send default end of data
                                                else
                                                    protocolServerDataEndPut(this);

                                                // Free the session
                                                objFree(sessionData);
                                                lstRemoveIdx(this->sessionList, sessionListIdx);

                                                break;
                                            }

                                            // Cancel protocol session
                                            default:
                                            {
                                                CHECK_FMT(
                                                    ProtocolError, command.type == protocolCommandTypeCancel,
                                                    "unknown command type '%s'", strZ(strIdToStr(command.type)));

                                                // Send end of data
                                                protocolServerDataEndPut(this);

                                                // Free the session
                                                objFree(sessionData);
                                                lstRemoveIdx(this->sessionList, sessionListIdx);

                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            CATCH_ANY()
                            {
                                // Add the error retry info
                                errRetryAdd(errRetry);

                                // On first error record the stack trace. Only the first error will contain a stack trace since
                                // the first error is most likely to contain valuable information.
                                if (errStackTrace == NULL)
                                    errStackTrace = strNewZ(errorStackTrace());

                                // Are there retries remaining?
                                if (retryRemaining > 0)
                                {
                                    // Get the sleep interval for this retry
                                    retrySleepMs = varUInt64(varLstGet(retryInterval, varLstSize(retryInterval) - retryRemaining));

                                    // Log the retry
                                    LOG_DEBUG_FMT(
                                        "retry %s after %" PRIu64 "ms: %s", errorTypeName(errorType()), retrySleepMs,
                                        errorMessage());

                                    // Sleep for interval
                                    sleepMSec(retrySleepMs);

                                    // Decrement retries remaining and retry
                                    retryRemaining--;
                                    retry = true;

                                    // Send keep-alive to remotes. A retry means the command is taking longer than usual so make
                                    // sure the remote does not timeout.
                                    protocolKeepAlive();
                                }
                                // Else report error to the client
                                else
                                {
                                    protocolServerError(
                                        this, errorTypeCode(errRetryType(errRetry)), errRetryMessage(errRetry), errStackTrace);
                                }
                            }
                            TRY_END();
                        }
                        while (retry);
                    }
                    MEM_CONTEXT_TEMP_END();
                }
                // Else check built-in commands
                else
                {
                    switch (command.id)
                    {
                        case PROTOCOL_COMMAND_EXIT:
                            exit = true;
                            break;

                        case PROTOCOL_COMMAND_NOOP:
                            protocolServerDataEndPut(this);
                            break;

                        default:
                            THROW_FMT(
                                ProtocolError, "invalid command '%s' (0x%" PRIx64 ")", strZ(strIdToStr(command.id)), command.id);
                    }
                }

                // Send keep-alive to remotes. When a local process is doing work that does not involve the remote it is important
                // that the remote does not timeout. This will send a keep alive once per unit of work that is performed by the
                // local process.
                protocolKeepAlive();
            }
            MEM_CONTEXT_TEMP_END();
        }
        CATCH_FATAL()
        {
            // Report error to the client
            protocolServerError(this, errorCode(), STR(errorMessage()), STR(errorStackTrace()));

            // Rethrow so the process exits with an error
            RETHROW();
        }
        TRY_END();
    }
    while (!exit);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerDataPut(ProtocolServer *const this, PackWrite *const data)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
        FUNCTION_LOG_PARAM(PACK_WRITE, data);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // End the pack
        if (data != NULL)
            pckWriteEndP(data);

        // Write the result
        PackWrite *resultMessage = pckWriteNewIo(this->write);
        pckWriteU32P(resultMessage, protocolMessageTypeData, .defaultWrite = true);
        pckWritePackP(resultMessage, pckWriteResult(data));
        pckWriteEndP(resultMessage);

        // Flush on NULL result since it might be used to synchronize
        if (data == NULL)
            ioWriteFlush(this->write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerDataEndPut(ProtocolServer *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, this);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write the response and flush to be sure it gets sent immediately
        PackWrite *response = pckWriteNewIo(this->write);
        pckWriteU32P(response, protocolMessageTypeDataEnd, .defaultWrite = true);
        pckWriteEndP(response);
    }
    MEM_CONTEXT_TEMP_END();

    ioWriteFlush(this->write);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolServerToLog(const ProtocolServer *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{name: %s}", strZ(this->name));
}
