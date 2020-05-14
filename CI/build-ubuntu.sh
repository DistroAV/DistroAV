#!/bin/sh
set -ex

mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DUSE_UBUNTU_FIX=true ..
make -j4
