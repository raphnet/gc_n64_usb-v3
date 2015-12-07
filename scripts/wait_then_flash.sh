#!/bin/bash

if [ $# -ne 2 ]; then
	echo "Syntax: ./wait_then_flash.sh CPU HEXFILE"
	exit 1;
fi

CPU=$1
HEXFILE=$2

echo "Will program $HEXFILE on $CPU target"
echo "Polling for chip..."
while true; do
	dfu-programmer $1 erase
	if [ $? -eq 0 ]; then
		echo "Chip found. Erased."
		break;
	fi

	sleep 1
done

echo "Writing flash..."
dfu-programmer $CPU flash $HEXFILE

echo "Starting program..."
dfu-programmer $CPU start
