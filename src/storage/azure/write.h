/***********************************************************************************************************************************
Azure Storage File write
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_WRITE_H
#define STORAGE_AZURE_WRITE_H

#include "storage/azure/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageWrite *storageWriteAzureNew(StorageAzure *storage, const String *name, size_t partSize);

#endif
