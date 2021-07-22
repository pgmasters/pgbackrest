/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/wait.h>

#include "command/server/server.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/io/tls/server.h"
#include "common/io/socket/server.h"
#include "config/config.h"
#include "config/load.h"
#include "config/protocol.h"
#include "db/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "storage/remote/protocol.h"

/***********************************************************************************************************************************
Command handlers
***********************************************************************************************************************************/
static const ProtocolServerHandler commandRemoteHandlerList[] =
{
    PROTOCOL_SERVER_HANDLER_DB_LIST
    PROTOCOL_SERVER_HANDLER_OPTION_LIST
    PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST
};

/**********************************************************************************************************************************/
static bool
cmdServerFork(IoServer *const tlsServer, IoSession *const socketSession, const String *const host)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_SERVER, tlsServer);
        FUNCTION_LOG_PARAM(IO_SESSION, socketSession);
        FUNCTION_LOG_PARAM(STRING, host);
    FUNCTION_LOG_END();

    // Fork off the async process
    pid_t pid = forkSafe();

    if (pid == 0)
    {
        // Start protocol handshake on the bare socket
        ProtocolServer *const socketServer = protocolServerNew(
            PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(socketSession),
            ioSessionIoWrite(socketSession));

        // Get the command and put data end. No need to check parameters since we know this is the first noop.
        CHECK(protocolServerCommandGet(socketServer).id == PROTOCOL_COMMAND_NOOP);
        protocolServerDataEndPut(socketServer);

        // Check if TLS negotiation was requested
        bool tls = false;

        ProtocolServerCommandGetResult command = protocolServerCommandGet(socketServer);

        if (command.id == PROTOCOL_COMMAND_TLS)
        {
            // Negotiate TLS
            tls = true;

            // Acknowledge TLS request
            protocolServerDataEndPut(socketServer);

            // Get next command, which should be exit
            command = protocolServerCommandGet(socketServer);
        }

        // Get exit commmand. Do not send a response.
        CHECK(command.id == PROTOCOL_COMMAND_EXIT);

        // Negotiate TLS if requested
        if (tls)
        {
            IoSession *const tlsSession = ioServerAccept(tlsServer, socketSession);

            ProtocolServer *const tlsServer = protocolServerNew(
                PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(tlsSession),
                ioSessionIoWrite(tlsSession));

            // Get the command and put data end. No need to check parameters since we know this is the first noop.
            CHECK(protocolServerCommandGet(tlsServer).id == PROTOCOL_COMMAND_NOOP);
            protocolServerDataEndPut(tlsServer);

            // Get parameter list from the client and load it
            command = protocolServerCommandGet(tlsServer);
            CHECK(command.id == PROTOCOL_COMMAND_CONFIG);

            StringList *const paramList = pckReadStrLstP(pckReadNewBuf(command.param));
            strLstInsert(paramList, 0, cfgExe());
            cfgLoad(strLstSize(paramList), strLstPtr(paramList));

            protocolServerDataEndPut(tlsServer);

            // !!! NEED TO SET READ TIMEOUT TO PROTOCOL-TIMEOUT HERE

            // Detach from parent process
            forkDetach();

            // !!! NEED TO CALL cmdRemote() DIRECTLY, WHICH WILL REQUIRE SOME REFACTORING
            protocolServerProcess(
                tlsServer, NULL, commandRemoteHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandRemoteHandlerList));

            ioSessionFree(tlsSession);
        }
        // Else exit
        else
            ioSessionFree(socketSession);
    }
    else
    {
        // The process that was just forked should return immediately
        int processStatus;

        THROW_ON_SYS_ERROR(waitpid(pid, &processStatus, 0) == -1, ExecuteError, "unable to wait for forked process");

        // The first fork should exit with success. If not, something went wrong during the second fork.
        CHECK(WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0);
    }

    FUNCTION_LOG_RETURN(BOOL, pid != 0);
}

void
cmdServer(uint64_t connectionMax)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(connectionMax > 0);

    const String *const host = STRDEF("localhost");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoServer *const tlsServer = tlsServerNew(
           host, cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert), cfgOptionUInt64(cfgOptIoTimeout));
        IoServer *const socketServer = sckServerNew(host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptIoTimeout));

        // Accept connections until connection max is reached. !!! THIS IS A HACK TO LIMIT THE LOOP AND ALLOW TESTING. IT SHOULD BE
        // REPLACED WITH A STOP REQUEST FROM AN AUTHENTICATED CLIENT.
        do
        {
            // Accept a new connection
            IoSession *const socketSession = ioServerAccept(socketServer, NULL);

            // Fork the child and break out of the loop when the child returns
            if (!cmdServerFork(tlsServer, socketSession, host))
                break;

            // Free the socket since the child is now using it
            ioSessionFree(socketSession);
        }
        while (--connectionMax > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
