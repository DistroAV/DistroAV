#!/bin/bash
set -ex

SDK_ROOT="/tmp/ndisdk"
ARCH="x86_64-linux-gnu"

LIBS_PATH="${SDK_ROOT}/lib/${ARCH}"
BIN_PATH="${SDK_ROOT}/bin/${ARCH}"

chmod +x ${LIBS_PATH}/*
cp ${LIBS_PATH}/* /usr/lib/

chmod +x $BIN_PATH/*
cp ${BIN_PATH}/* /usr/bin/
