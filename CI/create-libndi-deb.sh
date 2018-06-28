#!/bin/sh
set -ex

LIBS_PATH="/root/ndisdk/lib/x86_64-linux-gnu"
cp $LIBS_PATH/libndi* /usr/lib/

LATEST_LIB=$(ls /usr/lib/libndi* | sort -r | head -1)

ln -sf $LATEST_LIB /usr/lib/libndi.so
ln -sf $LATEST_LIB /usr/lib/libndi.so.3
chmod +x /usr/lib/libndi*
