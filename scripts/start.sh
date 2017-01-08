#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Syntax: ./start.sh CPU"
	exit 1;
fi

CPU=$1

echo "Polling for chip..."
while true; do
	dfu-programmer $1 start
	if [ $? -eq 0 ]; then
		echo "Chip found. Started."
		break;
	fi

	sleep 1
done


