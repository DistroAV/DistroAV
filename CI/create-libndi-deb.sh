#!/bin/sh
set -ex

LIBS_PATH="/tmp/ndisdk/lib/x86_64-linux-gnu"
cp $LIBS_PATH/libndi.so* /usr/lib/

chmod +x /usr/lib/libndi.so*
