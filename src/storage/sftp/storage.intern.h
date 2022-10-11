/***********************************************************************************************************************************
Sftp Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_INTERN_H
#define STORAGE_SFTP_STORAGE_INTERN_H

#include "common/type/object.h"
#include "storage/sftp/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageSftpNewInternal(
    const StringId type, const String *path, const String *host, unsigned int port, TimeMSec timeoutConnect,
    TimeMSec timeoutSession, const String *user, const String *password, const String *keyPub, const String *keyPriv,
    const String *keyPassphrase, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction,
    bool pathSync);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageSftpPathCreate(
    THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode, StorageInterfacePathCreateParam param);
void storageSftpPathSync(THIS_VOID, const String *path, StorageInterfacePathSyncParam param);
// jrt !!! remove rc param...
void storageSftpEvalLibssh2Error(
    const int sessionErrno, const uint64_t sftpErrno, const int rc, const ErrorType *const errorType, const String *const msg,
    const String *const hint);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_SFTP_TYPE                                                                                             \
    StorageSftp *
#define FUNCTION_LOG_STORAGE_SFTP_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "StorageSftp *", buffer, bufferSize)

#endif
