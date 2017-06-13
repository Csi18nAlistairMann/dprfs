#!/bin/bash
source ./common.sh

TESTNAME='Can create a subdirectory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${PATHTOFILE}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^\t${RDRIVE}${PATHTOFILE}\n\t${RDRIVE}${PATHTOFILE}/:Dmetadata\n\t${RDRIVE}${PATHTOFILE}/:Dmetadata\-[0-9]{20}$"

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
    PATHTOFILE='ee8bea7756ec790c3e6b3d6c09895924'
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
    checkAndRemove $RDRIVE$PATHTOFILE$FILE
    checkAndRemove $RDRIVE$PATHTOFILE
    FAILEDTESTS=0
    NUMTESTS=0
}

# Main
pretestWork
runTest
postTestWork
getTestResults
exit $FAILEDTESTS
