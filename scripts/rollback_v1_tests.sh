#!/bin/bash
#
# rollback_v1 tests
#
# Date of presumed poisoning: the stroke of midnight
# following 31 Jan 2016: 20160201000000000000
#
source rollback_v1_lib.sh

###################################################################
# library

# we could use functionform { but using functionform() { is apparently
# more portable. In which case the parenthesis need to be cleaned out
# before we use it
function internal_clean_function_name
{
    echo `echo "$1" | sed 's#()##'`
}

###################################################################
# set up for tests

TESTPATH="/tmp/dprfs/rollback-"`date +%s`
TESTPATH_SZ=$(internal_unicode_string_len "$TESTPATH")
((TESTPATH_SZ+=2)) # pointer: ...rollbac[k]/file > ...rollback/[f]ile
T_MINUS_CATCH_EVERYTHING=20160101000000000000
T_MINUS_ONE_MS=20160131235959999999
T=20160201000000000000
T_PLUS_ONE_MS=20160201000000000001
T_PLUS_CATCH_NOTHING=20161231235959999999

# Set up a linkedlist with one update: a payload and
# one :Fmetadata file
function setup_simple_linkedlist()
{
    local dirpath=$1
    local llname=$2
    local revision=$3
    local timestamp=$4

    mkdir -p "$dirpath/$llname/$revision-$timestamp"
    touch "$dirpath/$llname/$revision-$timestamp/$llname"
    touch "$dirpath/$llname/$revision-$timestamp/:Fmetadata-$timestamp"
    ln -s ":Fmetadata-$timestamp" "$dirpath/$llname/$revision-$timestamp/:Fmetadata"
}

# Setup a linkedlist with three updates
#
# AA00000 last known clean
# AA00001 first known poisoned
# AA00002 updated following poisoning
function setup_linkedlist_history()
{
    local dirpath=$1
    local llname=$2

    mkdir -p "$dirpath"
    touch "$dirpath/:Dmetadata-$T_MINUS_ONE_MS"
    ln -s ":Dmetadata-$T_MINUS_ONE_MS" "$dirpath/:Dmetadata"

    setup_simple_linkedlist "$dirpath" "$llname" "AA00000" "$T_MINUS_ONE_MS"
    setup_simple_linkedlist "$dirpath" "$llname" "AA00001" "$T"
    setup_simple_linkedlist "$dirpath" "$llname" "AA00002" "$T_PLUS_ONE_MS"
    ln -s "AA00002-$T_PLUS_ONE_MS" "$dirpath/$llname/:latest"
}

# Setup a dprfs directory with three updates
#
# 1) last known clean
# 2) first known poisoned
# 3) updated following poisoning
function setup_directory_history()
{
    local dirpath=$1
    local llname=$2

    mkdir -p "$dirpath"
    touch "$dirpath/:Dmetadata-$T_MINUS_ONE_MS"
    touch "$dirpath/:Dmetadata-$T"
    touch "$dirpath/:Dmetadata-$T_PLUS_ONE_MS"
    ln -s ":Dmetadata-$T_PLUS_ONE_MS" "$dirpath/:Dmetadata"
}

# Set up a simple dprfs hierarchy with three directories
# containing one linkedlist each
#
# /rootpath/dir/dir  last known clean
# /rootpath/dir      first known poisoned
# /rootpath/         updated following poisoning
function setup_simple_dprfs_hierarchy()
{
    local rootpath=$1
    local dirpath=$2
    local llname=$3

    mkdir -p "$dirpath/$rootpath/dir/dir"
    touch "$dirpath/$rootpath/dir/dir/:Dmetadata-$T_PLUS_ONE_MS"
    ln -s ":Dmetadata-$T_PLUS_ONE_MS" "$dirpath/$rootpath/dir/dir/:Dmetadata"
    touch "$dirpath/$rootpath/dir/:Dmetadata-$T"
    ln -s ":Dmetadata-$T" "$dirpath/$rootpath/dir/:Dmetadata"
    touch "$dirpath/$rootpath/:Dmetadata-$T_MINUS_ONE_MS"
    ln -s ":Dmetadata-$T_MINUS_ONE_MS" "$dirpath/$rootpath/:Dmetadata"

    setup_simple_linkedlist "$dirpath/$rootpath/dir/dir" "$llname" "AA00000" "$T_PLUS_ONE_MS"
    ln -s "AA00000-$T_PLUS_ONE_MS" "$dirpath/$rootpath/dir/dir/$llname/:latest"

    setup_simple_linkedlist "$dirpath/$rootpath/dir" "$llname" "AA00000" "$T"
    ln -s "AA00000-$T" "$dirpath/$rootpath/dir/$llname/:latest"

    setup_simple_linkedlist "$dirpath/$rootpath" "$llname" "AA00000" "$T_MINUS_ONE_MS"
    ln -s "AA00000-$T_MINUS_ONE_MS" "$dirpath/$rootpath/$llname/:latest"
}

