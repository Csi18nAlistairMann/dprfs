#!/bin/bash
source ./common.sh

TESTNAME='Can create a subdirectory, rename it, then touch a file within'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE_BEFORE
    mv $GDRIVE$PATHTOFILE_BEFORE $GDRIVE$PATHTOFILE_AFTER
    touch $GDRIVE$PATHTOFILE_AFTER/$FILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^(\t${GDRIVE}${PATHTOFILE_AFTER})\n\1/${FILE}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE_BEFORE})\n\1(-[0-9]{20})\n\1\2/:Dmetadata\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n(\1\2/${FILE})\n(\3/AA00000)(-[0-9]{20})\n\4\5/:Fmetadata\n\4\5/:Fmetadata\5\n\4\5/${FILE}\n\3/:latest\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${PATHTOFILE_AFTER})\n\6/:Dmetadata\n\6/:Dmetadata-[0-9]{20}$"
# '        /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/:Dmetadata-20170622002745817391
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/:Dmetadata-20170622002745817934
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/:Dmetadata-20170622002745821739
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/test007.sh_77585946cd986dda071f476978703cec
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/test007.sh_77585946cd986dda071f476978703cec/AA00000-20170622002745824345
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/test007.sh_77585946cd986dda071f476978703cec/AA00000-20170622002745824345/:Fmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/test007.sh_77585946cd986dda071f476978703cec/AA00000-20170622002745824345/:Fmetadata-20170622002745824345
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/test007.sh_77585946cd986dda071f476978703cec/AA00000-20170622002745824345/test007.sh_77585946cd986dda071f476978703cec
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002745817312/test007.sh_77585946cd986dda071f476978703cec/:latest
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before/:Dmetadata-20170622002745818097
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after/:Dmetadata
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after/:Dmetadata-20170622002745821928'

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
    PATHTOFILE_BEFORE=`basename "$0"`'a_ee8bea7756ec790c3e6b3d6c09895924_before'
    PATHTOFILE_AFTER=`basename "$0"`'b_ee8bea7756ec790c3e6b3d6c09895924_after'
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${PATHTOFILE_BEFORE}/${FILE}"
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
