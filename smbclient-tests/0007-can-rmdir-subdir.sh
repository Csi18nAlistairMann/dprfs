#!/bin/bash
#
# Can mkdir /shared/dir/subdir
# Then rmdir /shared/dir/subdir
#
source credents
COMMANDS1="mkdir dir"
COMMANDS2="mkdir dir/subdir"
COMMANDS3="rmdir dir/subdir"
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
