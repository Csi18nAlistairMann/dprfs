#!/bin/bash
source ./common.sh

TESTNAME='Can rename a directory, then create a new directory of same name, without second seeing first dir contents'

# This test necessary after discovering that a new directory with a
# previously used name would have the contents of that previous
# directory
# ie, mkdir <previously used name>

function runTest()
{
    mkdir $GDRIVE$ORIGINALDIR
    local FIRSTDIR=$(find $RDRIVE${ORIGINALDIR}-* | head -1)
    echo 'hello world' >$GDRIVE$ORIGINALDIR/$FILE
    mv $GDRIVE$ORIGINALDIR $GDRIVE$RENAMEDTODIR
    mkdir $GDRIVE$ORIGINALDIR
    touch $GDRIVE$RENAMEDTODIR/$TESTFILEORIGINALDIR
    touch $GDRIVE$ORIGINALDIR/$TESTFILERENAMEDTODIR
}

function runTest_corrective()
{
    # Originally we could automate this part...
    mkdir $GDRIVE$ORIGINALDIR
    local FIRSTDIR=$(find $RDRIVE${ORIGINALDIR}-* | head -1)
    echo 'hello world' >$GDRIVE$ORIGINALDIR/$FILE
    mv $GDRIVE$ORIGINALDIR $GDRIVE$RENAMEDTODIR
    mkdir $GDRIVE$ORIGINALDIR

    # ... but then we'd have to make these corrections manually

    local FIRSTDIRLASTDMETADATA=$(find $FIRSTDIR/:Dmetadata-* | tail -1 | sed "s#$FIRSTDIR/##")
    local FIRSTDIRORIGINALDIR=$(find $FIRSTDIR | head -1 | sed "s#$RDRIVE##")
    local FIRSTDIRSECONDLASTDMETADATA=$(find $FIRSTDIR/:Dmetadata-* | tail -2 | head -1 | sed "s#$FIRSTDIR/##")

    mv $FIRSTDIR/$FIRSTDIRLASTDMETADATA $RDRIVE$ORIGINALDIR/
    printf "mv $FIRSTDIR/$FIRSTDIRLASTDMETADATA $RDRIVE$ORIGINALDIR/\n"
    ln -sf $FIRSTDIRLASTDMETADATA $RDRIVE$ORIGINALDIR/:Dmetadata
    ln -sf $FIRSTDIRSECONDLASTDMETADATA $FIRSTDIR/:Dmetadata

    local SECONDDIRLASTDMETADATA=$(find $RDRIVE$RENAMEDTODIR/:Dmetadata-* | tail -1)
    local TMPFILE=$(mktemp /tmp/`basename "$0"`.XXXXXX)

    cat $SECONDDIRLASTDMETADATA | sed "s#${ORIGINALDIR}#${FIRSTDIRORIGINALDIR}#g" >$TMPFILE
    mv "$TMPFILE" "$SECONDDIRLASTDMETADATA"

    # prove
    touch $GDRIVE$ORIGINALDIR/$TESTFILEORIGINALDIR
    touch $GDRIVE$RENAMEDTODIR/$TESTFILERENAMEDTODIR
}

