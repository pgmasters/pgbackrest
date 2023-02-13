/***********************************************************************************************************************************
Block Restore

Calculate and return the blocks required to restore a file using an optional block hash list. The block hash list is optional
because the file to restore may not exist so all the blocks will need to be restored.
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCKDELTA_H
#define COMMAND_BACKUP_BLOCKDELTA_H

#include "command/backup/blockMap.h"
#include "common/compress/helper.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockRestore BlockRestore;

// Reads that must be performed in order to extract blocks
typedef struct BlockRestoreRead
{
    unsigned int reference;                                         // Reference to read from
    uint64_t bundleId;                                              // Bundle to read from
    uint64_t offset;                                                // Offset to begin read from
    uint64_t size;                                                  // Size of the read
    List *superBlockList;                                           // Super block list
} BlockRestoreRead;

// Writes that need to be performed to restore the file
typedef struct BlockRestoreWrite
{
    uint64_t offset;                                                // Offset for the write
    Buffer *block;                                                  // Block to write
} BlockRestoreWrite;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN BlockRestore *blockRestoreNew(
    const BlockMap *blockMap, size_t blockSize, const Buffer *blockHash, CipherType cipherType, const String *cipherPass,
    const CompressType compressType);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get the next write for the restore
FN_EXTERN const BlockRestoreWrite *blockRestoreNext(BlockRestore *this, const BlockRestoreRead *readDelta, IoRead *readIo);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct BlockRestorePub
{
    List *readList;                                                 // Read list
} BlockRestorePub;

// Get read info
FN_INLINE_ALWAYS const BlockRestoreRead *
blockRestoreReadGet(const BlockRestore *const this, const unsigned int readIdx)
{
    return (BlockRestoreRead *)lstGet(THIS_PUB(BlockRestore)->readList, readIdx);
}

// Read list size
FN_INLINE_ALWAYS unsigned int
blockRestoreReadSize(const BlockRestore *const this)
{
    return lstSize(THIS_PUB(BlockRestore)->readList);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
blockRestoreFree(BlockMap *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_DELTA_TYPE                                                                                              \
    BlockRestore *
#define FUNCTION_LOG_BLOCK_DELTA_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "BlockRestore", buffer, bufferSize)

#endif
