#!/bin/bash
source ./common.sh

TESTNAME='Cannot run same tests quickly even though listing is clear'

function runTest()
{
    echo "${FAKECONTENTS}" >"${GDRIVE}${WORLDTXTFILE}"
    mv "${GDRIVE}${WORLDTXTFILE}" "${GDRIVE}${WORLDTMPFILE}"
}

function getTestResults()
{
    testContents "${GDRIVE}${WORLDTMPFILE}" "${FAKECONTENTS}" 2&>/dev/null

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
    SHA256='8a5edab282632443219e051e4ade2d1d5bbc671c781051bf1437897cbdfea0f1'
    WORLDTXTFILE='world.txt'
    WORLDTMPFILE='world.tmp'
    FAKECONTENTS='hello'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS
{
    checkAndRemove "${RDRIVE}${WORLDTXTFILE}"
    checkAndRemove "${TDRIVE}${SHA256}-${WORLDTMPFILE}"
}

# Main
pretestWork
runTest
clearFS
runTest
postTestWork
getTestResults
# ls "/var/lib/samba/usershares/gdrive" -altr 2&>/dev/null
clearFS
exit $FAILEDTESTS
