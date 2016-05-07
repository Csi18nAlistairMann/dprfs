#!/bin/bash
#
# Can mkdir /shared/dir
#
source credents
COMMANDS="mkdir dir6"

RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "$COMMANDS" 2>/dev/null)
if [ "$RV" == "" ];
then
    echo "ok!"
else
    echo "error"
    echo $RESPONSE
fi