# Non-dprfs objects may exist in the dprfs prototype
function setup_non_dprfs()
{
    local rootpath=$1
    local dirpath=$2

    mkdir -p "$dirpath/Beyond Use"
    touch "$dirpath/Beyond Use/statistics"
    chmod 440 "$dirpath/Beyond Use/statistics"
    touch "$dirpath/Beyond Use/Readme 1st.txt"
    chmod 660 "$dirpath/Beyond Use/Readme 1st.txt"
    touch "$dirpath/Beyond Use/Readme 1st.txt"
    chmod 660 "$dirpath/Beyond Use/Readme 1st.txt"
    touch "$dirpath/Beyond Use/MCHammer.u!touch\$this"
    chmod 0 "$dirpath/Beyond Use/MCHammer.u!touch\$this"
}

#
###################################################################
# Checking test ran as expected

# Check that the filesystem appears as it should
# $1 name of test being examined
# $2 The string that we should match, or "" if there should no objects
#    to find in the filesystem.
#
# We match based on the results of find(1) output: we remove the testing
# path as it may differ on other machines; we sort using byte order as
# other people's locales may alphabetic order differently; we remove
# newlines and get a base64 string of what's left. This makes
# string comparison more reliable
function test_check_output()
{
    local testname=$1
    local shouldbe=$2

    # If there should be no filesystem objects - perhaps because they
    # were rolled back out of existence - handle this first otherwise
    # the "result" commands below will choke
    if [[ "$shouldbe" == "" ]]
    then
	if [[ -e "$TESTPATH/$testname" ]]
	then
	    echo "-------------------------------------------------------"
	    echo "[$testname] Reports error: file unexpectedly present"
	    echo "$TESTPATH/$testname"
	    return 1
	else
	    return 0
	fi
    fi

    # Otherwise there should be files and dirs around - make sure they
    # match what we're expecting.
    local shouldbe=`echo "$2" | tr -d '\n' | base64 -w 0`
    local result=`find "$TESTPATH/$testname" | cut -c $TESTPATH_SZ- | LC_ALL=C sort | tr -d '\n' | base64 -w 0`
    if [ "$result" != "$shouldbe" ]
    then
	echo "-------------------------------------------------------"
	echo "[$testname] Reports error: result didn't match expected"
	echo "What found:"
	find "$TESTPATH/$testname" | sed 's#'"$TESTPATH"'/##' | LC_ALL=C sort
	echo "$shouldbe"
	echo
	echo "$result"
	return 1;
    fi
    return 0;
}

#
###################################################################
# Individual tests

#
# Three Tests can handle linkedlist in isolation

function test_rollback_linkedlist_when_nothing_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local llpath="$1/$fnname"
    local llname=$2
    local poisoned_timestamp="$T_PLUS_CATCH_NOTHING"

    setup_linkedlist_history "$llpath" "$llname"
    rollback_linkedlist "$llpath" "$llname" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/:Dmetadata
$fnname/:Dmetadata-20160131235959999999
$fnname/$llname
$fnname/$llname/:latest
$fnname/$llname/AA00000-20160131235959999999
$fnname/$llname/AA00000-20160131235959999999/:Fmetadata
$fnname/$llname/AA00000-20160131235959999999/:Fmetadata-20160131235959999999
$fnname/$llname/AA00000-20160131235959999999/$llname
$fnname/$llname/AA00001-20160201000000000000
$fnname/$llname/AA00001-20160201000000000000/:Fmetadata
$fnname/$llname/AA00001-20160201000000000000/:Fmetadata-20160201000000000000
$fnname/$llname/AA00001-20160201000000000000/$llname
$fnname/$llname/AA00002-20160201000000000001
$fnname/$llname/AA00002-20160201000000000001/:Fmetadata
$fnname/$llname/AA00002-20160201000000000001/:Fmetadata-20160201000000000001
$fnname/$llname/AA00002-20160201000000000001/$llname"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

function test_rollback_linkedlist_when_everything_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local llpath="$1/$fnname"
    local llname=$2
    local poisoned_timestamp=$T_MINUS_CATCH_EVERYTHING

    setup_linkedlist_history "$llpath" "$llname"
    rollback_linkedlist "$llpath" "$llname" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/:Dmetadata
$fnname/:Dmetadata-20160131235959999999"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

function test_rollback_linkedlist_when_some_thing_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local llpath="$1/$fnname"
    local llname=$2
    local poisoned_timestamp="$T"

    setup_linkedlist_history "$llpath" "$llname"
    rollback_linkedlist "$llpath" "$llname" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/:Dmetadata
