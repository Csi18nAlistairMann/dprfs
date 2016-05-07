#!/bin/bash
#
# Can PUT a file in /shared
#
source credents
TMP_DIR="/tmp"
TMP_FILE="1001file.txt"
TMP_FILE_CONTENTS="Hello, world!\n"
COMMANDS1="put $TMP_DIR/$TMP_FILE $TMP_FILE"
COMMANDS2="ls /$DIR/*"
# Filename, A=msdos: file ready for archiving, 16=filesize
RESPONSE_SHOULD_CONTAIN="$TMP_FILE A 16"

rm -rf /tmp/1001file.txt /var/lib/samba/usershares/rdrive/1001file.txt
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
RV=$(echo $RESPONSE | grep -c "$RESPONSE_SHOULD_CONTAIN") 
if [ $RV -eq 1 ];
then
    echo "ok!"
else
    echo " error $COMMANDS2"
    echo $RESPONSE
    exit -1;
fi
