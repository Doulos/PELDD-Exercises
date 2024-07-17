#!/bin/bash
echo pushbutton forever
while : 
do 
    hidusb-relay-cmd on 2 
    hidusb-relay-cmd off 2 
    sleep 0.1 
    echo .
done
