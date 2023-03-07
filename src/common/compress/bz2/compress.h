/***********************************************************************************************************************************
BZ2 Compress

Compress IO to the bz2 format.
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_BZ2_COMPRESS_H
#define COMMON_COMPRESS_BZ2_COMPRESS_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BZ2_COMPRESS_FILTER_TYPE                                    STRID5("bz2-cmp", 0x41a3df3420)

/***********************************************************************************************************************************
Level constants
***********************************************************************************************************************************/
#define BZ2_COMPRESS_LEVEL_DEFAULT                                  9
#define BZ2_COMPRESS_LEVEL_MIN                                      1
#define BZ2_COMPRESS_LEVEL_MAX                                      9

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilter *bz2CompressNew(int level, bool raw);

#endif
