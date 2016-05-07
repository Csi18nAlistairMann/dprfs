#!/bin/bash
#
# Can mkdir /shared/dir1
# Then rename it to /shared/dir2
#
source credents
DIR1="dir1"
DIR2="dir2"
COMMANDS1="mkdir "$DIR1
COMMANDS2="rename "$DIR1" "$DIR2
COMMANDS3="ls /"
rm -rf /var/lib/samba/usershares/rdrive/dir1 /var/lib/samba/usershares/rdrive/dir2
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS1" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS2" 2>/dev/null)
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS3" 2>/dev/null)
RV_DIR1=$(echo $RESPONSE | grep -c $DIR1)
if [ $RV_DIR1 -eq 0 ];
then
    echo "ok!"
else
    echo $DIR1" error"
    echo $RESPONSE
    exit -1;
fi
RV_DIR2=$(echo $RESPONSE | grep -c $DIR2)
if [ $RV_DIR2 -eq 1 ];
then
    echo "ok!"
else
    echo $DIR2" error"
    echo " result: $RV_DIR2"
    echo $RESPONSE
    exit -1;
fi
