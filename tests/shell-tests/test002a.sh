#!/bin/bash
source ./common.sh

TESTNAME='Can create a subsubdirectory'

# Test necessary after discovering original-dir directive not containing
# the correct path

function runTest()
{
    mkdir $GDRIVE$PARENTDIR
    mkdir $GDRIVE$PARENTDIR/$CHILDDIR
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^(\t${GDRIVE}${PARENTDIR})\n\1/${CHILDDIR}\n$"
#'        /var/lib/samba/usershares/gdrive/test002a.sh_parent
#        /var/lib/samba/usershares/gdrive/test002a.sh_parent/test002a.sh_child'

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${PARENTDIR})\n\1(-[0-9]{20})\n(\1\2/:Dmetadata)\n\3-[0-9]{20}\n\3-[0-9]{20}\n(\1\2/${CHILDDIR})\n(\4-[0-9]{20})\n\5/:Dmetadata\n\5/:Dmetadata-[0-9]{20}\n\5/:Dmetadata-[0-9]{20}\n\4/:Dmetadata\n\4/:Dmetadata-[0-9]{20}\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}$"

# '        /var/lib/samba/usershares/rdrive/test002a.sh_parent
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/:Dmetadata-20170630064858028818
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/:Dmetadata-20170630064858029362
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child-20170630064858032148
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child-20170630064858032148/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child-20170630064858032148/:Dmetadata-20170630064858032335
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child-20170630064858032148/:Dmetadata-20170630064858033090
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent-20170630064858028749/test002a.sh_child/:Dmetadata-20170630064858033244
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test002a.sh_parent/:Dmetadata-20170630064858029531'

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
    PARENTDIR=`basename "$0"`'_parent'
    CHILDDIR=`basename "$0"`'_child'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${PARENTDIR}-"*
    checkAndRemove "${RDRIVE}${PARENTDIR}"*
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
