#!/bin/bash
#
# Can mkdir /shared/dir/subdir
#
source credents
COMMANDS1="mkdir dir"
COMMANDS2="mkdir dir/subdir"
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
