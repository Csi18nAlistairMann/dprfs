#!/bin/bash
source ./common.sh

TESTNAME='Can move a populated directory into another directory at the same level without losing the contents of the first'

# This test necessary after discovering that directories suddenly appeared
# empty after being moved.
#  mkdir "initial"
#  echo "cobblers" >"initial/testfile"
#  cat "initial/testfile"  # should show "cobblers"
#  mkdir "second"
#  mv initial second
#  ls second/initial/  # should show file but actually shows empty

function runTest()
{
    COBBLERS="cobblers"
    mkdir "${GDRIVE}${INITIAL}"
    echo "$COBBLERS" >"${GDRIVE}${INITIAL}/${FILE}"
    OUTPUT1=$(cat "${GDRIVE}${INITIAL}/${FILE}")
    if [ "$OUTPUT1" != "$COBBLERS" ]; then
	(($FAILEDTESTS))

    else
	mkdir "${GDRIVE}${SECOND}"
	mv "${GDRIVE}${INITIAL}" "${GDRIVE}${SECOND}"
	OUTPUT2=$(cat "${GDRIVE}${SECOND}/${INITIAL}/${FILE}")
	if [ "$OUTPUT2" != "$COBBLERS" ]; then
	    ((FAILEDTESTS++))
	fi
    fi

    # mkdir "${GDRIVE}${NEWF}"
    # mv "${GDRIVE}${NEWF}" ${GDRIVE}${B1}
    # touch ${GDRIVE}${B1}/${FILE}
    # mv ${GDRIVE}${B1} ${GDRIVE}${B2}
    # mkdir "${GDRIVE}${NEWF}"
    # mv "${GDRIVE}${NEWF}" "${GDRIVE}${B1}"
    # touch $GDRIVE$B2/$TESTFILEFIRSTDIR
    # touch $GDRIVE$B1/$TESTFILESECONDDIR
}

