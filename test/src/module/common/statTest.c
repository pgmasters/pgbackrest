/***********************************************************************************************************************************
Test Statistics Collector
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("all"))
    {
        TEST_RESULT_VOID(statInit(), "init stats");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
