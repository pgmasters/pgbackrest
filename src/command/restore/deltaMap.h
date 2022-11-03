/***********************************************************************************************************************************
Restore Delta Map

Build a list of hashes based on a block size. This is used to compare the contents of a file to block map to determine what needs to
be updated.
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_DELTA_MAP_H
#define COMMAND_RESTORE_DELTA_MAP_H

#include "common/io/filter/filter.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define DELTA_MAP_FILTER_TYPE                                       STRID5("dlt-map", 0x402ddd1840)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoFilter *deltaMapNew(size_t blockSize);
IoFilter *deltaMapNewPack(const Pack *paramList);

#endif
