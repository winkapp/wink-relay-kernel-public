#!/bin/bash
export ARCH=arm
export CROSS_COMPILE=prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-
make $1
make
[[ "$?" != "0" ]] && echo "$0 make: e r r o r" && exit -1
