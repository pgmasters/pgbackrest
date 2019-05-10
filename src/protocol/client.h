/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PROTOCOL_CLIENT_TYPE                                        ProtocolClient
#define PROTOCOL_CLIENT_PREFIX                                      protocolClient

typedef struct ProtocolClient ProtocolClient;

#include "common/io/read.h"
#include "common/io/write.h"
#include "protocol/command.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_GREETING_NAME                                      "name"
    STRING_DECLARE(PROTOCOL_GREETING_NAME_STR);
#define PROTOCOL_GREETING_SERVICE                                   "service"
    STRING_DECLARE(PROTOCOL_GREETING_SERVICE_STR);
#define PROTOCOL_GREETING_VERSION                                   "version"
    STRING_DECLARE(PROTOCOL_GREETING_VERSION_STR);

#define PROTOCOL_COMMAND_EXIT                                       "exit"
    STRING_DECLARE(PROTOCOL_COMMAND_EXIT_STR);
#define PROTOCOL_COMMAND_NOOP                                       "noop"
    STRING_DECLARE(PROTOCOL_COMMAND_NOOP_STR);

#define PROTOCOL_ERROR                                              "err"
    STRING_DECLARE(PROTOCOL_ERROR_STR);
#define PROTOCOL_ERROR_STACK                                        "errStack"
    STRING_DECLARE(PROTOCOL_ERROR_STACK_STR);

#define PROTOCOL_OUTPUT                                             "out"
    STRING_DECLARE(PROTOCOL_OUTPUT_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
const Variant *protocolClientExecute(ProtocolClient *this, const ProtocolCommand *command, bool outputRequired);
ProtocolClient *protocolClientMove(ProtocolClient *this, MemContext *parentNew);
void protocolClientNoOp(ProtocolClient *this);
const Variant *protocolClientReadOutput(ProtocolClient *this, bool outputRequired);
void protocolClientWriteCommand(ProtocolClient *this, const ProtocolCommand *command);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
IoRead *protocolClientIoRead(const ProtocolClient *this);
IoWrite *protocolClientIoWrite(const ProtocolClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolClientFree(ProtocolClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolClientToLog(const ProtocolClient *this);

#define FUNCTION_LOG_PROTOCOL_CLIENT_TYPE                                                                                          \
    ProtocolClient *
#define FUNCTION_LOG_PROTOCOL_CLIENT_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolClientToLog, buffer, bufferSize)

#endif
