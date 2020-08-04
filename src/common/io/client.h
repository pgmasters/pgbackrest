/***********************************************************************************************************************************
Io Client Interface

!!!
***********************************************************************************************************************************/
#ifndef COMMON_IO_CLIENT_H
#define COMMON_IO_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define IO_CLIENT_TYPE                                             IoClient
#define IO_CLIENT_PREFIX                                           ioClient

typedef struct IoClient IoClient;

#include "common/io/session.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open session
IoSession *ioClientOpen(IoClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void ioClientFree(IoClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_CLIENT_TYPE                                                                                                \
    IoClient *
#define FUNCTION_LOG_IO_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "IoClient", buffer, bufferSize)

#endif
