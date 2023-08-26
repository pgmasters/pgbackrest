/***********************************************************************************************************************************
CIFS Storage Helper
***********************************************************************************************************************************/
#ifndef STORAGE_CIFS_STORAGE_HELPER_H
#define STORAGE_CIFS_STORAGE_HELPER_H

#include "storage/cifs/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN Storage *storageCifsHelper(
    unsigned int repoIdx, bool write, StoragePathExpressionCallback pathExpressionCallback, const Pack *tag);

/***********************************************************************************************************************************
Storage helper for StorageHelper array passed to storageHelperInit()
***********************************************************************************************************************************/
#define STORAGE_CIFS_HELPER                                         {.type = STORAGE_CIFS_TYPE, .helper = storageCifsHelper}

#endif
