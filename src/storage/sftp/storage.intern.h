/***********************************************************************************************************************************
SFTP Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_INTERN_H
#define STORAGE_SFTP_STORAGE_INTERN_H

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "storage/sftp/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageSftp StorageSftp;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN void storageSftpEvalLibSsh2Error(
    int ssh2Errno, uint64_t sftpErrno, const ErrorType *errorType, const String *msg, const String *hint);

FN_EXTERN bool storageSftpWaitFd(
    StorageSftp *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_SFTP_TYPE                                                                                             \
    StorageSftp *
#define FUNCTION_LOG_STORAGE_SFTP_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "StorageSftp *", buffer, bufferSize)

#endif
