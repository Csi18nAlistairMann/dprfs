#!/bin/bash
#
# Can PUT a file in /shared/dir/subdir
# then rename it
# then GET it
#
source credents
TMP_DIR="/tmp"
REMOTE_DIR1="/dir"
REMOTE_DIR2="/subdir"
TMP_FILE1="1004file.txt-old"
TMP_FILE2="1004file.txt-new"
TMP_FILE_CONTENTS="Hello, world!\n"

COMMANDS1="mkdir $REMOTE_DIR1"
COMMANDS2="mkdir $REMOTE_DIR1/$REMOTE_DIR2"
COMMANDS3="put $TMP_DIR/$TMP_FILE1 $REMOTE_DIR1/$REMOTE_DIR2/$TMP_FILE1"
COMMANDS4="rename $REMOTE_DIR1/$REMOTE_DIR2/$TMP_FILE1 $REMOTE_DIR1/$REMOTE_DIR2/$TMP_FILE2"
COMMANDS5="get $REMOTE_DIR1/$REMOTE_DIR2/$TMP_FILE2 $TMP_DIR/$TMP_FILE2"
COMMANDS6="ls /$REMOTE_DIR1/$REMOTE_DIR2/*"

# Filename, A=msdos: file ready for archiving, 16=filesize
RESPONSE_SHOULD_CONTAIN="$TMP_FILE A 16"

rm -rf /tmp/1004file.txt-old /tmp/1004file.txt-new /var/lib/samba/usershares/rdrive/1004file.txt-old
echo $TMP_FILE_CONTENTS > $TMP_DIR/$TMP_FILE1

RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS1" 2>/dev/null)
if [ "$RV" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS2" 2>/dev/null)
if [ "$RV" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS3" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error $COMMANDS3"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS4" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error $COMMANDS4"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS5" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error $COMMANDS5"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(diff $TMP_DIR"/"$TMP_FILE1 $TMP_DIR"/"$TMP_FILE2)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error: file retrieved doesn't match file uploaded"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS6" 2>/dev/null)
RV_FILE1=$(echo $RESPONSE | grep -c $TMP_FILE1)
if [ $RV_FILE1 -eq 0 ];
then
    echo "ok!"
else
    echo $TMP_FILE1" error"
    echo $RESPONSE
    exit -1;
fi
RV_FILE2=$(echo $RESPONSE | grep -c $TMP_FILE2)
if [ $RV_FILE2 -eq 1 ];
then
    echo "ok!"
else
    echo $TMP_FILE2" error"
    echo $RESPONSE
    exit -1;
fi
