#!/bin/bash
source ./common.sh

TESTNAME='Can see both saves of a file saved twice'

function runTest()
{
    echo -n "$SAVE1" >"$GDRIVE$FILE"
    echo -n "$SAVE2" >>"$GDRIVE$FILE"
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${FILE}$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^\t${RDRIVE}${FILE}\n(\t${RDRIVE}${FILE}/AA00000)(-[0-9]{20})\n\g{1}\g{2}/:Fmetadata\n\g{1}\g{2}/:Fmetadata\g{2}\n\g{1}\g{2}/${FILE}\n(\t${RDRIVE}${FILE}/AA00001)(-[0-9]{20})\n\g{3}\g{4}/:Fmetadata\n\g{3}\g{4}/:Fmetadata\g{4}\n\g{3}\g{4}/${FILE}\n\t${RDRIVE}${FILE}/:latest$"
# '        /var/lib/samba/usershares/rdrive/test010.sh_FILE
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00000-20170709174612779116
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00000-20170709174612779116/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00000-20170709174612779116/:Fmetadata-20170709174612779116
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00000-20170709174612779116/test010.sh_FILE
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00001-20170709174612782063
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00001-20170709174612782063/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00001-20170709174612782063/:Fmetadata-20170709174612782063
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/AA00001-20170709174612782063/test010.sh_FILE
#         /var/lib/samba/usershares/rdrive/test010.sh_FILE/:latest'

    testStringEmpty "TDRIVE" "${DIFF_TDRIVE}"

    FIRST_REVISION_FILE=`find $RDRIVE | grep AA00000 | sort | tail -1`
    SECOND_REVISION_FILE=`find $RDRIVE | grep AA00001 | sort | tail -1`

    testContents $FIRST_REVISION_FILE "$SAVE1"
    testContents $SECOND_REVISION_FILE "$SAVE1$SAVE2"

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
    SAVE1='Hello'
    SAVE2=', World'
    FILE=`basename "$0"`'_FILE'
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${FILE}"
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
