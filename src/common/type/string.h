/***********************************************************************************************************************************
String Handler

Strings are lightweight objects in that they do not have their own memory context, instead they exist in the current context in
which they are instantiated. If a string is needed outside the current memory context, the memory context must be switched to the
old context and then back. Below is a simplified example:

    String *result = NULL;     <--- is created in the current memory context  (referred to as "old context" below)
    MEM_CONTEXT_TEMP_BEGIN()   <--- begins a new temporary context
    {
        String *resultStr = strNewN("myNewStr"); <--- creates a string in the temporary memory context

        MEM_CONTEXT_PRIOR_BEGIN() <--- switch to the old context so the duplication of the string is in that context
        {
            result = strDup(resultStr); <--- recreates a copy of the string in the old context where "result" was created
        }
        MEM_CONTEXT_PRIOR_END(); <--- switch back to the temporary context
    }
    MEM_CONTEXT_TEMP_END(); <-- frees everything created inside this temporary memory context - i.e resultStr
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRING_H
#define COMMON_TYPE_STRING_H

#include <stdint.h>

/***********************************************************************************************************************************
Minimum number of extra bytes to allocate for strings that are growing or are likely to grow
***********************************************************************************************************************************/
#ifndef STRING_EXTRA_MIN
    #define STRING_EXTRA_MIN                                            64
#endif

/***********************************************************************************************************************************
String object
***********************************************************************************************************************************/
typedef struct String String;

#include "common/assert.h"
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Fields that are common between dynamically allocated and constant strings

There is nothing user-accessible here but this construct allows constant strings to be created and then handled by the same
functions that process dynamically allocated strings.
***********************************************************************************************************************************/
#define STRING_COMMON                                                                                                              \
    uint64_t size:32;                                               /* Actual size of the string */                                \
    uint64_t extra:32;                                              /* Extra space allocated for expansion */                      \
    char *buffer;                                                   /* String buffer */

typedef struct StringConst
{
    STRING_COMMON
} StringConst;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create a new string from a zero-terminated string
String *strNew(const char *string);

// Create a new string from a buffer. If the buffer has a NULL character this may not work as expected. All the data will be copied
// but only the data before the NULL character will be used as a string.
String *strNewBuf(const Buffer *buffer);

// Create a new string from a format string with parameters (i.e. sprintf)
String *strNewFmt(const char *format, ...) __attribute__((format(printf, 1, 2)));

// Create a new string from a string with a specific length. The string may or may not be zero-terminated but we'll use that
// nomenclature since we're not concerned about the end of the string.
String *strNewN(const char *string, size_t size);

// Duplicate a string
String *strDup(const String *this);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Return the file part of a string (i.e. everything after the last / or the entire string if there is no /)
String *strBase(const String *this);

// Does the string begin with the specified string?
bool strBeginsWith(const String *this, const String *beginsWith);
bool strBeginsWithZ(const String *this, const char *beginsWith);

// Append a string or zero-terminated string
String *strCat(String *this, const String *cat);
String *strCatZ(String *this, const char *cat);

// Append a character
String *strCatChr(String *this, char cat);

// Append a formatted string
String *strCatFmt(String *this, const char *format, ...) __attribute__((format(printf, 2, 3)));

// Append N characters from a zero-terminated string. Note that the string does not actually need to be zero-terminated as long as
// N is <= the end of the string being concatenated.
String *strCatZN(String *this, const char *cat, size_t size);

// Return the index to the location of the the first occurrence of a character within a string, else -1
int strChr(const String *this, char chr);

// C-style string compare
int strCmp(const String *this, const String *compare);
int strCmpZ(const String *this, const char *compare);

// Is the string empty?
bool strEmpty(const String *this);

// Does the string end with the specified string?
bool strEndsWith(const String *this, const String *endsWith);
bool strEndsWithZ(const String *this, const char *endsWith);

// Are two strings equal?
bool strEq(const String *this, const String *compare);
bool strEqZ(const String *this, const char *compare);

// Upper-case the first letter
String *strFirstUpper(String *this);

// Lower-case the first letter
String *strFirstLower(String *this);

// Upper-case entire string
String *strUpper(String *this);

// Upper-case entire string
String *strLower(String *this);

// Return the path part of a string (i.e. everything before the last / or "" if there is no /)
String *strPath(const String *this);

// Combine with a base path to get an absolute path
String *strPathAbsolute(const String *this, const String *base);

