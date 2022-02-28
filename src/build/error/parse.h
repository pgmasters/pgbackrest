/***********************************************************************************************************************************
Parse Error Yaml
***********************************************************************************************************************************/
#ifndef BUILD_ERROR_PARSE_H
#define BUILD_ERROR_PARSE_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldErrError
{
    const String *const name;                                       // Name
    const unsigned int code;                                        // Code
    const bool fatal;                                               // Is the error fatal?
} BldErrError;

typedef struct BldErr
{
    const List *const errList;                                      // Command list
} BldErr;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse error.yaml
BldErr bldErrParse(const Storage *const storageRepo);

#endif
