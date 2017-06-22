#!/bin/bash
source ./common.sh

TESTNAME='Can create a subdirectory, rename it, then touch and rename a file within'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE_BEFORE
    mv $GDRIVE$PATHTOFILE_BEFORE $GDRIVE$PATHTOFILE_AFTER
    touch $GDRIVE$PATHTOFILE_AFTER/$FILE_BEFORE
    mv $GDRIVE$PATHTOFILE_AFTER/$FILE_BEFORE $GDRIVE$PATHTOFILE_AFTER/$FILE_AFTER
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^(\t${GDRIVE}${PATHTOFILE_AFTER})\n\1/${FILE_AFTER}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE_BEFORE})\n\1(-[0-9]{20})\n\1\2/:Dmetadata\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n(\1\2/${FILE_BEFORE})\n(\3/AA00000)(-[0-9]{20})\n\4\5/:Fmetadata\n\4\5/:Fmetadata\5\n\4\5/:Fmetadata-[0-9]{20}\n\4\5/${FILE_BEFORE}\n\3/:latest\n(\1\2/${FILE_AFTER})\n(\6/AA00000)(-[0-9]{20})\n\7\8/:Fmetadata\n\7\8/:Fmetadata\8\n\6/:latest\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${PATHTOFILE_AFTER})\n\9/:Dmetadata\n\9/:Dmetadata-[0-9]{20}$"

# '        /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/:Dmetadata-20170622002436964768
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/:Dmetadata-20170622002436965299
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/:Dmetadata-20170622002436969161
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622002436971500
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622002436971500/:Fmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622002436971500/:Fmetadata-20170622002436971500
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622002436971500/:Fmetadata-20170622002436976316
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622002436971500/test008.sha_77585946cd986dda071f476978703cec_before
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.sha_77585946cd986dda071f476978703cec_before/:latest
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.shb_77585946cd986dda071f476978703cec_after
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.shb_77585946cd986dda071f476978703cec_after/AA00000-20170622002436976500
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.shb_77585946cd986dda071f476978703cec_after/AA00000-20170622002436976500/:Fmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.shb_77585946cd986dda071f476978703cec_after/AA00000-20170622002436976500/:Fmetadata-20170622002436976500
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before-20170622002436964690/test008.shb_77585946cd986dda071f476978703cec_after/:latest
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before/:Dmetadata
#         /var/lib/samba/usershares/rdrive/a_ee8bea7756ec790c3e6b3d6c09895924_before/:Dmetadata-20170622002436965464
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after/:Dmetadata
#         /var/lib/samba/usershares/rdrive/b_ee8bea7756ec790c3e6b3d6c09895924_after/:Dmetadata-20170622002436969352'

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
    FILE_BEFORE=`basename "$0"`'a_77585946cd986dda071f476978703cec_before'
    FILE_AFTER=`basename "$0"`'b_77585946cd986dda071f476978703cec_after'
    checkAndRemove $RDRIVE$PATHTOFILE_BEFORE/$FILE_AFTER
    checkAndRemove $RDRIVE$PATHTOFILE_BEFORE/$FILE_BEFORE
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
