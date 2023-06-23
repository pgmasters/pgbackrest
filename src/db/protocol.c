/***********************************************************************************************************************************
Db Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "config/config.h"
#include "db/protocol.h"
#include "postgres/client.h"
#include "postgres/interface.h"

/**********************************************************************************************************************************/
FN_EXTERN void *
dbOpenProtocol(PackRead *const param, ProtocolServer *const server, const uint64_t sessionId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(UINT64, sessionId);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);
    ASSERT(sessionId != 0);

    PgClient *const result = pgClientNew(
        cfgOptionStrNull(cfgOptPgSocketPath), cfgOptionUInt(cfgOptPgPort), cfgOptionStr(cfgOptPgDatabase),
        cfgOptionStrNull(cfgOptPgUser), cfgOptionUInt64(cfgOptDbTimeout));
    pgClientOpen(result);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Return session id which should be included in subsequent calls
        protocolServerDataPut(server, pckWriteU64P(protocolPackNew(), sessionId));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
dbQueryProtocol(PackRead *const param, ProtocolServer *const server, void *const pgClient)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(PG_CLIENT, pgClient);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(pgClient != NULL);

    const PgClientQueryResult resultType = (PgClientQueryResult)pckReadStrIdP(param);
    const String *const query = pckReadStrP(param);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerDataPut(server, pckWritePackP(protocolPackNew(), pgClientQuery(pgClient, query, resultType)));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
dbCloseProtocol(PackRead *const param, ProtocolServer *const server, void *const pgClient)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(PG_CLIENT, pgClient);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);
    ASSERT(pgClient != NULL);

    pgClientClose(pgClient);
    protocolServerDataEndPut(server);

    FUNCTION_LOG_RETURN_VOID();
}
