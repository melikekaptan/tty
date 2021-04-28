#!/bin/bash

#call cmake

cmake .

#call make
FILE=./Makefile
if test -f "$FILE"; then
	echo "$FILE exsist"
	make
else
	echo "$FILE do not exsist"
fi

sudo chmod 777 module
#call make for kernel module
FILE=./module/Makefile
if test -f "$FILE"; then
	echo "$FILE exsist"
	cd ./module/
	make
	echo "testing.."
	make test
	sudo dmesg -C
	sudo insmod ttyM.ko
	sudo chmod 777 /dev/ttyM
	echo "kernel module loaded. /dev/ttyM is ready to use. To remove run : sudo rmmod ttyM.ko"
else
	echo "$FILE do not exsist"
fi



#load kernel module test