function getTestResults()
{
    testStringEqual "GDRIVE" "${DIFF_GDRIVE}" "^(\t${GDRIVE}${ORIGINALDIR})\n\1/${TESTFILERENAMEDTODIR}\n(\t${GDRIVE}${RENAMEDTODIR})\n\2/${FILE}\n\2/${TESTFILEORIGINALDIR}\n$"
# '        /var/lib/samba/usershares/gdrive/test021.sh-ORIGINAL
#         /var/lib/samba/usershares/gdrive/test021.sh-ORIGINAL/test021.shthis-is-renamed-dir
#         /var/lib/samba/usershares/gdrive/test021.sh-RENAMED
#         /var/lib/samba/usershares/gdrive/test021.sh-RENAMED/file_test021.sh
#         /var/lib/samba/usershares/gdrive/test021.sh-RENAMED/test021.shthis-is-original-dir'

    testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${ORIGINALDIR})\n\g{1}(-[0-9]{20})\n(\g{1}\g{2}/:Dmetadata)\n\g{3}-[0-9]{20}\n\g{3}-[0-9]{20}\n\g{3}-[0-9]{20}\n(\g{1}\g{2}/${FILE})\n(\g{4}/AA00000)(-[0-9]{20})\n\g{5}\g{6}/${FILE}\n\g{5}\g{6}/:Fmetadata\n\g{5}\g{6}/:Fmetadata\g{6}\n\g{4}/:latest\n\g{1}\g{2}/${TESTFILEORIGINALDIR}\n(\g{1}\g{2}/${TESTFILEORIGINALDIR}/AA00000)(-[0-9]{20})\n\g{7}\g{8}/:Fmetadata\n\g{7}\g{8}/:Fmetadata\g{8}\n\g{7}\g{8}/${TESTFILEORIGINALDIR}\n\g{1}\g{2}/${TESTFILEORIGINALDIR}/:latest\n\g{1}(-[0-9]{20})\n\g{1}\g{9}/:Dmetadata\n\g{1}\g{9}/:Dmetadata-[0-9]{20}\n\g{1}\g{9}/:Dmetadata-[0-9]{20}\n\g{1}\g{9}/${TESTFILERENAMEDTODIR}\n(\g{1}\g{9}/${TESTFILERENAMEDTODIR}/AA00000)(-[0-9]{20})\n\g{10}\g{11}/:Fmetadata\n\g{10}\g{11}/:Fmetadata\g{11}\n\g{10}\g{11}/${TESTFILERENAMEDTODIR}\n\g{1}\g{9}/${TESTFILERENAMEDTODIR}/:latest\n\g{1}/:Dmetadata\n\g{1}/:Dmetadata-[0-9]{20}\n\g{1}/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${RENAMEDTODIR})\n\g{12}/:Dmetadata\n\g{12}/:Dmetadata-[0-9]{20}\n$"
# '        /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/:Dmetadata-20170706005052142400
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/:Dmetadata-20170706005052143566
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/:Dmetadata-20170706005052156885
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/file_test021.sh
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/file_test021.sh/AA00000-20170706005052149519
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/file_test021.sh/AA00000-20170706005052149519/file_test021.sh
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/file_test021.sh/AA00000-20170706005052149519/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/file_test021.sh/AA00000-20170706005052149519/:Fmetadata-20170706005052149519
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/file_test021.sh/:latest
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/test021.shthis-is-original-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/test021.shthis-is-original-dir/AA00000-20170706005052166037
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/test021.shthis-is-original-dir/AA00000-20170706005052166037/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/test021.shthis-is-original-dir/AA00000-20170706005052166037/:Fmetadata-20170706005052166037
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/test021.shthis-is-original-dir/AA00000-20170706005052166037/test021.shthis-is-original-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052142113/test021.shthis-is-original-dir/:latest
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/:Dmetadata-20170706005052160147
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/:Dmetadata-20170706005052161364
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/test021.shthis-is-renamed-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/test021.shthis-is-renamed-dir/AA00000-20170706005052171907
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/test021.shthis-is-renamed-dir/AA00000-20170706005052171907/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/test021.shthis-is-renamed-dir/AA00000-20170706005052171907/:Fmetadata-20170706005052171907
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/test021.shthis-is-renamed-dir/AA00000-20170706005052171907/test021.shthis-is-renamed-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170706005052159912/test021.shthis-is-renamed-dir/:latest
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL/:Dmetadata-20170706005052143966
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL/:Dmetadata-20170706005052162292
#         /var/lib/samba/usershares/rdrive/test021.sh-RENAMED
#         /var/lib/samba/usershares/rdrive/test021.sh-RENAMED/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-RENAMED/:Dmetadata-20170706005052157237

    # testStringEqual "RDRIVE" "${DIFF_RDRIVE}" "^(\t${RDRIVE}${ORIGINALDIR})\n\1(-[0-9]{20})\n(\1\2/${FILE})\n(\3/AA00000)(-[0-9]{20})\n\4\5/${FILE}\n\4\5/:Fmetadata\n\4\5/:Fmetadata\5\n\3/:latest\n\1\2/:Dmetadata\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n\1\2/:Dmetadata-[0-9]{20}\n(\1\2/${TESTFILERENAMEDTODIR})\n(\6/AA00000)(-[0-9]{20})\n\7\8/:Fmetadata\n\7\8/:Fmetadata\8\n\7\8/${TESTFILERENAMEDTODIR}\n\6/:latest\n\1(-[0-9]{20})\n\1\9/:Dmetadata\n\1\9/:Dmetadata-[0-9]{20}\n\1\9/:Dmetadata-[0-9]{20}\n(\1\9/${TESTFILEORIGINALDIR})\n(\g{10}/AA00000)(-[0-9]{20})\n\g{11}\g{12}/:Fmetadata\n\g{11}\g{12}/:Fmetadata\g{12}\n\g{11}\g{12}/${TESTFILEORIGINALDIR}\n\g{10}/:latest\n\1/:Dmetadata\n\1/:Dmetadata-[0-9]{20}\n\1/:Dmetadata-[0-9]{20}\n(\t${RDRIVE}${RENAMEDTODIR})\n\g{13}/:Dmetadata\n\g{13}/:Dmetadata-[0-9]{20}\n$"
