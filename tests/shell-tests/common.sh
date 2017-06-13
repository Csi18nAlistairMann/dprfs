#!/bin/bash
function establishAllTestsGlobals()
{
    # Constants for all tests
    GDRIVE='/var/lib/samba/usershares/gdrive/'
    RDRIVE='/var/lib/samba/usershares/rdrive/'
    TDRIVE='/tmp/dprfs/'
}

function establishFSBefore()
{
    # Capture FS before the test
    GDRIVE_BEFORE=`find $GDRIVE | sort`
    RDRIVE_BEFORE=`find $RDRIVE | sort`
    TDRIVE_BEFORE=`find $TDRIVE 2>/dev/null | sort`
}

function establishFSAfter()
{
    # Capture FS after the test
    GDRIVE_AFTER=`find $GDRIVE | sort`
    RDRIVE_AFTER=`find $RDRIVE | sort`
    TDRIVE_AFTER=`find $TDRIVE 2>/dev/null | sort`
}

function establishFSChanges()
{
    # Discover new entries made as a result of the test
    DIFF_GDRIVE=`comm -3 <(printf "$GDRIVE_BEFORE") <(printf "$GDRIVE_AFTER")`
    DIFF_RDRIVE=`comm -3 <(printf "$RDRIVE_BEFORE") <(printf "$RDRIVE_AFTER")`
    DIFF_TDRIVE=`comm -3 <(printf "$TDRIVE_BEFORE") <(printf "$TDRIVE_AFTER")`

    }

function pretestWork()
{
    establishAllTestsGlobals
    establishThisTestGlobals
    establishFSBefore
}

function postTestWork()
{
    establishFSAfter
    establishFSChanges
}

function checkAndRemove()
{
    if [ -d $1 ]
    then
	printf "Remove: $1\n"
	rm -Ir $1
    fi
}

function testStringEmpty()
{
    ((NUMTESTS++))
    if [ "$2" != "" ]
    then
	((FAILEDTESTS++))
	printf "[FAIL] $1 should be empty, found '$2'\n"
	return 1
    fi
    return 0
}

function testStringEqual()
{
    ((NUMTESTS++))
    local rv=$(echo "$2" | pcregrep -c -M -N ANY "$3" -)
    if [ "$rv" -ne "1" ]
    then
	((FAILEDTESTS++))
	printf "[FAIL] $1 has unknown match '$2'\n"
	return 1
    fi
}
