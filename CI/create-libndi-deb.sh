#!/bin/sh

LIBS_PATH="/root/ndisdk/lib/x86_64-linux-gnu"

chmod +x $LIBS_PATH/libndi*
cp $LIBS_PATH/libndi* /usr/lib/
ln -sf $(ls /usr/lib/libndi* | sort -r | head -1) /usr/lib/libndi.so