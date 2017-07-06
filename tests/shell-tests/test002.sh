#!/bin/bash
source ./common.sh

TESTNAME='Can create a subdirectory'

function runTest()
{
    mkdir $GDRIVE$PATHTOFILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${PATHTOFILE}$"
# '        /var/lib/samba/usershares/rdrive/test002.shee8bea7756ec790c3e6b3d6c09895924
#         /var/lib/samba/usershares/rdrive/test002.shee8bea7756ec790c3e6b3d6c09895924/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002.shee8bea7756ec790c3e6b3d6c09895924/:Dmetadata-20170630195445102460'

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PATHTOFILE})\n\1(-[0-9]{20})\n(\1\2/:Dmetadata)\n\3-[0-9]{20}\n\3-[0-9]{20}\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}$"
# '        /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622011028104145
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622011028104145/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622011028104145/:Dmetadata-20170622011028104206
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924-20170622011028104145/:Dmetadata-20170622011028104636
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/:Dmetadata
#         /var/lib/samba/usershares/rdrive/ee8bea7756ec790c3e6b3d6c09895924/:Dmetadata-20170622011028104771'

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
    PATHTOFILE=`basename "$0"`'ee8bea7756ec790c3e6b3d6c09895924'
    FILE=`basename "$0"`'_77585946cd986dda071f476978703cec'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
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
