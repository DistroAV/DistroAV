#!/bin/sh

LIB_PATH="/root/ndisdk/lib/x86_64-linux-gnu-4.7/libndi.so.1.0.1"

cp $LIB_PATH /usr/lib/
ln -s /usr/lib/libndi.so.1.0.1 /usr/lib/libndi.so.1
