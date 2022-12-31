/***********************************************************************************************************************************
Gz Decompress

Decompress IO from the gz format.
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_GZ_DECOMPRESS_H
#define COMMON_COMPRESS_GZ_DECOMPRESS_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define GZ_DECOMPRESS_FILTER_TYPE                                   STRID5("gz-dcmp", 0x41a326f470)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FV_EXTERN IoFilter *gzDecompressNew(void);

#endif
