#!/bin/bash
source ./common.sh

TESTNAME='Can rename a file in the root directory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE_BEFORE
    mv $GDRIVE$PATHTOFILE_BEFORE $GDRIVE$PATHTOFILE_AFTER
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${PATHTOFILE_AFTER}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^\t${RDRIVE}${PATHTOFILE_BEFORE}\n\t${RDRIVE}${PATHTOFILE_BEFORE}/:Dmetadata\n\t${RDRIVE}${PATHTOFILE_BEFORE}/:Dmetadata\-[0-9]{20}\n\t${RDRIVE}${PATHTOFILE_BEFORE}/:Dmetadata\-[0-9]{20}\n\t${RDRIVE}${PATHTOFILE_AFTER}\n\t${RDRIVE}${PATHTOFILE_AFTER}/:Dmetadata\n\t${RDRIVE}${PATHTOFILE_AFTER}/:Dmetadata\-[0-9]{20}$"

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
    PATHTOFILE_BEFORE='a_ee7ac1bfb4434323f10239c01972bbab_before'
    PATHTOFILE_AFTER='b_9105fcc12cdba58560ad34db34ef3a3a_after'
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
    checkAndRemove $RDRIVE$PATHTOFILE_BEFORE
    checkAndRemove $RDRIVE$PATHTOFILE_AFTER
    FAILEDTESTS=0
    NUMTESTS=0
}

# Main
pretestWork
runTest
postTestWork
getTestResults
exit $FAILEDTESTS
