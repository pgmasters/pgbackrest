/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/type/convert.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/encode.h"
#include "common/io/filter/filter.intern.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/json.h"
#include "info/info.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/
STRING_STATIC(INFO_SECTION_BACKREST_STR,                            "backrest");
STRING_STATIC(INFO_SECTION_CIPHER_STR,                              "cipher");

STRING_STATIC(INFO_KEY_CIPHER_PASS_STR,                             "cipher-pass");
STRING_STATIC(INFO_KEY_CHECKSUM_STR,                                "backrest-checksum");
STRING_EXTERN(INFO_KEY_FORMAT_STR,                                  INFO_KEY_FORMAT);
STRING_EXTERN(INFO_KEY_VERSION_STR,                                 INFO_KEY_VERSION);

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Mem context
    const String *cipherPass;                                       // Cipher passphrase if set
};

OBJECT_DEFINE_FREE(INFO);

struct InfoSave
{
    MemContext *memContext;                                         // Mem context
    IoWrite *infoWrite;                                             // Write object for main copy
    IoFilter *checksum;                                             // hash to generate file checksum
    String *sectionLast;                                            // The last section seen
};

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static Info *
infoNewInternal(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    Info *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Info")
    {
        // Create object
        this = memNew(sizeof(Info));
        this->memContext = MEM_CONTEXT_NEW();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
Info *
infoNew(CipherType cipherType, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);                 // Use FUNCTION_TEST so cipher is not logged
    FUNCTION_LOG_END();

    // Ensure cipherPassSub is set and not an empty string when cipherType is not NONE and is not set when cipherType is NONE
    ASSERT(
        !((cipherType == cipherTypeNone && cipherPassSub != NULL) ||
          (cipherType != cipherTypeNone && (cipherPassSub == NULL || strSize(cipherPassSub) == 0))));

    Info *this = infoNewInternal();

    // Cipher used to encrypt/decrypt subsequent dependent files. Value may be NULL.
    if (cipherPassSub != NULL)
        this->cipherPass = cipherPassSub;

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Load and validate the info file (or copy)
***********************************************************************************************************************************/
typedef struct InfoLoadData
{
    MemContext *memContext;                                         // Mem context to use for storing data in this structure
    void (*callbackFunction)(                                       // Callback function for child object
        InfoCallbackType type, void *data, const String *section, const String *key, const String *value);
    void *callbackData;                                             // Callback data for child object
    const String *fileName;                                         // File being loaded
    String *sectionLast;                                            // The last section seen during load
    IoFilter *checksumActual;                                       // Checksum calculated from the file
    String *checksumExpected;                                       // Checksum found in ini file
    String *cipherPass;                                             // Cipher passphrase (when present)
} InfoLoadData;

static void
infoLoadCallback(void *callbackData, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    InfoLoadData *data = (InfoLoadData *)callbackData;

    // Calculate checksum
    if (!(strEq(section, INFO_SECTION_BACKREST_STR) && strEq(key, INFO_KEY_CHECKSUM_STR)))
    {
        if (data->sectionLast == NULL || !strEq(section, data->sectionLast))
        {
            if (data->sectionLast != NULL)
                ioFilterProcessIn(data->checksumActual, BUFSTRDEF("},"));

            ioFilterProcessIn(data->checksumActual, BUFSTRDEF("\""));
            ioFilterProcessIn(data->checksumActual, BUFSTR(section));
            ioFilterProcessIn(data->checksumActual, BUFSTRDEF("\":{"));

            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->sectionLast = strDup(section);
            }
            MEM_CONTEXT_END();
        }
        else
            ioFilterProcessIn(data->checksumActual, BUFSTRDEF(","));

        ioFilterProcessIn(data->checksumActual, BUFSTRDEF("\""));
        ioFilterProcessIn(data->checksumActual, BUFSTR(key));
        ioFilterProcessIn(data->checksumActual, BUFSTRDEF("\":"));
        ioFilterProcessIn(data->checksumActual, BUFSTR(value));
    }

    // Process backrest section
    if (strEq(section, INFO_SECTION_BACKREST_STR))
    {
        // Validate format
        if (strEq(key, INFO_KEY_FORMAT_STR))
        {
            if (jsonToUInt(value) != REPOSITORY_FORMAT)
            {
                THROW_FMT(
                    FormatError, "invalid format in '%s', expected %d but found %d", strPtr(data->fileName), REPOSITORY_FORMAT,
                    cvtZToInt(strPtr(value)));
            }
        }
        // Store checksum to be validated later
        else if (strEq(key, INFO_KEY_CHECKSUM_STR))
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->checksumExpected = jsonToStr(value);
            }
            MEM_CONTEXT_END();
        }
    }
    // Process cipher section
    else if (strEq(section, INFO_SECTION_CIPHER_STR))
    {
        // No validation needed for cipher-pass, just store it
        if (strEq(key, INFO_KEY_CIPHER_PASS_STR))
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->cipherPass = jsonToStr(value);
            }
            MEM_CONTEXT_END();
        }
    }
    else
        data->callbackFunction(infoCallbackTypeValue, data->callbackData, section, key, value);

    FUNCTION_TEST_RETURN_VOID();
}

