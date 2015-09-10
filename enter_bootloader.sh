#!/bin/bash

TOOL=./tool/gcn64ctl

if [ -x $TOOL ]; then
	$TOOL -f --bootloader
else
	echo $TOOL not found. Please compile it to enter bootloader automatically.
fi
