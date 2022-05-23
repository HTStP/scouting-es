#!/bin/sh
#
# This script will start Scouting Data-Taking Software
#

cd ../src

echo "Clearing caches..."
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null

umask 000 # Files and folders we create should be world accessible

while true 
do
    echo "Starting scdaq..."
    ./scdaq 2>&1 | logger --tag scdaq --id -p user.debug
    echo "Resetting the board..."
    ../scripts/reset-firmware.sh
    echo "Clearing caches..."
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
    echo "Waiting 30 seconds..."
    for ((i=30; i>0; i=i-15)) 
    do
	echo "$i..."
	sleep 15
    done
done
