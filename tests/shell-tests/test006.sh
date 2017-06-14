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

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE})\n(\1/:Dmetadata)\n\2-[0-9]{20}\n(\1/${FILE_BEFORE})\n(\3/AA00000-[0-9]{20})\n\4/:Fmetadata\n\4/:Fmetadata-[0-9]{20}\n\4/:Fmetadata-[0-9]{20}\n\4/${FILE_BEFORE}\n\3/:latest\n(\1/${FILE_AFTER})\n(\5/AA00000-[0-9]{20})\n(\6/:Fmetadata)\n\7-[0-9]{20}\n\5/:latest$"

# '        /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/:Dmetadata-20170613183340409977
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170613183340412217
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170613183340412217/:Fmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170613183340412217/:Fmetadata-20170613183340412217
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170613183340412217/:Fmetadata-20170613183340416405
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before/AA00000-20170613183340412217/test006.sha_77585946cd986dda071f476978703cec_before
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.sha_77585946cd986dda071f476978703cec_before/:latest
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.shb_77585946cd986dda071f476978703cec_after
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.shb_77585946cd986dda071f476978703cec_after/AA00000-20170613183340416591
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.shb_77585946cd986dda071f476978703cec_after/AA00000-20170613183340416591/:Fmetadata
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.shb_77585946cd986dda071f476978703cec_after/AA00000-20170613183340416591/:Fmetadata-20170613183340416591
#         /var/lib/samba/usershares/rdrive/ee7ac1bfb4434323f10239c01972bbab/test006.shb_77585946cd986dda071f476978703cec_after/:latest'

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
    PATHTOFILE='ee7ac1bfb4434323f10239c01972bbab'
    FILE_BEFORE=`basename "$0"`'a_77585946cd986dda071f476978703cec_before'
    FILE_AFTER=`basename "$0"`'b_77585946cd986dda071f476978703cec_after'
    checkAndRemove $RDRIVE$PATHTOFILE/$FILE_BEFORE
    checkAndRemove $RDRIVE$PATHTOFILE/$FILE_AFTER
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
