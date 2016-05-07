#!/bin/bash
#
# List shares on the remote machine
# Expect to see shared, readonly and test shares
#
printf "========\nRunning %s\n" `basename "$0"`
source credents
RESPONSE=$(smbclient -L $HOSTNAME -N 2> /dev/null)
RVOUT=0;

RV=$(echo $RESPONSE | grep -c shared)
if [ $RV -ne 0 ]
then 
    printf "ok!\n"
else
    printf "error\n"
    (( RVOUT++ ))
fi

RV=$(echo $RESPONSE | grep -c readonly)
if [ $RV -ne 0 ]
then 
    printf "ok!\n"
else
    printf "error\n"
    (( RVOUT++ ))
fi

RV=$(echo $RESPONSE | grep -c test)
if [ $RV -ne 0 ]
then 
    printf "ok!\n"
else
    printf "error\n"
    (( RVOUT++ ))
fi

RV=$(echo $RESPONSE | grep -c goosnargh)
if [ $RV -eq 0 ]
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
