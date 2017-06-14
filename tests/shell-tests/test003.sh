#!/bin/bash
source ./common.sh

TESTNAME='Can touch a file in a subdirectory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE
    touch $GDRIVE$PATHTOFILE/$FILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^(\t${GDRIVE}${PATHTOFILE})\n\1/${FILE}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE})\n(\1/:Dmetadata)\n\2-[0-9]{20}\n(\1/${FILE})\n(\3/AA00000)(-[0-9]{20})\n\4\5/:Fmetadata\n\4\5/:Fmetadata\5\n\4\5/${FILE}\n\3/:latest$"

   # /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/:Dmetadata
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/:Dmetadata-20170614091351958270
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/test003.sh_77585946cd986dda071f476978703cec
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170614091351960408
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170614091351960408/:Fmetadata
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170614091351960408/:Fmetadata-20170614091351960408
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170614091351960408/test003.sh_77585946cd986dda071f476978703cec
   #      /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/test003.sh_77585946cd986dda071f476978703cec/:latest

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
    PATHTOFILE='ee8bea7756ec790c3e6b3d6c09895924'
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
    checkAndRemove $RDRIVE$PATHTOFILE$FILE
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