$fnname/:Dmetadata-20160131235959999999
$fnname/$llname
$fnname/$llname/:latest
$fnname/$llname/AA00000-20160131235959999999
$fnname/$llname/AA00000-20160131235959999999/:Fmetadata
$fnname/$llname/AA00000-20160131235959999999/:Fmetadata-20160131235959999999
$fnname/$llname/AA00000-20160131235959999999/$llname"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

#
# Three tests can handle directory in isolation

function test_rollback_directory_when_nothing_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local llpath="$1/$fnname"
    local poisoned_timestamp="$T_PLUS_CATCH_NOTHING"

    setup_directory_history "$llpath"
    rollback_directory "$llpath" "$poisoned_timestamp"
    shouldbe="
$fnname
$fnname/:Dmetadata
$fnname/:Dmetadata-20160131235959999999
$fnname/:Dmetadata-20160201000000000000
$fnname/:Dmetadata-20160201000000000001"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

function test_rollback_directory_when_everything_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local llpath="$1/$fnname"
    local poisoned_timestamp="$T_MINUS_CATCH_EVERYTHING"

    setup_directory_history "$llpath"
    rollback_directory "$llpath" "$poisoned_timestamp"

    shouldbe=""
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

function test_rollback_directory_when_something_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local llpath="$1/$fnname"
    local poisoned_timestamp="$T"

    setup_directory_history "$llpath"
    rollback_directory "$llpath" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/:Dmetadata
$fnname/:Dmetadata-20160131235959999999"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

#
# Three tests can handle directories and linkedlists together

function test_rollback_when_nothing_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local rootpath="rootpath"
    local llpath="$1/$fnname"
    local llname=$2
    local poisoned_timestamp="$T_PLUS_CATCH_NOTHING"

    setup_simple_dprfs_hierarchy "$rootpath" "$llpath" "$llname"
    rollback_v1 "$llpath" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/$rootpath
$fnname/$rootpath/:Dmetadata
$fnname/$rootpath/:Dmetadata-20160131235959999999
$fnname/$rootpath/dir
$fnname/$rootpath/dir/:Dmetadata
$fnname/$rootpath/dir/:Dmetadata-20160201000000000000
$fnname/$rootpath/dir/dir
$fnname/$rootpath/dir/dir/:Dmetadata
$fnname/$rootpath/dir/dir/:Dmetadata-20160201000000000001
$fnname/$rootpath/dir/dir/$llname
$fnname/$rootpath/dir/dir/$llname/:latest
$fnname/$rootpath/dir/dir/$llname/AA00000-20160201000000000001
$fnname/$rootpath/dir/dir/$llname/AA00000-20160201000000000001/:Fmetadata
$fnname/$rootpath/dir/dir/$llname/AA00000-20160201000000000001/:Fmetadata-20160201000000000001
$fnname/$rootpath/dir/dir/$llname/AA00000-20160201000000000001/$llname
$fnname/$rootpath/dir/$llname
$fnname/$rootpath/dir/$llname/:latest
$fnname/$rootpath/dir/$llname/AA00000-20160201000000000000
$fnname/$rootpath/dir/$llname/AA00000-20160201000000000000/:Fmetadata
$fnname/$rootpath/dir/$llname/AA00000-20160201000000000000/:Fmetadata-20160201000000000000
$fnname/$rootpath/dir/$llname/AA00000-20160201000000000000/$llname
$fnname/$rootpath/$llname
$fnname/$rootpath/$llname/:latest
$fnname/$rootpath/$llname/AA00000-20160131235959999999
$fnname/$rootpath/$llname/AA00000-20160131235959999999/:Fmetadata
$fnname/$rootpath/$llname/AA00000-20160131235959999999/:Fmetadata-20160131235959999999
$fnname/$rootpath/$llname/AA00000-20160131235959999999/$llname"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

function test_rollback_when_everything_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local rootpath="rootpath"
    local llpath="$1/$fnname"
    local llname=$2
    local poisoned_timestamp="$T_MINUS_CATCH_EVERYTHING"

    setup_simple_dprfs_hierarchy "$rootpath" "$llpath" "$llname"
    rollback_v1 "$llpath" "$poisoned_timestamp"

    shouldbe="
$fnname"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?

}

function test_rollback_when_something_poisoned()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local rootpath="rootpath"
    local llpath="$1/$fnname"
    local llname=$2
    local poisoned_timestamp="$T"

    setup_simple_dprfs_hierarchy "$rootpath" "$llpath" "$llname"
    rollback_v1 "$llpath" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/$rootpath
