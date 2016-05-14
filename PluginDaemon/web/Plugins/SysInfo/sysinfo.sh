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

ipaddr=$(echo "$iface" | egrep -o "addr:([0-9]{1,3}[\.]?){4}" | sed 's/addr://g')


cpuTemp0=$(cat /sys/class/thermal/thermal_zone0/temp)
cpuTemp1=$(($cpuTemp0/1000))
cpuTemp2=$(($cpuTemp0/100))
cpuTempM=$(($cpuTemp2 % $cpuTemp1))

gpuTemp=$(/opt/vc/bin/vcgencmd measure_temp | sed 's/temp=//g')

echo '<div id="infoTitle">System Information:</div>'
echo '<div class="infoParam">IP Address:</div> <div class="infoValue">'$ipaddr'</div>'
echo '<div class="infoParam">CPU Temp:</div> <div class="infoValue">'$cpuTemp1.$cpuTempM' C</div>'
echo '<div class="infoParam">GPU Temp:</div> <div class="infoValue">'$gpuTemp'</div>'

exit 0

