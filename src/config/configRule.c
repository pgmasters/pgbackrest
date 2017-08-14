/***********************************************************************************************************************************
Command and Option Rules
***********************************************************************************************************************************/
#include "config.h"
#include "configRule.h"

#include "configRule.auto.c"

/***********************************************************************************************************************************
cfgCommandTotal - total number of commands
***********************************************************************************************************************************/
uint32
cfgCommandTotal()
{
    return CONFIG_COMMAND_TOTAL;
}

/***********************************************************************************************************************************
cfgOptionTotal - total number of configuration options
***********************************************************************************************************************************/
uint32
cfgOptionTotal()
{
    return CONFIG_OPTION_TOTAL;
}

/***********************************************************************************************************************************
cfgOptionRuleAllowListValueValid - check if the value matches a value in the allow list
***********************************************************************************************************************************/
bool
cfgOptionRuleAllowListValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue)
{
    if (szValue != NULL)
    {
        for (uint32 uiIndex = 0; uiIndex < cfgOptionRuleAllowListValueTotal(uiCommandId, uiOptionId); uiIndex++)
            if (strcmp(szValue, cfgOptionRuleAllowListValue(uiCommandId, uiOptionId, uiIndex)) == 0)
                return true;
    }

    return false;
}

/***********************************************************************************************************************************
cfgOptionRuleDependValueValid - check if the value matches a value in the allow list
***********************************************************************************************************************************/
bool
cfgOptionRuleDependValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue)
{
    if (szValue != NULL)
    {
        for (uint32 uiIndex = 0; uiIndex < cfgOptionRuleDependValueTotal(uiCommandId, uiOptionId); uiIndex++)
            if (strcmp(szValue, cfgOptionRuleDependValue(uiCommandId, uiOptionId, uiIndex)) == 0)
                return true;
    }

    return false;
}
