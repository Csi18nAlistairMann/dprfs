#!/bin/bash
#
# Can PUT a file in /shared
# then delete it
# and can't GET it
#
source credents
TMP_DIR="/tmp"
TMP_FILE="1005file.txt-old"
TMP_FILE_CONTENTS="Hello, world!\n"
COMMANDS1="put $TMP_DIR/$TMP_FILE $TMP_FILE"
COMMANDS2="del $TMP_FILE"
COMMANDS3="get $TMP_FILE"

RESPONSE_SHOULD_CONTAIN="NT_STATUS_OBJECT_NAME_NOT_FOUND opening remote file \\"$TMP_FILE

rm -rf /tmp/1005file.txt-old /tmp/1005file.txt-new /var/lib/samba/usershares/rdrive/1005file.txt-old
echo $TMP_FILE_CONTENTS > $TMP_DIR/$TMP_FILE

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
if [ "$RESPONSE" == "$RESPONSE_SHOULD_CONTAIN" ];
then
    echo "ok!"
else
    echo "error $COMMANDS3"
    echo $RESPONSE
    exit -1;
fi



