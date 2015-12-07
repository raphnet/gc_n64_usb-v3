#!/bin/bash

TOOL=./tool/gcn64ctl

if [ -x $TOOL ]; then
	$TOOL -f --bootloader
	exit 0 # The tool fails, but we should continue
else
	echo $TOOL not found. Please compile it to enter bootloader automatically.
fi
