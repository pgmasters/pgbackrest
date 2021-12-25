/***********************************************************************************************************************************
Test Tape Archive
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    // Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("tarHdrReadU64()"))
    {
        TEST_TITLE("field full of octal digits");

        TEST_RESULT_UINT(tarHdrReadU64("77777777", 8), 077777777, "check octal");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid octal returns invalid result");

        TEST_RESULT_UINT(tarHdrReadU64("8", 1), 0, "invalid octal");
    }

    // *****************************************************************************************************************************
    if (testBegin("TarHeader"))
    {
        TEST_TITLE("check sizes");

        TEST_RESULT_UINT(sizeof(TarHeaderData), 512, "sizeof TarHeader");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("size errors");

        char nameLong[TAR_HEADER_NAME_SIZE + 1] = {0};
        memset(nameLong, 'a', TAR_HEADER_NAME_SIZE);
        TEST_ERROR_FMT(tarHdrNewP(.name = STR(nameLong)), FormatError, "file name '%s' is too long for the tar format", nameLong);

        char userLong[TAR_HEADER_UNAME_SIZE + 1] = {0};
        memset(userLong, 'u', TAR_HEADER_UNAME_SIZE);
        TEST_ERROR_FMT(
            tarHdrNewP(.name = STRDEF("test"), .user = STR(userLong)), FormatError, "user '%s' is too long for the tar format",
            userLong);

        char groupLong[TAR_HEADER_GNAME_SIZE + 1] = {0};
        memset(groupLong, 'u', TAR_HEADER_GNAME_SIZE);
        TEST_ERROR_FMT(
            tarHdrNewP(.name = STRDEF("test"), .group = STR(groupLong)), FormatError, "group '%s' is too long for the tar format",
            groupLong);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complete header");

        TarHeader *header = NULL;

        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("test"), .size = UINT64_MAX, .timeModified = 1640460255, .mode = 0640, .userId = 0,
            .user = STRDEF("user"), .groupId = 777777777, .group = STRDEF("group")), "new header");

        TEST_RESULT_STR_Z(tarHdrName(header), "test", "check name");
        TEST_RESULT_UINT(tarHdrSize(header), UINT64_MAX, "check size");

        TEST_RESULT_Z(header->data.name, "test", "check data name");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.size, TAR_HEADER_SIZE_SIZE), UINT64_MAX, "check data size");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.mtime, TAR_HEADER_MTIME_SIZE), 1640460255, "check data time");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.mode, TAR_HEADER_MODE_SIZE), 0640, "check data mode");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.uid, TAR_HEADER_UID_SIZE), 0, "check data uid");
        TEST_RESULT_Z(header->data.uname, "user", "check data user");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.gid, TAR_HEADER_GID_SIZE), 777777777, "check data gid");
        TEST_RESULT_Z(header->data.gname, "group", "check data group");

        TEST_RESULT_STR_Z(tarHdrToLog(header), "{name: test, size: 18446744073709551615}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple files in tar format");

        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("file1"), .size = 27, .timeModified = 1640460255, .mode = 0600,
            .userId = TEST_USER_ID, .groupId = TEST_GROUP_ID, ), "file with no user/group");
        (void)header; // !!!

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_TITLE("check range");

        // char field[12];

        // for (uint64_t idx = 0; idx < 1000000; idx++)
        // {
        //     memset(field, 0, sizeof(field));

        //     tarHdrWriteU64(field, sizeof(field), idx);

        //     if (tarHdrReadU64(field, sizeof(field)) != idx)
        //         THROW_FMT(AssertError, "failed on %" PRIu64, idx);
        // }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
