/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOARCHIVE_H
#define INFO_INFOARCHIVE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoArchive InfoArchive;

#include "common/type/string.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Archive info filename
***********************************************************************************************************************************/
#define INFO_ARCHIVE_FILE                                           "archive.info"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
InfoArchive *infoArchiveNew(const Storage *storage, const String *fileName, bool ignoreMissing);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void infoArchiveCheckPg(const InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId);
const String *infoArchiveIdMatch(const InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const String *infoArchiveId(const InfoArchive *this);
InfoPg *infoArchivePg(const InfoArchive *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoArchiveFree(InfoArchive *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_INFO_ARCHIVE_TYPE                                                                                           \
    InfoArchive *
#define FUNCTION_DEBUG_INFO_ARCHIVE_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "InfoArchive", buffer, bufferSize)

#endif
