#!/bin/sh
set -ex

LIBS_PATH="/tmp/NDI SDK for Linux/lib/x86_64-linux-gnu"
cp "$LIBS_PATH/libndi*" /usr/lib/

LATEST_LIB=$(ls /usr/lib/libndi* | sort -r | head -1)

chmod +x /usr/lib/libndi*
