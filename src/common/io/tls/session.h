/***********************************************************************************************************************************
TLS Session

!!!
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_SESSION_H
#define COMMON_IO_TLS_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define TLS_SESSION_TYPE                                            TlsSession
#define TLS_SESSION_PREFIX                                          tlsSession

typedef struct TlsSession TlsSession;

#include <openssl/ssl.h>

#include "common/io/read.h"
#include "common/io/socket/session.h"
#include "common/io/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
TlsSession *tlsSessionNew(SSL *session, SocketSession *socketSession, TimeMSec timeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
TlsSession *tlsSessionMove(TlsSession *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *tlsSessionIoRead(TlsSession *this);

// Write interface
IoWrite *tlsSessionIoWrite(TlsSession *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void tlsSessionFree(TlsSession *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TLS_SESSION_TYPE                                                                                              \
    TlsSession *
#define FUNCTION_LOG_TLS_SESSION_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "TlsSession", buffer, bufferSize)

#endif