static void
infoLoad(
    Info *this, const Storage *storage, const String *fileName, bool copyFile, CipherType cipherType, const String *cipherPass,
    void (*callbackFunction)(InfoCallbackType type, void *data, const String *section, const String *key, const String *value),
    void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(INFO, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);                     // Full path/filename to load
        FUNCTION_LOG_PARAM(BOOL, copyFile);                       // Is this the copy file?
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(fileName != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        InfoLoadData data =
        {
            .memContext = MEM_CONTEXT_TEMP(),
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .fileName = copyFile ? strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName)) : fileName,
            .checksumActual = cryptoHashNew(HASH_TYPE_SHA1_STR),
        };

        // Attempt to load the file
        StorageRead *infoRead = storageNewReadP(storage, data.fileName, .compressible = cipherType == cipherTypeNone);

        if (cipherType != cipherTypeNone)
        {
            ioFilterGroupAdd(
                ioReadFilterGroup(storageReadIo(infoRead)),
                cipherBlockNew(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), NULL));
        }

        // Notify callback function that the ini load is beginning
        data.callbackFunction(infoCallbackTypeBegin, data.callbackData, NULL, NULL, NULL);

        // Load and parse the info file
        ioFilterProcessIn(data.checksumActual, BUFSTRDEF("{"));

        TRY_BEGIN()
        {
            iniLoad(storageReadIo(infoRead), infoLoadCallback, &data);
        }
        CATCH(CryptoError)
        {
            THROW_FMT(
                CryptoError, "'%s' %s\nHINT: Is or was the repo encrypted?", strPtr(storagePathNP(storage, data.fileName)),
                errorMessage());
        }
        TRY_END();

        ioFilterProcessIn(data.checksumActual, BUFSTRDEF("}}"));

        // Notify callback function that the ini load is ending
        data.callbackFunction(infoCallbackTypeEnd, data.callbackData, NULL, NULL, NULL);

        // Verify the checksum
        const String *checksumActual = varStr(ioFilterResult(data.checksumActual));

        if (data.checksumExpected == NULL)
        {
            THROW_FMT(
                ChecksumError, "invalid checksum in '%s', expected '%s' but no checksum found",
                strPtr(storagePathNP(storage, data.fileName)), strPtr(checksumActual));
        }
        else if (!strEq(data.checksumExpected, checksumActual))
        {
                THROW_FMT(
                    ChecksumError, "invalid checksum in '%s', expected '%s' but found '%s'",
                    strPtr(storagePathNP(storage, data.fileName)), strPtr(checksumActual), strPtr(data.checksumExpected));
        }

        // Assign cipher pass if present
        memContextSwitch(MEM_CONTEXT_OLD());
        this->cipherPass = strDup(data.cipherPass);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
Info *
infoNewLoad(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass,
    void (*callbackFunction)(InfoCallbackType type, void *data, const String *section, const String *key, const String *value),
    void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);                     // Full path/filename to load
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);
    ASSERT(callbackFunction != NULL);

    Info *this = infoNewInternal();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Attempt to load the primary file
        TRY_BEGIN()
        {
            infoLoad(this, storage, fileName, false, cipherType, cipherPass, callbackFunction, callbackData);
        }
        CATCH_ANY()
        {
            // On error store the error and try to load the copy
            String *primaryError = strNewFmt("%s: %s", errorTypeName(errorType()), errorMessage());
            bool primaryMissing = errorType() == &FileMissingError;
            const ErrorType *primaryErrorType = errorType();

            // Notify callback of a reset
            callbackFunction(infoCallbackTypeReset, callbackData, NULL, NULL, NULL);

            TRY_BEGIN()
            {
                infoLoad(this, storage, fileName, true, cipherType, cipherPass, callbackFunction, callbackData);
            }
            CATCH_ANY()
            {
                // If both copies of the file have the same error then throw that error,
                // else if one file is missing but the other is in error and it is not missing, throw that error
                // else throw an open error
                THROWP_FMT(
                    errorType() == primaryErrorType ? errorType() :
                        (errorType() == &FileMissingError ? primaryErrorType :
                        (primaryMissing ? errorType() : &FileOpenError)),
                    "unable to load info file '%s' or '%s" INFO_COPY_EXT "':\n%s\n%s: %s",
                    strPtr(storagePathNP(storage, fileName)), strPtr(storagePathNP(storage, fileName)),
                    strPtr(primaryError), errorTypeName(errorType()), errorMessage());
            }
            TRY_END();
        }
        TRY_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Check to see if the section should be saved