# '        /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/a_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/a_77585946cd986dda071f476978703ce_file/AA00000-20170621232100340803
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/a_77585946cd986dda071f476978703ce_file/AA00000-20170621232100340803/a_77585946cd986dda071f476978703ce_file
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/a_77585946cd986dda071f476978703ce_file/AA00000-20170621232100340803/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/a_77585946cd986dda071f476978703ce_file/AA00000-20170621232100340803/:Fmetadata-20170621232100340803
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/a_77585946cd986dda071f476978703ce_file/:latest
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/:Dmetadata-20170621232100335791
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/:Dmetadata-20170621232100336349
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/:Dmetadata-20170621232100345170
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/this-is-renamed-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/this-is-renamed-dir/AA00000-20170621232100353407
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/this-is-renamed-dir/AA00000-20170621232100353407/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/this-is-renamed-dir/AA00000-20170621232100353407/:Fmetadata-20170621232100353407
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/this-is-renamed-dir/AA00000-20170621232100353407/this-is-renamed-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100335715/this-is-renamed-dir/:latest
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/:Dmetadata-20170621232100347549
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/:Dmetadata-20170621232100348138
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/this-is-original-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/this-is-original-dir/AA00000-20170621232100350450
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/this-is-original-dir/AA00000-20170621232100350450/:Fmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/this-is-original-dir/AA00000-20170621232100350450/:Fmetadata-20170621232100350450
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/this-is-original-dir/AA00000-20170621232100350450/this-is-original-dir
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL-20170621232100347496/this-is-original-dir/:latest
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL/:Dmetadata-20170621232100336525
#         /var/lib/samba/usershares/rdrive/test021.sh-ORIGINAL/:Dmetadata-20170621232100348318
#         /var/lib/samba/usershares/rdrive/test021.sh-RENAMED
#         /var/lib/samba/usershares/rdrive/test021.sh-RENAMED/:Dmetadata
#         /var/lib/samba/usershares/rdrive/test021.sh-RENAMED/:Dmetadata-20170621232100345349'

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
    FILE='file_'`basename "$0"`
    ORIGINALDIR=`basename "$0"`'-ORIGINAL'
    TESTFILEORIGINALDIR=`basename "$0"`'this-is-original-dir'
    RENAMEDTODIR=`basename "$0"`'-RENAMED'
    TESTFILERENAMEDTODIR=`basename "$0"`'this-is-renamed-dir'
    clearFS
    FAILEDTESTS=0
    NUMTESTS=0
}

function clearFS()
{
    checkAndRemove "${RDRIVE}${ORIGINALDIR}"
    checkAndRemove "${RDRIVE}${ORIGINALDIR}-"*
    checkAndRemove "${RDRIVE}${ORIGINALDIR}-"*
    checkAndRemove "${RDRIVE}${RENAMEDTODIR}"
}

# Main
pretestWork
runTest
postTestWork
getTestResults
clearFS
exit $FAILEDTESTS
