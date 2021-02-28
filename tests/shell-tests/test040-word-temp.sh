#!/bin/bash
source ./common.sh

TESTNAME='Replicate Microsoft Words use of temporary files'

function runTest()
{
    # touch gdrive/6oct.docx
    # // touch gdrive/~$6oct.docx
    # echo "content" >gdrive/2F793998.tmp
    # mv gdrive/6oct.docx gdrive/55F27CB9.tmp
    # mv gdrive/2F793998.tmp gddrive/6oct.docx
    # // rm gdrive/55F27CB9.tmp
    touch "${GDRIVE}${A_6OCTDOCX}"
    touch "${GDRIVE}${A_6OCTDOCX}"
    touch "${GDRIVE}${A_TILDEDOLLAR6OCTDOCX}"
    #    mv "${GDRIVE}${A_6OCTDOCX}" "${GDRIVE}${A_6OCTDOCX}"
    truncate -s 0 "${GDRIVE}${A_2F793998TMP}"
    echo "${FAKECONTENT}" >>"${GDRIVE}${A_2F793998TMP}"
    touch "${GDRIVE}${A_2F793998TMP}"
    mv "${GDRIVE}${A_6OCTDOCX}" "${GDRIVE}${A_55F27CB9TMP}"
    chmod +r "${GDRIVE}${A_55F27CB9TMP}"
    mv "${GDRIVE}${A_2F793998TMP}" "${GDRIVE}${A_6OCTDOCX}"
    chmod +r "${GDRIVE}${A_6OCTDOCX}"
    rm "${GDRIVE}${A_55F27CB9TMP}"
}

function getTestResults()
{
    testContents "${GDRIVE}${WORLDTMPFILE}" "${FAKECONTENTS}" 2&>/dev/null

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
    SHA256='8a5edab282632443219e051e4ade2d1d5bbc671c781051bf1437897cbdfea0f1'
    FAKECONTENT='Hello, world!'
    A_6OCTDOCX='6oct.docx'
    A_2F793998TMP='2F793998.tmp'
    A_55F27CB9TMP='55F27CB9.tmp'
    A_TILDEDOLLAR6OCTDOCX='~$6oct.docx'

    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS
{
    checkAndRemove "${RDRIVE}${A_6OCTDOCX}"
    checkAndRemove "${RDRIVE}${A_TILDEDOLLAR6OCTDOCX}"
    checkAndRemove "${TDRIVE}${SHA256}:${A_2F793998TMP}"
    checkAndRemove "${TDRIVE}${SHA256}:${A_55F27CB9TMP}"
}

# Main
pretestWork
clearFS
runTest
postTestWork
getTestResults
ls "/var/lib/samba/usershares/gdrive" -altr 2>/dev/null
exit $FAILEDTESTS
