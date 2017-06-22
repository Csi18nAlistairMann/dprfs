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

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE})\n\1(-[0-9]{20})\n(\1\2/:Dmetadata)\n\3-[0-9]{20}\n\3-[0-9]{20}\n(\1\2/${FILE})\n(\4/AA00000)(-[0-9]{20})\n\5\6/:Fmetadata\n\5\6/:Fmetadata\6\n\5\6/${FILE}\n\4/:latest\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}$"

# '        /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/:Dmetadata-20170622010250528100
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/:Dmetadata-20170622010250528531
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/test003.sh_77585946cd986dda071f476978703cec
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170622010250530644
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170622010250530644/:Fmetadata
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170622010250530644/:Fmetadata-20170622010250530644
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/test003.sh_77585946cd986dda071f476978703cec/AA00000-20170622010250530644/test003.sh_77585946cd986dda071f476978703cec
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622010250528038/test003.sh_77585946cd986dda071f476978703cec/:latest
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/:Dmetadata-20170622010250528665'

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
