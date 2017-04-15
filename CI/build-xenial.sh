#!/bin/sh
set -ex

cd /root/obs-websocket

mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR="../../obs-studio/libobs" -DNDISDK_DIR="../../ndisdk" -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
