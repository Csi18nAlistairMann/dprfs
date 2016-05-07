#!/bin/bash
#
# rollback_v1 library routines
#

# Function to get the byte length of a (unicode) string
# $1 unicode string whose length we need
#
# Supports use of unicode in filesystem paths
function internal_unicode_string_len()
{
    un_str=$1
    lang=$LANG
    LANG=C
    paf_sz=${#un_str}
    LANG=$lang
    echo $paf_sz
}

# Destructive rollback of a single linkedlist
# $1 /path/to
# $2 linkedlist name
# $3 first timestamp known to be Pete Tong
function rollback_linkedlist()
{
    local llpath="$1"
    local llname="$2"
    local poisoned_timestamp="$3"
    local paf="$llpath/$llname"
    local paf_sz=$(internal_unicode_string_len "$paf")
    ((paf_sz+=2)) # pointer: di[r]/linkedlist/ to dir/[l]inkedlist/
    local object=""
    local mdobject=""
    local filerevision=""
    local filetimestamp=""
    local mdrevision=""
    local mdtimestamp=""
    local leftbehind_count=0
    local latest_revisiontimestamp="AA00000-00000000000000000000"
    local md_paf_sz=$(internal_unicode_string_len "$paf/$latest_revisiontimestamp")
    ((md_paf_sz+=2))
    local rv=0

    # Look through all available links for this linkedlist
    # Not sure why * matches hyphen, ? doesn't and nor does - or \-, -- etc
    # When goes wrong, object ends up matching the literal regex string
    for object in "$paf"/[A-ZA-Z0-90-90-90-90-9]*[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]
    do
	filerevision=`echo "$object" | cut -c $paf_sz- | cut -c -7`
	filetimestamp=`echo "$object" | cut -c $paf_sz- | cut -c 9-`

	if [[ ! "$filetimestamp" < "$poisoned_timestamp" ]]
	then
	    # Remove the link if on or after the poisoning date
	    rm -f "$object/:Fmetadata"*
	    rm -f "$object/$llname"
	    rmdir "$object"
	    rv=1
	else
	    # Although this directory is earlier than the poisoning,
	    # handle things if containing :Fmetadata present that's
	    # later
	    for mdobject in "$paf/$filerevision-$filetimestamp/:Fmetadata-"[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]
	    do
		mdtimestamp=`echo "$mdobject" | cut -c $md_paf_sz- | cut -c 12-`
		if [[ ! "$mdtimestamp" < "$poisoned_timestamp" ]]
		then
		    # Remove the :Fmetadta if on or after the poisoning date
		    rm -f "$mdobject/$mdtimestamp/$md"
		    rv=1
		fi
	    done

	    # note how many links not deleted, also the latest
	    # revision-timestamp as it'll form the new head
	    ((leftbehind_count++))
	    if [[ "$filerevision-$filetimestamp" > "$latest_revisiontimestamp" ]]
	    then
		latest_revisiontimestamp="$filerevision-$filetimestamp"
	    fi
	fi
    done

    # Any links left earlier than the poisoning date?
    rm -f "$paf/:latest"
    if [[ $leftbehind_count -eq 0 ]]
    then
	# no: remove the linkedlist itself
	rmdir "$paf"
	rv=1
    else
	# yes: point :latest at the new head
	ln -s "$latest_revisiontimestamp" "$paf/:latest"
    fi
    return $rv
}

# Destructive rollback of a single directory
# $1 /path/to/dir
# $2 first timestamp known to be Pete Tong
function rollback_directory()
{
    local lldir="$1"
    local lldir_sz=$(internal_unicode_string_len "$lldir")
    ((lldir_sz+=13)) # pointer: di[r]/:Dmetadata-2016 > dir/:Dmetadata-[2]016
    local poisoned_timestamp="$2"
    local object=""
    local filetimestamp=""
    local leftbehind_count=0
    local latest_revisiontimestamp="00000000000000000000"
    local is_dprfs_directory=0
    local rv=0

    # Look through all available :Dmetadatas for this directory
    for object in "$lldir"/:Dmetadata-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]
    do
	filetimestamp=`echo "$object" | cut -c $lldir_sz-`

	if [[ ! "$filetimestamp" < "$poisoned_timestamp" ]]
	then
	    # Remove the :Dmetadata-* if on or after the poisoning date
	    rm -f "$object"
	    rv=1
	else
	    # count how many :Dmetadata-* not deleted, also get
	    # the latest timestamp for the new head
	    ((leftbehind_count++))
	    if [[ "$filetimestamp" > "$latest_revisiontimestamp" ]]
	    then
		latest_revisiontimestamp="$filetimestamp"
	    fi
	fi
    done

    # Is this a dprfs directory or not? We need to know in order
    # to remove/restore the :Dmetadata softlink
    if [[ -h "$lldir/:Dmetadata" ]]
    then
	is_dprfs_directory=1
	rm -f "$lldir/:Dmetadata"
    fi

    # Any :Dmetadata-* left earlier than the poisoning date?
    if [[ $leftbehind_count -eq 0 ]]
    then
	# no: remove directory itself
	rmdir "$lldir"
	rv=1
    else
	# yes: point :Dmetadata at the new head
	if [[ $is_dprfs_directory -ne 0 ]]
	then
	    ln -s ":Dmetadata-$latest_revisiontimestamp" "$lldir/:Dmetadata"
	fi
    fi
    return $rv
}

# Initiate destructive rollback of dprfs elements
# $1 /path/to/rdrive
# $2 timestamp of the first known poisoned element
function rollback_v1()
{
    local rootpath="$1"
    local rootpath_sz=$(internal_unicode_string_len "$rootpath")
    ((rootpath_sz+=2)) # pointer: .../rdriv[e]/* to .../rdrive/[*]
    local poisoned_timestamp="$2"
    local llname=""
    local object=""
    local result=0

    # recurse through the filesystem below this point, handling
    # files, linkedlists and directories as appropriate
    for object in "$rootpath/"* "$rootpath/."*
    do
	if [[ "$object" == "$rootpath/.." ]]
	then
	    continue
	fi
	if [[ "$object" == "$rootpath/." ]]
	then
	    continue
	fi
	llname=`echo "$object" | cut -c $rootpath_sz-`
	result=0

	if [ -d "$object" ]
	then
	    if [ -e "$object/:latest" ]
	    then
		rollback_linkedlist "$rootpath" "$llname" "$poisoned_timestamp"
		result=$?
	    else
		# recurse down to clean subdirectories before
		# cleaning this one
		echo "    $object [recurse]"
		rollback_v1 "$object" "$poisoned_timestamp"
		((result|=$?))
		rollback_directory "$object" "$poisoned_timestamp"
		((result|=$?))
	    fi
	# else
	    # Could be :Dmetadata*, the /Beyond Use/files etc
	fi
	if [[ $result -eq 0 ]]
	then
	    echo "[ ] $object"
	else
	    echo "[X] $object"
	fi
    done
}