function getTestResults()
{
#    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^(\t${GDRIVE}${B1})\n\1/${TESTFILESECONDDIR}\n(\t${GDRIVE}${B2})\n\2/${FILE}\n\2/${TESTFILEFIRSTDIR}\n$";
# '        /var/lib/samba/usershares/gdrive/b1
#         /var/lib/samba/usershares/gdrive/b1/this-is-second-dir
#         /var/lib/samba/usershares/gdrive/b2
#         /var/lib/samba/usershares/gdrive/b2/test022.sha_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/gdrive/b2/this-is-first-dir'

#    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${B1})\n(\1/:Dmetadata)\n\2-[0-9]{20}\n\2-[0-9]{20}\n(\t${RDRIVE}${B2})\n\3/:Dmetadata\n\3/:Dmetadata-[0-9]{20}\n\t${RDRIVE}${NEWF}\n(\t${RDRIVE}${NEWF}-[0-9]{20})\n(\4/:Dmetadata)\n\5-[0-9]{20}\n\5-[0-9]{20}\n\5-[0-9]{20}\n\5-[0-9]{20}\n(\4/${FILE})\n(\6/AA00000)(-[0-9]{20})\n\7\8/:Fmetadata\n\7\8/:Fmetadata\8\n\7\8/${FILE}\n\6/:latest\n(\4/${TESTFILEFIRSTDIR})\n(\9/AA00000)(-[0-9]{20})\n\g{10}\g{11}/:Fmetadata\n\g{10}\g{11}/:Fmetadata\g{11}\n\g{10}\g{11}/${TESTFILEFIRSTDIR}\n\9/:latest\n(\t${RDRIVE}${NEWF}-[0-9]{20})\n(\g{12}/:Dmetadata)\n\g{13}-[0-9]{20}\n\g{13}-[0-9]{20}\n\g{13}-[0-9]{20}\n(\g{12}/${TESTFILESECONDDIR})\n(\g{14}/AA00000)(-[0-9]{20})\n\g{15}\g{16}/:Fmetadata\n\g{15}\g{16}/:Fmetadata\g{16}\n\g{15}\g{16}/${TESTFILESECONDDIR}\n\g{14}/:latest\n\t${RDRIVE}${NEWF}/:Dmetadata\n(\t${RDRIVE}${NEWF}/:Dmetadata)-[0-9]{20}\n\g{17}-[0-9]{20}$"

# '        /var/lib/samba/usershares/rdrive/b1
#         /var/lib/samba/usershares/rdrive/b1/:Dmetadata
#         /var/lib/samba/usershares/rdrive/b1/:Dmetadata-20170625151412801284
#         /var/lib/samba/usershares/rdrive/b1/:Dmetadata-20170625151412812473
#         /var/lib/samba/usershares/rdrive/b2
#         /var/lib/samba/usershares/rdrive/b2/:Dmetadata
#         /var/lib/samba/usershares/rdrive/b2/:Dmetadata-20170625151412805413
#         /var/lib/samba/usershares/rdrive/New folder
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/:Dmetadata
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/:Dmetadata-20170625151412793845
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/:Dmetadata-20170625151412794970
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/:Dmetadata-20170625151412801058
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/:Dmetadata-20170625151412805130
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/test022.sha_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/test022.sha_77585946cd986dda071f476978703ce_file/AA00000-20170625151412802670
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/test022.sha_77585946cd986dda071f476978703ce_file/AA00000-20170625151412802670/:Fmetadata
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/test022.sha_77585946cd986dda071f476978703ce_file/AA00000-20170625151412802670/:Fmetadata-20170625151412802670
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/test022.sha_77585946cd986dda071f476978703ce_file/AA00000-20170625151412802670/test022.sha_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/test022.sha_77585946cd986dda071f476978703ce_file/:latest
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/this-is-first-dir
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/this-is-first-dir/AA00000-20170625151412814139
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/this-is-first-dir/AA00000-20170625151412814139/:Fmetadata
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/this-is-first-dir/AA00000-20170625151412814139/:Fmetadata-20170625151412814139
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/this-is-first-dir/AA00000-20170625151412814139/this-is-first-dir
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412793711/this-is-first-dir/:latest
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/:Dmetadata
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/:Dmetadata-20170625151412808570
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/:Dmetadata-20170625151412809327
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/:Dmetadata-20170625151412812326
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/this-is-second-dir
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/this-is-second-dir/AA00000-20170625151412816297
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/this-is-second-dir/AA00000-20170625151412816297/:Fmetadata
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/this-is-second-dir/AA00000-20170625151412816297/:Fmetadata-20170625151412816297
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/this-is-second-dir/AA00000-20170625151412816297/this-is-second-dir
#         /var/lib/samba/usershares/rdrive/New folder-20170625151412808487/this-is-second-dir/:latest
#         /var/lib/samba/usershares/rdrive/New folder/:Dmetadata
#         /var/lib/samba/usershares/rdrive/New folder/:Dmetadata-20170625151412795338
#         /var/lib/samba/usershares/rdrive/New folder/:Dmetadata-20170625151412809448'

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
    # NEWF=`basename "$0"`"New folder"
    INITIAL=`basename "$0"`"Initial"
    SECOND=`basename "$0"`"Second"
    # B1=`basename "$0"`'_b1'
    # B2=`basename "$0"`'_b2'
    FILE=`basename "$0"`'a_77585946cd986dda071f476978703ce_file'
    # TESTFILEFIRSTDIR=`basename "$0"`'this-is-first-dir'
    # TESTFILESECONDDIR=`basename "$0"`'this-is-second-dir'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${INITIAL}"*
    checkAndRemove "${RDRIVE}${SECOND}"*
    checkAndRemove "${RDRIVE}${INITIAL}-"*
    checkAndRemove "${RDRIVE}${SECOND}-"*
    # checkAndRemove "${RDRIVE}${NEWF}-"*
    # checkAndRemove "${RDRIVE}${NEWF}-"*
    # checkAndRemove "${RDRIVE}${NEWF}"*
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
