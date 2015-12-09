#!/bin/bash

TOOL=./tools/gcn64ctl

if [ -x $TOOL ]; then
	$TOOL -f --bootloader
	exit 0 # The tool fails, but we should continue anyway in case the chip is already in bootloader
else
	echo $TOOL not found. Please compile it to enter bootloader automatically.
fi
