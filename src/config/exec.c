/***********************************************************************************************************************************
Exec Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "config/exec.h"

/***********************************************************************************************************************************
Generate a list of options required for execution of a new command, replacing options as specified in optionReplace
***********************************************************************************************************************************/
StringList *
cfgExecParam(ConfigCommand commandId, ConfigCommandRole commandRoleId, const KeyValue *optionReplace, bool local)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, commandId);
        FUNCTION_LOG_PARAM(ENUM, commandRoleId);
        FUNCTION_LOG_PARAM(KEY_VALUE, optionReplace);
        FUNCTION_LOG_PARAM(BOOL, local);                            // Will the new process be running on the same host?
    FUNCTION_LOG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the command role
        const String *commandRole = cfgCommandRoleStr(commandRoleId);

        // Get the actual command id.  This is a bit confusing -- the command that is passed in can be transformed into a new
        // command once the role has been added.  For example, the 'archive-push' command plus the 'async' role yields the
        // 'archive-push:async' command.  The confusing part is that the 'backup' command plus the 'local' role yields the 'local'
        // command.  This is something we'd like to improved internally, but for now it seems more important to get
        // the interface correct since this is what the user sees (even if only in a ps list).
        const String *commandActual = strNewFmt(
            "%s%s", cfgCommandName(commandId), commandRole == NULL ? "" : strPtr(strNewFmt(":%s", strPtr(commandRole))));

        ConfigCommand commandIdActual = cfgCommandId(strPtr(commandActual), false);

        if (commandIdActual == cfgCmdNone)
            commandIdActual = cfgCommandId(strPtr(commandRole), false);

        CHECK(commandIdActual != cfgCmdNone);

        ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(commandIdActual);

        // Loop though options and add the ones that apply to the actual command
        result = strLstNew();

        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

            // Skip the option if it is not valid for the actual command or if is secure.  Also skip repo1-cipher-type because
            // there's no point of passing it if the other process doesn't have access to repo1-cipher-pass.  There is probably a
            // better way to do this...
            if (!cfgDefOptionValid(commandDefId, optionDefId) || cfgDefOptionSecure(optionDefId) ||
                optionDefId == cfgDefOptRepoCipherType)
            {
                continue;
            }

            // First check for a replacement
            const Variant *key = VARSTRZ(cfgOptionName(optionId));
            const Variant *value = NULL;
            bool exists = false;

            if (optionReplace != NULL)
            {
                exists = kvKeyExists(optionReplace, key);

                if (exists)
                    value = kvGet(optionReplace, key);
            }

            // If the key exists but is NULL then skip this option
            if (exists && value == NULL)
                continue;

            // If no replacement then see if this option is valid for the current command and is not default
            if (value == NULL && cfgOptionValid(optionId))
            {
                if (cfgOptionNegate(optionId))
                    value = BOOL_FALSE_VAR;
                else if (cfgOptionSource(optionId) != cfgSourceDefault)
                    value = cfgOption(optionId);
            }

            // If the option was reset
            if (cfgOptionReset(optionId))
            {
                strLstAdd(result, strNewFmt("--reset-%s", cfgOptionName(optionId)));
            }
            // Else format the value if found
            else if (value != NULL && (!local || exists || cfgOptionSource(optionId) == cfgSourceParam))
            {
                if (varType(value) == varTypeBool)
                {
                    strLstAdd(result, strNewFmt("--%s%s", varBool(value) ? "" : "no-", cfgOptionName(optionId)));
                }
                else
                {
                    StringList *valueList = NULL;

                    if (varType(value) == varTypeKeyValue)
                    {
                        valueList = strLstNew();

                        const KeyValue *optionKv = varKv(value);
                        const VariantList *keyList = kvKeyList(optionKv);

                        for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
                        {
                            strLstAdd(
                                valueList,
                                strNewFmt(
                                    "%s=%s", strPtr(varStr(varLstGet(keyList, keyIdx))),
                                        strPtr(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx))))));
                        }
                    }
                    else if (varType(value) == varTypeVariantList)
                    {
                        valueList = strLstNewVarLst(varVarLst(value));
                    }
                    // Else only one value
                    else
                    {
                        valueList = strLstNew();
                        strLstAdd(valueList, varStrForce(value));
                    }

                    // Output options and values
                    for (unsigned int valueListIdx = 0; valueListIdx < strLstSize(valueList); valueListIdx++)
                    {
                        const String *value = strLstGet(valueList, valueListIdx);

                        if (strchr(strPtr(value), ' ') != NULL)
                            value = strNewFmt("\"%s\"", strPtr(value));

                        strLstAdd(result, strNewFmt("--%s=%s", cfgOptionName(optionId), strPtr(value)));
                    }
                }
            }
        }

        // Add the command
        strLstAdd(result, commandActual);

        // Move list to the calling context
        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}
