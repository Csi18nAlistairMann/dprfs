#!/bin/bash
source ./common.sh

TESTNAME='Can touch a file in a subdirectory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE
    touch $GDRIVE$PATHTOFILE/$FILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${PATHTOFILE}/${FILE}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^\t${RDRIVE}${PATHTOFILE}/$FILE\n\t${RDRIVE}${PATHTOFILE}/$FILE/(AA00000\-)([0-9]{20})\n\t${RDRIVE}${PATHTOFILE}/$FILE/\1\2/:Fmetadata\n\t${RDRIVE}${PATHTOFILE}/$FILE/\1\2/:Fmetadata-\2\n\t${RDRIVE}${PATHTOFILE}/$FILE/\1\2/${FILE}\n\t${RDRIVE}${PATHTOFILE}/$FILE/:latest$"

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
