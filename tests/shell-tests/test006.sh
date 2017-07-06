#!/bin/bash
source ./common.sh

TESTNAME='Can rename a file in a subdirectory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE
    touch $GDRIVE$PATHTOFILE/$FILE_BEFORE
    mv $GDRIVE$PATHTOFILE/$FILE_BEFORE $GDRIVE$PATHTOFILE/$FILE_AFTER
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${PATHTOFILE}\n\t${GDRIVE}${PATHTOFILE}/${FILE_AFTER}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE})\n\1(-[0-9]{20})\n\1\2/:Dmetadata\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n(\1\2/${FILE_BEFORE})\n(\3/AA00000)(-[0-9]{20})\n\4\5/:Fmetadata\n\4\5/:Fmetadata\5\n\4\5/:Fmetadata-[0-9]{20}\n\4\5/${FILE_BEFORE}\n\3/:latest\n(\1\2/${FILE_AFTER})\n(\6/AA00000)(-[0-9]{20})\n\7\8/:Fmetadata\n\7\8/:Fmetadata\8\n\6/:latest\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n$"
# '        /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/:Dmetadata-20170622003933523923
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/:Dmetadata-20170622003933524406
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622003933526655
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622003933526655/:Fmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622003933526655/:Fmetadata-20170622003933526655
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622003933526655/:Fmetadata-20170622003933531080
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170622003933526655/test006.sha_77585946cd986dda071f476978703cec_before
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.sha_77585946cd986dda071f476978703cec_before/:latest
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.shb_77585946cd986dda071f476978703cec_after
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.shb_77585946cd986dda071f476978703cec_after/AA00000-20170622003933531272
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.shb_77585946cd986dda071f476978703cec_after/AA00000-20170622003933531272/:Fmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.shb_77585946cd986dda071f476978703cec_after/AA00000-20170622003933531272/:Fmetadata-20170622003933531272
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab-20170622003933523868/test006.shb_77585946cd986dda071f476978703cec_after/:latest
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/:Dmetadata-20170622003933524557'

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
    PATHTOFILE=`basename "$0"`'_dir'
    FILE_BEFORE=`basename "$0"`'a_before'
    FILE_AFTER=`basename "$0"`'b_after'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${PATHTOFILE}/${FILE_BEFORE}"
    checkAndRemove "${RDRIVE}${PATHTOFILE}/${FILE_AFTER}"
    checkAndRemove "${RDRIVE}${PATHTOFILE}-"*
    checkAndRemove "${RDRIVE}${PATHTOFILE}"*
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
