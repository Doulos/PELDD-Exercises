#!/bin/bash

HOLD_LINE_DURATION=12 

echo -n "Powering down board"
hidusb-relay-cmd on 1
for (( i=$HOLD_LINE_DURATION; i>0; i-- ))
do
	echo -n "."
	sleep 1
done
hidusb-relay-cmd off 1
echo "done"
