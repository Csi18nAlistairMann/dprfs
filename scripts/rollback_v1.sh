#!/bin/bash
#
# rollback_v1
#
# This will be the main script for a DESTRUCTIVE
# rollback. Any dprfs object discovered to have the same
# or later timestamp than the time of poisoning gets
# remove()d.
#
source ./rollback_v1_lib.sh

poisoned_ts=$1
poisoned_path="/var/lib/samba/usershares/rdrive"
rollback_v1 "$poisoned_path" "$poisoned_ts"
