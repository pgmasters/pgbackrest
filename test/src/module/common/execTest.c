/***********************************************************************************************************************************
Execute Process
***********************************************************************************************************************************/
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("Exec"))
    {
        Exec *exec = NULL;

        TEST_ASSIGN(exec, execNew(strNew("catt"), NULL, strNew("cat"), 1000), "invalid exec");
        TEST_RESULT_VOID(execOpen(exec), "open invalid exec");
        TEST_RESULT_VOID(ioWriteLine(execIoWrite(exec), EMPTY_STR), "write invalid exec");
        sleep(1);
        TEST_ERROR(
            ioWriteFlush(execIoWrite(exec)), ExecuteError,
            "cat terminated unexpectedly [102]: unable to execute 'catt': [2] No such file or directory");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(strNew("cat"), NULL, strNew("cat"), 1000), "new cat exec");
        TEST_RESULT_PTR(execMemContext(exec), exec->memContext, "get mem context");
        TEST_RESULT_INT(execHandleRead(exec), exec->handleRead, "check read handle");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");

        String *message = strNew("ACKBYACK");
        TEST_RESULT_VOID(ioWriteLine(execIoWrite(exec), message), "write cat exec");
        ioWriteFlush(execIoWrite(exec));
        TEST_RESULT_STR(strPtr(ioReadLine(execIoRead(exec))), strPtr(message), "read cat exec");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(strNew("cat"), NULL, strNew("cat"), 1000), "new cat exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");
        close(exec->handleWrite);

        TEST_ERROR(strPtr(ioReadLine(execIoRead(exec))), UnknownError, "cat terminated unexpectedly [0]");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(strNew("cat"), NULL, strNew("cat"), 1000), "new cat exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");
        kill(exec->processId, SIGKILL);

        TEST_ERROR(strPtr(ioReadLine(execIoRead(exec))), ExecuteError, "cat terminated unexpectedly on signal 9");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(strNew("cat"), strLstAddZ(strLstNew(), "-b"), strNew("cat"), 1000), "new cat exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");

        TEST_RESULT_VOID(ioWriteLine(execIoWrite(exec), message), "write cat exec");
        ioWriteFlush(execIoWrite(exec));
        TEST_RESULT_STR(strPtr(ioReadLine(execIoRead(exec))), "     1\tACKBYACK", "read cat exec");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // Run the same test as above but close all file descriptors first to ensure we don't accidentally close a required
        // descriptor while running dup2()/close() between the fork() and the exec().
        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                // This is not really fd max but for the purposes of testing is fine -- we won't have more than 64 fds open
                for (int fd = 0; fd < 64; fd++)
                    close(fd);

                TEST_ASSIGN(exec, execNew(strNew("cat"), strLstAddZ(strLstNew(), "-b"), strNew("cat"), 1000), "new cat exec");
                TEST_RESULT_VOID(execOpen(exec), "open cat exec");

                TEST_RESULT_VOID(ioWriteLine(execIoWrite(exec), message), "write cat exec");
                ioWriteFlush(execIoWrite(exec));
                TEST_RESULT_STR(strPtr(ioReadLine(execIoRead(exec))), "     1\tACKBYACK", "read cat exec");
                TEST_RESULT_VOID(execFree(exec), "free exec");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(strNew("sleep"), strLstAddZ(strLstNew(), "2"), strNew("sleep"), 1000), "new sleep exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");

        TEST_ERROR(execFree(exec), ExecuteError, "sleep did not exit when expected");

        TEST_ERROR(ioReadLine(execIoRead(exec)), FileReadError, "unable to select from sleep read: [9] Bad file descriptor");
        ioWriteLine(execIoWrite(exec), strNew(""));
        TEST_ERROR(ioWriteFlush(execIoWrite(exec)), FileWriteError, "unable to write to sleep write: [9] Bad file descriptor");

        sleepMSec(500);
        TEST_RESULT_VOID(execFree(exec), "sleep exited as expected");
        TEST_RESULT_VOID(execFree(NULL), "free null exec");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
