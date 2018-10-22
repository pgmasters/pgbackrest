/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOBACKUP_H
#define INFO_INFOBACKUP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoBackup InfoBackup;

#include "common/type/string.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Backup info filename
***********************************************************************************************************************************/
#define INFO_BACKUP_FILE                                           "backup.info"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
InfoBackup *infoBackupNew(const Storage *storage, const String *fileName, bool ignoreMissing);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
unsigned int infoBackupCheckPg(
    const InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, uint32_t pgCatalogVersion, uint32_t pgControlVersion);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
InfoPg *infoBackupPg(const InfoBackup *this);
const Variant *infoBackupCurrentGet(const InfoBackup *this, const String *section, const String *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoBackupFree(InfoBackup *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_INFO_BACKUP_TYPE                                                                                           \
    InfoBackup *
#define FUNCTION_DEBUG_INFO_BACKUP_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "InfoBackup", buffer, bufferSize)

#endif
