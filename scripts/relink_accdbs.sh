#!/bin/bash
#
# relink_accdbs.sh
#
# When run, this script makes softlinks in /tmp/dprfs/ to
# each <file>.accdb file found in /home/smb/accdbs.
#
# ".accdb" is a filetype of Microsoft Access, and is 
# treated as a datastore, not a file. As these don't 
# fit the dprfs model, we do something else: we store 
# the file via the /tmp/dprfs directory - which means 
# it can be persisted without dprfs, but softlinked 
# to /home/smb/dprfs where it'll survive reboot.
# As /tmp/dprfs is flushed, we need to rebuild those
# links.

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

function link_item()
{
    local from="$1"
    local to="$2"

    if [[ -e $to ]]
    then
	unlink "$to"
    fi
    ln -s "$from" "$to"
}


tmp_dir="/tmp/dprfs/"
persistent_dir="/home/smb/accdb/"
persistent_dir_sz=$(internal_unicode_string_len "$persistent_dir")
((persistent_dir_sz++)) # [/]file .. /[f]ile
for object in "$persistent_dir"*
do
    file_sz=$(internal_unicode_string_len "$object")
    ((file_sz-=5))
    file=`echo "$object" | cut -c $persistent_dir_sz-`
    ext=`echo "$object" | cut -c $file_sz-`
    if [[ "$ext" == ".accdb" ]]
    then
	link_item "$object" "$tmp_dir$file"
	echo "[x] $file"
    else
	echo "[ ] $file"
    fi
done
