#!/bin/sh

LIBS_PATH="/root/ndisdk/lib/x86_64-linux-gnu"

chmod +x "$LIBS_PATH/*.so"
cp "$LIBS_PATH/*.so" /usr/lib/
