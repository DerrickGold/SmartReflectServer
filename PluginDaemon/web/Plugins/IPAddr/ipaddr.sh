#!/bin/bash

#get the current ip address of the machine
iface=$(ifconfig wlan0)

if [ $? -ne 0 ]; then
  iface=$(ifconfig eth0)
fi

if [ $? -ne 0 ]; then
  echo "Error grabbing IP Address..."
  exit 1
fi

echo "$iface" | egrep -o "addr:([0-9]{1,3}[\.]?){4}" | sed 's/addr://g'
exit 0