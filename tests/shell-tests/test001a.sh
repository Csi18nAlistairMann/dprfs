#!/bin/bash
source ./common.sh

TESTNAME='Can rename a file in the root directory'

function runTest()
{
    touch $GDRIVE$FILE_BEFORE
    mv $GDRIVE$FILE_BEFORE $GDRIVE$FILE_AFTER
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${FILE_AFTER}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^\t${RDRIVE}${FILE_AFTER}\n\t${RDRIVE}${FILE_AFTER}/(AA00000)(-[0-9]{20})\n\t${RDRIVE}${FILE_AFTER}/\g{1}\g{2}/:Fmetadata\n\t${RDRIVE}${FILE_AFTER}/\g{1}\g{2}/:Fmetadata\g{2}\n\t${RDRIVE}${FILE_AFTER}/:latest\n\t${RDRIVE}${FILE_BEFORE}\n(\t${RDRIVE}${FILE_BEFORE}/AA00000)(-[0-9]{20})\n\g{3}\g{4}/:Fmetadata\n\g{3}\g{4}/:Fmetadata\g{4}\n\g{3}\g{4}/:Fmetadata-[0-9]{20}\n\g{3}\g{4}/${FILE_BEFORE}\n\t${RDRIVE}${FILE_BEFORE}/:latest$"
# '        /var/lib/samba/usershares/rdrive/test001a.sh_after
#         /var/lib/samba/usershares/rdrive/test001a.sh_after/AA00000-20170703131327766397
#         /var/lib/samba/usershares/rdrive/test001a.sh_after/AA00000-20170703131327766397/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test001a.sh_after/AA00000-20170703131327766397/:Fmetadata-20170703131327766397
#         /var/lib/samba/usershares/rdrive/test001a.sh_after/:latest
#         /var/lib/samba/usershares/rdrive/test001a.sh_before
#         /var/lib/samba/usershares/rdrive/test001a.sh_before/AA00000-20170703131327759104
#         /var/lib/samba/usershares/rdrive/test001a.sh_before/AA00000-20170703131327759104/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test001a.sh_before/AA00000-20170703131327759104/:Fmetadata-20170703131327759104
#         /var/lib/samba/usershares/rdrive/test001a.sh_before/AA00000-20170703131327759104/:Fmetadata-20170703131327766004
#         /var/lib/samba/usershares/rdrive/test001a.sh_before/AA00000-20170703131327759104/test001a.sh_before
#         /var/lib/samba/usershares/rdrive/test001a.sh_before/:latest'

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
    PATHTOFILE=''
    FILE_BEFORE=`basename "$0"`'_before'
    FILE_AFTER=`basename "$0"`'_after'
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${FILE_BEFORE}"
    checkAndRemove "${RDRIVE}${FILE_AFTER}"
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
