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

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE_BEFORE})\n\g{1}(-[0-9]{20})\n(\g{1}\g{2}/:Dmetadata)\n\g{3}-[0-9]{20}\n\g{3}-[0-9]{20}\n\g{3}-[0-9]{20}\n\g{1}/:Dmetadata\n\g{1}/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${PATHTOFILE_AFTER})\n\g{4}/:Dmetadata\n\g{4}/:Dmetadata-[0-9]{20}\n$"
# '        /var/lib/samba/usershares/rdrive/test004.sha__before
#         /var/lib/samba/usershares/rdrive/test004.sha__before-20170706004711526731
#         /var/lib/samba/usershares/rdrive/test004.sha__before-20170706004711526731/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test004.sha__before-20170706004711526731/:Dmetadata-20170706004711526989
#         /var/lib/samba/usershares/rdrive/test004.sha__before-20170706004711526731/:Dmetadata-20170706004711528230
#         /var/lib/samba/usershares/rdrive/test004.sha__before-20170706004711526731/:Dmetadata-20170706004711533578
#         /var/lib/samba/usershares/rdrive/test004.sha__before/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test004.sha__before/:Dmetadata-20170706004711528566
#         /var/lib/samba/usershares/rdrive/test004.shb__after
#         /var/lib/samba/usershares/rdrive/test004.shb__after/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test004.shb__after/:Dmetadata-20170706004711533928'

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
    PATHTOFILE_BEFORE=`basename "$0"`'a__before'
    PATHTOFILE_AFTER=`basename "$0"`'b__after'
    FILE=`basename "$0"`'_file'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS
{
    checkAndRemove "${RDRIVE}${PATHTOFILE_BEFORE}-"*
    checkAndRemove "${RDRIVE}${PATHTOFILE_BEFORE}"
    checkAndRemove "${RDRIVE}${PATHTOFILE_AFTER}-"*
    checkAndRemove "${RDRIVE}${PATHTOFILE_AFTER}"
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