$fnname/$rootpath/:Dmetadata
$fnname/$rootpath/:Dmetadata-20160131235959999999
$fnname/$rootpath/$llname
$fnname/$rootpath/$llname/:latest
$fnname/$rootpath/$llname/AA00000-20160131235959999999
$fnname/$rootpath/$llname/AA00000-20160131235959999999/:Fmetadata
$fnname/$rootpath/$llname/AA00000-20160131235959999999/:Fmetadata-20160131235959999999
$fnname/$rootpath/$llname/AA00000-20160131235959999999/$llname"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

#
# Non dprfs objects could be found

function test_rollback_can_handle_non_dprfs()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    local poisoned_timestamp="$T"
    local rootpath="rootpath"
    local llpath="$1/$fnname"

    setup_non_dprfs "$rootpath" "$llpath"
    rollback_v1 "$llpath" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/Beyond Use
$fnname/Beyond Use/MCHammer.u!touch\$this
$fnname/Beyond Use/Readme 1st.txt
$fnname/Beyond Use/statistics"
    test_check_output "$FUNCNAME" "$shouldbe"
    return $?
}

#
# Multiple tests for oddities in path and file names

function test_rollback_can_handle_char_core()
{
    local functionname=$1
    local fnname=$(internal_clean_function_name $functionname)
    local rootpath="rootpath"
    local llpath="$2/$fnname"
    local llname=$3
    local poisoned_timestamp="$T"

    setup_simple_dprfs_hierarchy "$rootpath" "$llpath" "$llname"
    rollback_v1 "$llpath" "$poisoned_timestamp"

    shouldbe="
$fnname
$fnname/$rootpath
$fnname/$rootpath/:Dmetadata
$fnname/$rootpath/:Dmetadata-20160131235959999999
$fnname/$rootpath/$llname
$fnname/$rootpath/$llname/:latest
$fnname/$rootpath/$llname/AA00000-20160131235959999999
$fnname/$rootpath/$llname/AA00000-20160131235959999999/:Fmetadata
$fnname/$rootpath/$llname/AA00000-20160131235959999999/:Fmetadata-20160131235959999999
$fnname/$rootpath/$llname/AA00000-20160131235959999999/$llname"
    test_check_output "$functionname" "$shouldbe"
    return $?
}

function test_rollback_can_handle_spaces()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_dollars()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_square_brackets()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_hashes()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_unicode()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_unicode_in_last()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    # testing unicode pointer arithmetic
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_single_right_parenthesis()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

function test_rollback_can_handle_single_left_parenthesis()
{
    local fnname=$(internal_clean_function_name $FUNCNAME)
    test_rollback_can_handle_char_core "$fnname" "$1" "$2" "$3"
    return $?
}

###################################################################
# main entry point

mkdir -p "$TESTPATH"
rv=0

# test the library routines
test_rollback_linkedlist_when_nothing_poisoned "$TESTPATH" "testll"; ((rv|=$?))
test_rollback_linkedlist_when_everything_poisoned "$TESTPATH" "testll"; ((rv|=$?))
test_rollback_linkedlist_when_some_thing_poisoned "$TESTPATH" "testll"; ((rv|=$?))

test_rollback_directory_when_nothing_poisoned "$TESTPATH" "testll"; ((rv|=$?))
test_rollback_directory_when_everything_poisoned "$TESTPATH" "testll"; ((rv|=$?))
test_rollback_directory_when_something_poisoned "$TESTPATH" "testll"; ((rv|=$?))

test_rollback_when_nothing_poisoned "$TESTPATH" "testll"; ((rv|=$?))
test_rollback_when_everything_poisoned "$TESTPATH" "testll"; ((rv|=$?))
test_rollback_when_something_poisoned "$TESTPATH" "testll"; ((rv|=$?))

# test ordinary files in dprfs tree
test_rollback_can_handle_non_dprfs "$TESTPATH"; ((rv|=$?))

# test odd chars in filenames
test_rollback_can_handle_spaces "$TESTPATH" "Hell o w orld"; ((rv|=$?))
test_rollback_can_handle_dollars "$TESTPATH" "Hello\$world"; ((rv|=$?))
test_rollback_can_handle_square_brackets "$TESTPATH" "Hel[lowo]rld"; ((rv|=$?))
test_rollback_can_handle_hashes "$TESTPATH" "Helloworld #1"; ((rv|=$?))
test_rollback_can_handle_unicode "$TESTPATH" "Hello▶world"; ((rv|=$?))
test_rollback_can_handle_unicode_in_last "$TESTPATH" "Helloworld▶"; ((rv|=$?))
test_rollback_can_handle_single_right_parenthesis "$TESTPATH" "Hello) world"; ((rv|=$?))
test_rollback_can_handle_single_left_parenthesis "$TESTPATH" "Hello (world"; ((rv|=$?))

if [ $rv -eq 0 ]
then
    printf "Tests end, nothing spotted going wrong\n"
else
    printf "1 or more tests failed\n"
fi

rm -rf "$TESTPATH"

exit $rv
