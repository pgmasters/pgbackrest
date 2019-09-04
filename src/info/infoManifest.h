/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOMANIFEST_H
#define INFO_INFOMANIFEST_H

#include "command/backup/common.h"
#include "common/crypto/common.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_MANIFEST_FILE                                          "backup.manifest"
    STRING_DECLARE(INFO_MANIFEST_FILE_STR);

#define INFO_MANIFEST_TARGET_PGDATA                                 "pg_data"
    STRING_DECLARE(INFO_MANIFEST_TARGET_PGDATA_STR);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoManifest InfoManifest;

#include "common/crypto/hash.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Manifest data
***********************************************************************************************************************************/
typedef struct InfoManifestData
{
    const String *backupLabel;                                      // Backup label (unique identifier for the backup)
    const String *backupLabelPrior;                                 // Backup label for backup this diff/incr is based on
    time_t backupTimestampCopyStart;                                // When did the file copy start?
    time_t backupTimestampStart;                                    // When did the backup start?
    time_t backupTimestampStop;                                     // When did the backup stop?
    BackupType backupType;                                          // Type of backup: full, diff, incr

    // ??? Note that these fields are redundant and verbose since toring the start/stop lsn as a uint64 would be sufficient.
    // However, we currently lack the functions to transform these values back and forth so this will do for now.
    const String *archiveStart;                                     // First WAL file in the backup
    const String *archiveStop;                                      // Last WAL file in the backup
    const String *lsnStart;                                         // Start LSN for the backup
    const String *lsnStop;                                          // Stop LSN for the backup

    unsigned int pgId;                                              // PostgreSQL id in backup.info
    unsigned int pgVersion;                                         // PostgreSQL version
    uint64_t pgSystemId;                                            // PostgreSQL system identifier
    uint32_t pgControlVersion;                                      // PostgreSQL control version
    uint32_t pgCatalogVersion;                                      // PostgreSQL catalog version

    bool backupOptionArchiveCheck;                                  // Will WAL segments be checked at the end of the backup?
    bool backupOptionArchiveCopy;                                   // Will WAL segments be copied to the backup?
    const Variant *backupOptionStandby;                             // Will the backup be performed from a standby?
    const Variant *backupOptionBufferSize;                          // Buffer size used for file/protocol operations
    const Variant *backupOptionChecksumPage;                        // Will page checksums be verified?
    bool backupOptionCompress;                                      // Will compression be used for backup?
    const Variant *backupOptionCompressLevel;                       // Level to use for compression
    const Variant *backupOptionCompressLevelNetwork;                // Level to use for network compression
    const Variant *backupOptionDelta;                               // Will a checksum delta be performed?
    bool backupOptionHardLink;                                      // Will hardlinks be created in the backup?
    bool backupOptionOnline;                                        // Will an online backup be performed?
    const Variant *backupOptionProcessMax;                          // How many processes will be used for backup?
} InfoManifestData;

/***********************************************************************************************************************************
Target type
***********************************************************************************************************************************/
typedef enum
{
    manifestTargetTypePath,
    manifestTargetTypeLink,
} ManifestTargetType;

typedef struct InfoManifestTarget
{
    const String *name;                                             // Target name (must be first member in struct)
    ManifestTargetType type;                                        // Target type
    const String *path;                                             // Target path (if path or link)
    const String *file;                                             // Target file (if file link)
    unsigned int tablespaceId;                                      // Oid if this link is a tablespace
    const String *tablespaceName;                                   // Name of the tablespace
} InfoManifestTarget;

/***********************************************************************************************************************************
Path type
***********************************************************************************************************************************/
typedef struct InfoManifestPath
{
    const String *name;                                             // Path name (must be first member in struct)
    bool base:1;                                                    // Is this the base path?
    bool db:1;                                                      // Does this path contain db relation files?
    mode_t mode;                                                    // Directory mode
    const String *user;                                             // User name
    const String *group;                                            // Group name
} InfoManifestPath;

/***********************************************************************************************************************************
File type
***********************************************************************************************************************************/
typedef struct InfoManifestFile
{
    const String *name;                                             // File name (must be first member in struct)
    bool primary:1;                                                 // Should this file be copied from the primary?
    bool checksumPage:1;                                            // Does this file have page checksums?
    bool checksumPageError:1;                                       // Is there an error in the page checksum?
    mode_t mode;                                                    // File mode
    char checksumSha1[HASH_TYPE_SHA1_SIZE_HEX + 1];                 // SHA1 checksum
    const VariantList *checksumPageErrorList;                       // List of page checksum errors if there are any
    const String *user;                                             // User name
    const String *group;                                            // Group name
    const String *reference;                                        // Reference to a prior backup
    uint64_t size;                                                  // Original size
    uint64_t sizeRepo;                                              // Size in repo
    time_t timestamp;                                               // Original timestamp
} InfoManifestFile;

/***********************************************************************************************************************************
Link type
***********************************************************************************************************************************/
typedef struct InfoManifestLink
{
    const String *name;                                             // Link name (must be first member in struct)
    const String *destination;                                      // Link destination
    const String *user;                                             // User name
    const String *group;                                            // Group name
} InfoManifestLink;

/***********************************************************************************************************************************
Db type
***********************************************************************************************************************************/
typedef struct InfoManifestDb
{
    const String *name;                                             // Db name (must be first member in struct)
    unsigned int id;                                                // Db oid
    unsigned int lastSystemId;                                      // Highest oid used by system objects in this database
} InfoManifestDb;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
InfoManifest *infoManifestNewLoad(IoRead *read);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void infoManifestSave(InfoManifest *this, IoWrite *write);

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
const InfoManifestTarget *infoManifestTarget(const InfoManifest *this, unsigned int targetIdx);
const InfoManifestTarget *infoManifestTargetFind(const InfoManifest *this, const String *name);
const InfoManifestTarget *infoManifestTargetFindDefault(
    const InfoManifest *this, const String *name, const InfoManifestTarget *targetDefault);
unsigned int infoManifestTargetTotal(const InfoManifest *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const InfoManifestData *infoManifestData(const InfoManifest *this);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
InfoManifest *infoManifestLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_MANIFEST_TYPE                                                                                            \
    InfoManifest *
#define FUNCTION_LOG_INFO_MANIFEST_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "InfoManifest", buffer, bufferSize)

#define FUNCTION_LOG_INFO_MANIFEST_TARGET_TYPE                                                                                     \
    InfoManifestTarget *
#define FUNCTION_LOG_INFO_MANIFEST_TARGET_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(value, "InfoManifestTarget", buffer, bufferSize)

#endif
