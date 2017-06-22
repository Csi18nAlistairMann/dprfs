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

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE_BEFORE})\n\1(-[0-9]{20})\n(\1\2/:Dmetadata)\n\3-[0-9]{20}\n\3-[0-9]{20}\n\3-[0-9]{20}\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${PATHTOFILE_AFTER})\n\4/:Dmetadata\n\4/:Dmetadata-[0-9]{20}\n$"

# '        /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622005516557081
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622005516557081/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622005516557081/:Dmetadata-20170622005516557151
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622005516557081/:Dmetadata-20170622005516557620
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622005516557081/:Dmetadata-20170622005516561041
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before/:Dmetadata-20170622005516557760
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after/:Dmetadata
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after/:Dmetadata-20170622005516561216'

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
    PATHTOFILE_BEFORE='a_ee8bea7756ec790c3e6b3d6c09895924_before'
    PATHTOFILE_AFTER='b_ee8bea7756ec790c3e6b3d6c09895924_after'
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
