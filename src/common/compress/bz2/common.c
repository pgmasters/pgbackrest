/***********************************************************************************************************************************
BZ2 Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <bzlib.h>

#include "common/compress/bz2/common.h"
#include "common/debug.h"
#include "common/memContext.h"

/**********************************************************************************************************************************/
int
bz2Error(int error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, error);
    FUNCTION_TEST_END();

    if (error < 0)
	{
		const char *errorMsg;
		const ErrorType *errorType = &FormatError;
		
		switch (error)
		{
			case BZ_SEQUENCE_ERROR:
			{
				errorMsg = "sequence error";
				errorType = &AssertError;
				break;
			}

			case BZ_PARAM_ERROR:
			{
				errorMsg = "parameter error";
				errorType = &AssertError;
				break;
			}

			case BZ_MEM_ERROR:
			{
				errorMsg = "memory error";
				errorType = &AssertError;
				break;
			}

			case BZ_DATA_ERROR:
			{
				errorMsg = "data error";
				errorType = &AssertError;
				break;
			}

			case BZ_DATA_ERROR_MAGIC:
			{
				errorMsg = "data error magic";
				errorType = &AssertError;
				break;
			}

			case BZ_IO_ERROR:
			{
				errorMsg = "io error";
				errorType = &AssertError;
				break;
			}

			case BZ_UNEXPECTED_EOF:
			{
				errorMsg = "unexpected eof";
				errorType = &AssertError;
				break;
			}

			case BZ_OUTBUFF_FULL:
			{
				errorMsg = "outbuff full";
				errorType = &AssertError;
				break;
			}

			case BZ_CONFIG_ERROR:
			{
				errorMsg = "config error";
				errorType = &AssertError;
				break;
			}

			default:
			{
				errorMsg = "unknown error";
				errorType = &AssertError;
			}
		}
        THROWP_FMT(errorType, "bz2 error: [%d] %s", error, errorMsg);
	}

    FUNCTION_TEST_RETURN(error);
}