// Pointer to zero-terminated string. strPtrNull() returns NULL when the String is NULL.
__attribute__((always_inline)) static inline const char *
strPtr(const String *this)
{
    // Avoid uncovered branch during coverage testing
#ifndef DEBUG_COVERAGE
    ASSERT(this != NULL);
#endif

    return ((const StringConst *)this)->buffer;
}

const char *strPtrNull(const String *this);

// Quote a string
String *strQuote(const String *this, const String *quote);
String *strQuoteZ(const String *this, const char *quote);

// Replace a character with another character
String *strReplaceChr(String *this, char find, char replace);

// String size minus null-terminator, i.e. the same value that strlen() would return
size_t strSize(const String *this);

// Format sizes (file, buffer, etc.) in human-readable form
String *strSizeFormat(const uint64_t fileSize);

// Return a substring given only the start position
String *strSub(const String *this, size_t start);

// Return a substring given the start position and size
String *strSubN(const String *this, size_t start, size_t size);

// Trim whitespace from the beginning and end of a string
String *strTrim(String *this);

// Truncate the end of a string from the index provided to the current end (e.g. 123KB pass index of K returns 123)
String *strTrunc(String *this, int idx);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void strFree(String *this);

/***********************************************************************************************************************************
Macros for constant strings

Frequently used constant strings can be declared with these macros at compile time rather than dynamically at run time.

Note that strings created in this way are declared as const so can't be modified or freed by the str*() methods.  Casting to
String * will result in a segfault due to modifying read-only memory.

By convention all string constant identifiers are appended with _STR.
***********************************************************************************************************************************/
// Create a String constant inline from any zero-terminated string
#define STR(bufferParam)                                                                                                           \
    ((const String *)&(const StringConst){.buffer = (char *)(bufferParam), .size = (unsigned int)strlen(bufferParam)})

// Create a String constant inline from a #define or inline string constant
#define STRDEF(bufferParam)                                                                                                        \
    ((const String *)&(const StringConst){.buffer = (char *)(bufferParam), .size = (unsigned int)sizeof(bufferParam) - 1})

// Used to declare String constants that will be externed using STRING_DECLARE().  Must be used in a .c file.
#define STRING_EXTERN(name, buffer)                                                                                                \
    const String *const name = STRDEF(buffer)

// Used to declare String constants that will be local to the .c file.  Must be used in a .c file.
#define STRING_STATIC(name, buffer)                                                                                                \
    static const String *const name = STRDEF(buffer)

// Used to extern String constants declared with STRING_EXTERN().  Must be used in a .h file.
#define STRING_DECLARE(name)                                                                                                       \
    extern const String *const name

/***********************************************************************************************************************************
Constant strings that are generally useful
***********************************************************************************************************************************/
STRING_DECLARE(BRACKETL_STR);
STRING_DECLARE(BRACKETR_STR);
STRING_DECLARE(COLON_STR);
STRING_DECLARE(CR_STR);
STRING_DECLARE(DASH_STR);
STRING_DECLARE(DOT_STR);
STRING_DECLARE(DOTDOT_STR);
STRING_DECLARE(EMPTY_STR);
STRING_DECLARE(EQ_STR);
STRING_DECLARE(FALSE_STR);
STRING_DECLARE(FSLASH_STR);
STRING_DECLARE(LF_STR);
STRING_DECLARE(N_STR);
STRING_DECLARE(NULL_STR);
STRING_DECLARE(QUOTED_STR);
STRING_DECLARE(TRUE_STR);
STRING_DECLARE(Y_STR);
STRING_DECLARE(ZERO_STR);

/***********************************************************************************************************************************
Helper function/macro for object logging
***********************************************************************************************************************************/
typedef String *(*StrObjToLogFormat)(const void *object);

size_t strObjToLog(const void *object, StrObjToLogFormat formatFunc, char *buffer, size_t bufferSize);

#define FUNCTION_LOG_STRING_OBJECT_FORMAT(object, formatFunc, buffer, bufferSize)                                                  \
    strObjToLog(object, (StrObjToLogFormat)formatFunc, buffer, bufferSize)

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *strToLog(const String *this);

#define FUNCTION_LOG_STRING_TYPE                                                                                                   \
    String *
#define FUNCTION_LOG_STRING_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, strToLog, buffer, bufferSize)

#endif
