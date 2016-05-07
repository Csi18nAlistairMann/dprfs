#!/bin/bash
#
# Can see in the shared directory 
#
printf "========\nRunning %s\n" `basename "$0"`

source credents
RESPONSE=$(smbclient $HOSTNAME'/shared' -U $USERNAME $PASSWORD -c "ls /" 2>/dev/null)
RVOUT=0;

RV=$(echo $RESPONSE | grep -c "blocks available")
if [ $RV -eq 1 ];
then
    printf "ok!\n"
else
    printf "error\n"
    (( RVOUT++ ))
fi

if [ $RVOUT -eq 0 ]
then
    printf " All tests pass\n"
else
    printf " Errors found\n"
fi

exit $RVOUT