***********************************************************************************************************************************/
bool
infoSaveSection(InfoSave *infoSaveData, const String *section, const String *sectionNext)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        (infoSaveData->sectionLast == NULL || strCmp(section, infoSaveData->sectionLast) > 0) &&
            (sectionNext == NULL || strCmp(section, sectionNext) < 0));
}

/***********************************************************************************************************************************
Save a value
***********************************************************************************************************************************/
void
infoSaveValue(InfoSave *infoSaveData, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(infoSaveData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    // Do we calculate the checksum for this value?
    bool checksum = !(strEq(section, INFO_SECTION_BACKREST_STR) && strEq(key, INFO_KEY_CHECKSUM_STR));

    // Calculate checksum
    if (infoSaveData->sectionLast == NULL || !strEq(section, infoSaveData->sectionLast))
    {
        if (infoSaveData->sectionLast != NULL)
        {
            if (checksum)
                ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("},"));

            ioWriteLine(infoSaveData->infoWrite, BUFSTRDEF(""));
        }

        if (checksum)
        {
            ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("\""));
            ioFilterProcessIn(infoSaveData->checksum, BUFSTR(section));
            ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("\":{"));
        }

        ioWrite(infoSaveData->infoWrite, BRACKETL_BUF);
        ioWrite(infoSaveData->infoWrite, BUFSTR(section));
        ioWriteLine(infoSaveData->infoWrite, BRACKETR_BUF);

        MEM_CONTEXT_BEGIN(infoSaveData->memContext)
        {
            infoSaveData->sectionLast = strDup(section);
        }
        MEM_CONTEXT_END();
    }
    else
    {
        // Since the checksum goes in a section by itself we should never be skipping checksums in a section with more than one
        // value.  If this changes in the future then this assertion will break.
        ASSERT(checksum);

        ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF(","));
    }

    if (checksum)
    {
        ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("\""));
        ioFilterProcessIn(infoSaveData->checksum, BUFSTR(key));
        ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("\":"));
        ioFilterProcessIn(infoSaveData->checksum, BUFSTR(value));
    }

    ioWrite(infoSaveData->infoWrite, BUFSTR(key));
    ioWrite(infoSaveData->infoWrite, EQ_BUF);
    ioWriteLine(infoSaveData->infoWrite, BUFSTR(value));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
void
infoSave(
    Info *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass,
    InfoSaveCallback callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);
    ASSERT(
        !((cipherType != cipherTypeNone && this->cipherPass == NULL) ||
          (cipherType == cipherTypeNone && this->cipherPass != NULL)));
    ASSERT(callbackFunction != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        InfoSave *infoSaveData = memNew(sizeof(InfoSave));
        infoSaveData->memContext = memContextCurrent();

        // Open info file for write
        infoSaveData->infoWrite = storageWriteIo(storageNewWriteP(storage, fileName, .compressible = cipherType == cipherTypeNone));

        if (cipherType != cipherTypeNone)
        {
            ioFilterGroupAdd(
                ioWriteFilterGroup(infoSaveData->infoWrite),
                cipherBlockNew(cipherModeEncrypt, cipherType, BUFSTR(cipherPass), NULL));
        }

        ioWriteOpen(infoSaveData->infoWrite);

        // Hash object to calculate checksum
        infoSaveData->checksum = cryptoHashNew(HASH_TYPE_SHA1_STR);
        ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("{"));

        // Add version and format
        callbackFunction(callbackData, INFO_SECTION_BACKREST_STR, infoSaveData);
        infoSaveValue(infoSaveData, INFO_SECTION_BACKREST_STR, INFO_KEY_FORMAT_STR, jsonFromUInt(REPOSITORY_FORMAT));
        infoSaveValue(infoSaveData, INFO_SECTION_BACKREST_STR, INFO_KEY_VERSION_STR, jsonFromStr(STRDEF(PROJECT_VERSION)));

        // Add cipher passphrase if defined
        if (this->cipherPass != NULL)
        {
            callbackFunction(callbackData, INFO_SECTION_CIPHER_STR, infoSaveData);
            infoSaveValue(infoSaveData, INFO_SECTION_CIPHER_STR, INFO_KEY_CIPHER_PASS_STR, jsonFromStr(this->cipherPass));
        }

        // Flush out any additional sections
        callbackFunction(callbackData, NULL, infoSaveData);

        // Add checksum (this must be set after all other values or it will not be valid)
        ioFilterProcessIn(infoSaveData->checksum, BUFSTRDEF("}}"));
        infoSaveValue(
            infoSaveData, INFO_SECTION_BACKREST_STR, INFO_KEY_CHECKSUM_STR, jsonFromVar(ioFilterResult(infoSaveData->checksum), 0));

        // Close the file
        ioWriteClose(infoSaveData->infoWrite);

        // Copy to .copy file
        storageCopyNP(
            storageNewReadNP(storage, fileName), storageNewWriteNP(storage, strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Accessor functions
***********************************************************************************************************************************/
const String *
infoCipherPass(const Info *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->cipherPass);
}
