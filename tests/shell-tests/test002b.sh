#!/bin/bash
source ./common.sh

TESTNAME='Can rename a subdirectory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE_BEFORE
    mv $GDRIVE$PATHTOFILE_BEFORE $GDRIVE$PATHTOFILE_AFTER
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${PATHTOFILE_AFTER}$"
# '        /var/lib/samba/usershares/rdrive/test002.shee8bea7756ec790c3e6b3d6c09895924
#         /var/lib/samba/usershares/rdrive/test002.shee8bea7756ec790c3e6b3d6c09895924/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002.shee8bea7756ec790c3e6b3d6c09895924/:Dmetadata-20170630195445102460'

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE_AFTER})\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${PATHTOFILE_BEFORE})\n\2(-[0-9]{20})\n\2\3/:Dmetadata\n\2\3/:Dmetadata-[0-9]{20}\n\2\3/:Dmetadata-[0-9]{20}\n\2\3/:Dmetadata-[0-9]{20}\n\2/:Dmetadata\n\2/:Dmetadata-[0-9]{20}\n$"
# '        /var/lib/samba/usershares/rdrive/test002b.sh_after
#         /var/lib/samba/usershares/rdrive/test002b.sh_after/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002b.sh_after/:Dmetadata-20170706004602365307
#         /var/lib/samba/usershares/rdrive/test002b.sh_before
#         /var/lib/samba/usershares/rdrive/test002b.sh_before-20170706004602358233
#         /var/lib/samba/usershares/rdrive/test002b.sh_before-20170706004602358233/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002b.sh_before-20170706004602358233/:Dmetadata-20170706004602358507
#         /var/lib/samba/usershares/rdrive/test002b.sh_before-20170706004602358233/:Dmetadata-20170706004602359668
#         /var/lib/samba/usershares/rdrive/test002b.sh_before-20170706004602358233/:Dmetadata-20170706004602364961
#         /var/lib/samba/usershares/rdrive/test002b.sh_before/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002b.sh_before/:Dmetadata-20170706004602360075'

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
    PATHTOFILE_BEFORE=`basename "$0"`'_before'
    PATHTOFILE_AFTER=`basename "$0"`'_after'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${PATHTOFILE_BEFORE}-"*
    checkAndRemove "${RDRIVE}${PATHTOFILE_BEFORE}"*
    checkAndRemove "${RDRIVE}${PATHTOFILE_AFTER}-"*
    checkAndRemove "${RDRIVE}${PATHTOFILE_AFTER}"*
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
