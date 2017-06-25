#!/bin/bash
source ./common.sh

TESTNAME='Can move a directory outside to inside the gdrive'

# This test necessary after discovering a move sees the linkedlist directory
# given 664 instead of 770 permissions

function runTest()
{
    echo 'hello world' >$PATHTOTMPFILE$FILE
    mv $PATHTOTMPFILE$FILE $GDRIVE$FILE
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^\t${GDRIVE}${FILE}$"

    local rfile=$(find ${RDRIVE} -wholename ${RDRIVE}${FILE} -printf "%y %M %p\n" 2>/dev/null)
    testStringEqual "metadata" "${rfile}" "^d drwxrwx--- ${RDRIVE}${FILE}\n$"
    local gfile=$(find ${GDRIVE} -wholename ${GDRIVE}${FILE} -printf "%y %M %p\n" 2>/dev/null)
    testStringEqual "metadata" "${gfile}" "^f -rw-rw-r-- ${GDRIVE}${FILE}\n$"

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${FILE})\n(\1/AA00000)(-[0-9]{20})\n(\2\3/:Fmetadata)\n\4\3\n\2\3/${FILE}\n\1/:latest\n$"
# '        /var/lib/samba/usershares/rdrive/test020.sha_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/rdrive/test020.sha_77585946cd986dda071f476978703ce_file/AA00000-20170617001522717337
#         /var/lib/samba/usershares/rdrive/test020.sha_77585946cd986dda071f476978703ce_file/AA00000-20170617001522717337/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test020.sha_77585946cd986dda071f476978703ce_file/AA00000-20170617001522717337/:Fmetadata-20170617001522717337
#         /var/lib/samba/usershares/rdrive/test020.sha_77585946cd986dda071f476978703ce_file/AA00000-20170617001522717337/test020.sha_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/rdrive/test020.sha_77585946cd986dda071f476978703ce_file/:latest'

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
    PATHTOTMPFILE='/tmp/'
    FILE=`basename "$0"`'a_77585946cd986dda071f476978703ce_file'
    PATHTOFILE=''
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${PATHTOTMPFILE}${FILE}"
    chmod 770 "${RDRIVE}${FILE}" 2>/dev/null
    checkAndRemove "${RDRIVE}${FILE}"
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
