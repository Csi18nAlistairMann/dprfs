#!/bin/bash
#
# Can mkdir /shared/dir/subdir/subsubdir
#
source credents
COMMANDS="mkdir dir"
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS" 2>/dev/null)
if [ "$RV" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
fi
COMMANDS="mkdir dir/subdir"
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS" 2>/dev/null)
if [ "$RV" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
fi
COMMANDS="mkdir dir/subdir/subsubdir"
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS" 2>/dev/null)
if [ "$RV" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
fi
