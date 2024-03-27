/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Message types used by the protocol
***********************************************************************************************************************************/
typedef enum
{
    // Data passed from client to server. This can be used as many times as needed.
    protocolMessageTypeData = 0,

    // Command sent from the client to the server
    protocolMessageTypeCommand = 1,

    // An error occurred on the server and the command ended abnormally. protocolMessageTypeData will not be sent to the client.
    protocolMessageTypeError = 2,
} ProtocolMessageType;

/***********************************************************************************************************************************
Command types
***********************************************************************************************************************************/
typedef enum
{
    protocolCommandTypeOpen = STRID5("opn", 0x3a0f0),               // Open command for processing
    protocolCommandTypeProcess = STRID5("prc", 0xe500),             // Process command
    protocolCommandTypeClose = STRID5("cls", 0x4d830),              // Close command
    protocolCommandTypeCancel = STRID5("cnc", 0xdc30),              // Cancel command
} ProtocolCommandType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolClient ProtocolClient;
typedef struct ProtocolClientSession ProtocolClientSession;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_GREETING_NAME                                      STRID5("name", 0x2b42e0)
#define PROTOCOL_GREETING_SERVICE                                   STRID5("service", 0x1469b48b30)
#define PROTOCOL_GREETING_VERSION                                   STRID5("version", 0x39e99c8b60)

#define PROTOCOL_COMMAND_CONFIG                                     STRID5("config", 0xe9339e30)
#define PROTOCOL_COMMAND_EXIT                                       STRID5("exit", 0xa27050)
#define PROTOCOL_COMMAND_NOOP                                       STRID5("noop", 0x83dee0)

/***********************************************************************************************************************************
This size should be safe for most pack data without wasting a lot of space. If binary data is being transferred then this size can
be added to the expected binary size to account for overhead.
***********************************************************************************************************************************/
#define PROTOCOL_PACK_DEFAULT_SIZE                                  1024

// Pack large enough for standard data. Note that the buffer will automatically resize when required.
FN_INLINE_ALWAYS PackWrite *
protocolPackNew(void)
{
    return pckWriteNewP(.size = PROTOCOL_PACK_DEFAULT_SIZE);
}

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);

// New session
typedef struct ProtocolClientSessionNewParam
{
    VAR_PARAM_HEADER;
    bool async;                                                     // Async requests allowed?
} ProtocolClientSessionNewParam;

#define protocolClientSessionNewP(client, command, ...)                                                                            \
    protocolClientSessionNew(client, command, (ProtocolClientSessionNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN ProtocolClientSession *protocolClientSessionNew(
    ProtocolClient *client, StringId command, ProtocolClientSessionNewParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ProtocolClientPub
{
    IoRead *read;                                                   // Read interface
} ProtocolClientPub;

typedef struct ProtocolClientSessionPub
{
    bool open;                                                      // Is the session open?
    bool queued;                                                    // Is a response currently queued?
} ProtocolClientSessionPub;

// Read file descriptor
FN_INLINE_ALWAYS int
protocolClientIoReadFd(ProtocolClient *const this)
{
    return ioReadFd(THIS_PUB(ProtocolClient)->read);
}

// Is a response currently queued?
FN_INLINE_ALWAYS bool
protocolClientSessionQueued(const ProtocolClientSession *const this)
{
    return THIS_PUB(ProtocolClientSession)->queued;
}

// Is the session closed?
FN_INLINE_ALWAYS bool
protocolClientSessionClosed(const ProtocolClientSession *const this)
{
    return !THIS_PUB(ProtocolClientSession)->open;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS ProtocolClient *
protocolClientMove(ProtocolClient *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Do not send exit command to the server when the client is freed
FN_INLINE_ALWAYS void
protocolClientNoExit(ProtocolClient *const this)
{
    memContextCallbackClear(objMemContext(this));
}

// Send noop to test connection or keep it alive
FN_EXTERN void protocolClientNoOp(ProtocolClient *this);

// Session open
typedef struct ProtocolClientSessionOpenParam
{
    VAR_PARAM_HEADER;
    PackWrite *param;
} ProtocolClientSessionOpenParam;

#define protocolClientSessionOpenP(this, ...)                                                                                      \
    protocolClientSessionOpen(this, (ProtocolClientSessionOpenParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *protocolClientSessionOpen(ProtocolClientSession *const this, ProtocolClientSessionOpenParam param);

// Session request
typedef struct ProtocolClientSessionRequestParam
{
    VAR_PARAM_HEADER;
    PackWrite *param;
} ProtocolClientSessionRequestParam;

#define protocolClientSessionRequestP(this, ...)                                                                                   \
    protocolClientSessionRequest(this, (ProtocolClientSessionRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *protocolClientSessionRequest(ProtocolClientSession *const this, ProtocolClientSessionRequestParam param);

// Session request async
#define protocolClientSessionRequestAsyncP(this, ...)                                                                              \
    protocolClientSessionRequestAsync(this, (ProtocolClientSessionRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void protocolClientSessionRequestAsync(ProtocolClientSession *const this, ProtocolClientSessionRequestParam param);

// Session response after a call to protocolClientSessionRequestAsyncP()
FN_EXTERN PackRead *protocolClientSessionResponse(ProtocolClientSession *const this);

// Session close
FN_EXTERN PackRead *protocolClientSessionClose(ProtocolClientSession *const this);

// Session cancel
FN_EXTERN void protocolClientSessionCancel(ProtocolClientSession *const this);

// Client request
#define protocolClientRequestP(this, command, ...)                                                                                          \
    protocolClientRequest(this, command, (ProtocolClientSessionRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *protocolClientRequest(ProtocolClient *this, StringId command, ProtocolClientSessionRequestParam param);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolClientFree(ProtocolClient *const this)
{
    objFree(this);
}

FN_INLINE_ALWAYS void
protocolClientSessionFree(ProtocolClientSession *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void protocolClientToLog(const ProtocolClient *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_CLIENT_TYPE                                                                                          \
    ProtocolClient *
#define FUNCTION_LOG_PROTOCOL_CLIENT_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolClientToLog, buffer, bufferSize)

FN_EXTERN void protocolClientSessionToLog(const ProtocolClientSession *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_CLIENT_SESSION_TYPE                                                                                  \
    ProtocolClientSession *
#define FUNCTION_LOG_PROTOCOL_CLIENT_SESSION_FORMAT(value, buffer, bufferSize)                                                     \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolClientSessionToLog, buffer, bufferSize)

#endif
