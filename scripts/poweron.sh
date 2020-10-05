#!/bin/bash

echo Powering on board...
hidusb-relay-cmd on 1
sleep 1
hidusb-relay-cmd off 1
