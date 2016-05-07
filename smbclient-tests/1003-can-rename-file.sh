#!/bin/bash
#
# Can PUT a file in /shared
# then rename it
# then GET it
#
source credents
TMP_DIR="/tmp"
TMP_FILE1="1003file.txt-old"
TMP_FILE2="1003file.txt-new"
TMP_FILE_CONTENTS="Hello, world!\n"
COMMANDS1="put $TMP_DIR/$TMP_FILE1 $TMP_FILE1"
COMMANDS2="rename $TMP_FILE1 $TMP_FILE2"
COMMANDS3="get $TMP_FILE2 $TMP_DIR/$TMP_FILE2"

# Filename, A=msdos: file ready for archiving, 16=filesize
RESPONSE_SHOULD_CONTAIN="$TMP_FILE A 16"

rm -rf /tmp/1003file.txt-old /tmp/1003file.txt-new /var/lib/samba/usershares/rdrive/1003file.txt-old
echo $TMP_FILE_CONTENTS > $TMP_DIR/$TMP_FILE1

RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS1" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error $COMMANDS1"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS2" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error $COMMANDS2"
    echo $RESPONSE
    exit -1;
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



RESPONSE=$(diff $TMP_DIR"/"$TMP_FILE1 $TMP_DIR"/"$TMP_FILE2)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error: file retrieved doesn't match file uploaded"
    echo $RESPONSE
    exit -1;
fi
