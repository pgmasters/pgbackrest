/***********************************************************************************************************************************
Configuration Common
***********************************************************************************************************************************/
#ifndef CONFIG_COMMON_H
#define CONFIG_COMMON_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse option size, e.g. 23m
int64_t cfgParseSize(const String *value);

// Parse option time, e.g. 900
int64_t cfgParseTime(const String *value);

#endif
