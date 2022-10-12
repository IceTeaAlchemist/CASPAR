#!/usr/bin/env bash

mkdir -p /home/pi/Documents/githubs/CASPAR/data/serverlog
mylog=/home/pi/Documents/githubs/CASPAR/data/serverlog/casparRun.txt

echo " " >> ${mylog}
echo "===================================================" >> ${mylog}
echo "==  server start `date` ==" >> ${mylog}   
echo "===================================================" >> ${mylog}
#     0123456789012345678901234567890123456789012345678901234567890
# date is 31 chars long.
# tee -a appends to the document.
cd /home/pi/Documents/githubs/CASPAR
sudo node caspar.js  | tee -a data/serverlog/casparRun.txt

