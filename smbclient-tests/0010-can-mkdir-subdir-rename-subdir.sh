#!/bin/bash
#
# Can mkdir /shared/dir
# mkdir /shared/dir/subdir1
# Then rename subdir to /shared/dir/subdir2
#
source credents
DIR="dir"
SDIR1="subdir1"
SDIR2="subdir2"
COMMANDS1="mkdir $DIR/"
COMMANDS2="mkdir $DIR/"$SDIR1
COMMANDS3="rename $DIR/"$SDIR1" $DIR/"$SDIR2
COMMANDS4="ls /$DIR/*"
rm -rf /var/lib/samba/usershares/rdrive/dir
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
if [ "$RESPONSE" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
    exit -1;
fi
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS4" 2>/dev/null)
RV_SDIR1=$(echo $RESPONSE | grep -c $SDIR1)
if [ $RV_SDIR1 -eq 0 ];
then
    echo "ok!"
else
    echo $SDIR1" error"
    echo $RESPONSE
    exit -1;
fi
RV_SDIR2=$(echo $RESPONSE | grep -c $SDIR2)
if [ $RV_SDIR2 -eq 1 ];
then
    echo "ok!"
else
    echo $SDIR2" error"
    echo " result: $RV_DIR2"
    echo $RESPONSE
    exit -1;
fi
