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

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE_BEFORE})\n\1(-[0-9]{20})\n(\1\2/:Dmetadata)\n\3-[0-9]{20}\n\3-[0-9]{20}\n\3-[0-9]{20}\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${PATHTOFILE_AFTER})\n\4/:Dmetadata\n\4/:Dmetadata-[0-9]{20}\n$"
# '        /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before-20170622010133134652
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before-20170622010133134652/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before-20170622010133134652/:Dmetadata-20170622010133134716
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before-20170622010133134652/:Dmetadata-20170622010133135157
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before-20170622010133134652/:Dmetadata-20170622010133138224
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee7ac1bfb4434323f10239c01972bbab_before/:Dmetadata-20170622010133135292
#         /var/lib/samba/usershares/rdrive/b_9105fcc12cdba58560ad34db34ef3a3a_after
#         /var/lib/samba/usershares/rdrive/b_9105fcc12cdba58560ad34db34ef3a3a_after/:Dmetadata
#         /var/lib/samba/usershares/rdrive/b_9105fcc12cdba58560ad34db34ef3a3a_after/:Dmetadata-20170622010133138391'

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
    PATHTOFILE_BEFORE=`basename "$0"`'a_ee7ac1bfb4434323f10239c01972bbab_before'
    PATHTOFILE_AFTER=`basename "$0"`'b_9105fcc12cdba58560ad34db34ef3a3a_after'
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
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
