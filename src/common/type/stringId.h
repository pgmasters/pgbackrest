/***********************************************************************************************************************************
Represent Short Strings as Integers

It is often useful to represent indentifiers as strings when they cannot easily be represented as an enum/integer, e.g. because they
are distributed among a number of unrelated modules or need to be passed to remote processes. Strings are also more helpful in
debugging since they can be recognized without cross-referencing the source. However, strings are awkward to work with in code since
they cannot be directly used in switch statments leading to less efficient if-else structures.

A StringId encodes a short string into an integer so it can be used in switch statements but may also be readily converted back into
a string for debugging purposes. The provided STR*ID*() macros allow a StringId to be defined as characters to make it easier to
ensure that a StringId is unique within a certain namespace, e.g. all storage drivers.

!!! Functions are also provided for converting back and forth to be used for debugging and runtime generation of StringIds.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGID_H
#define COMMON_TYPE_STRINGID_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Maximum number of characters in a StringId. This is a safe buffer size when calling strIdToZN. If the buffer needs to be
zero-terminated then an extra byte will be needed.
***********************************************************************************************************************************/
#define STRING_ID_MAX                                               13

/***********************************************************************************************************************************
StringId typedef to make them more recognizable in the code
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
Number of bits to use for encoding. The number of bits affects the character set that can be used.
***********************************************************************************************************************************/
typedef enum
{
    stringIdBit5 = 0,                                               // 5-bit
    stringIdBit6 = 1,                                               // 6-bit
} StringIdBit;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert N chars to StringId, If the string is longer than the allowable length for the selected encoding then the StringID will
// be marked as "partial" and will have a '+' appended whenever it is converted back to a string.
StringId strIdFromZN(const StringIdBit bit, const char *const buffer, const size_t size);

// Convert String to StringId using strIdFromZN()
__attribute__((always_inline)) static inline StringId
strIdFromStr(const StringIdBit bit, const String *const str)
{
    return strIdFromZN(bit, strZ(str), strSize(str));
}

// Convert zero-terminted string to StringId using strIdFromZN()
__attribute__((always_inline)) static inline StringId
strIdFromZ(const StringIdBit bit, const char *const str)
{
    return strIdFromZN(bit, str, strlen(str));
}

// Write StringId to characters without zero-terminating. The buffer at ptr must have enough space to write the entire StringId,
// which could be eight characters. However, the caller may know the exact (or max length) in advance and act accordingly. The
// actual number of bytes written is returned.
size_t strIdToZN(StringId strId, char *const buffer);

// Convert StringId to String
String *strIdToStr(const StringId strId);

// Convert StringId to zero-terminated string. See strIdToZN() for buffer sizing and return value.
size_t strIdToZ(const StringId strId, char *const buffer);

/***********************************************************************************************************************************
Debug functions and macros
***********************************************************************************************************************************/
#ifdef DEBUG
    void strIdGenerate(const char *const buffer) __attribute__((__noreturn__));
#endif

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
#define STRID5(str, strId)                                          strId
#define STRID6(str, strId)                                          strId

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize);

#define FUNCTION_LOG_STRING_ID_TYPE                                                                                                \
    StringId
#define FUNCTION_LOG_STRING_ID_FORMAT(value, buffer, bufferSize)                                                                   \
    strIdToLog(value, buffer, bufferSize)

#endif
