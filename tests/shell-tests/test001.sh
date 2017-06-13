#!/bin/bash
source ./common.sh

TESTNAME='Can touch a file in the root directory'

function runTest()
{
    touch $GDRIVE$FILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${FILE}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^\t${RDRIVE}${FILE}\n\t${RDRIVE}${FILE}/(AA00000\-)([0-9]{20})\n\t${RDRIVE}${FILE}/\1\2/:Fmetadata\n\t${RDRIVE}${FILE}/\1\2/:Fmetadata-\2\n\t${RDRIVE}${FILE}/\1\2/${FILE}\n\t${RDRIVE}${FILE}/:latest$"

    testStringEmpty "TDRIVE" "${DIFF_TDRIVE}"

    if [ $FAILEDTESTS -eq 0 ]
    then
	printf "[Passed] $NUMTESTS/$NUMTESTS - $TESTNAME\n"
    else
	printf "[Failed] $FAILEDTESTS/$NUMTESTS - $TESTNAME\n";
    fi
}

function establishThisTestGlobals()
{
    # Constants for this test
    PATHTOFILE=''
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
    checkAndRemove $RDRIVE$FILE
    FAILEDTESTS=0
    NUMTESTS=0
}

# Main
pretestWork
runTest
postTestWork
getTestResults
exit $FAILEDTESTS
