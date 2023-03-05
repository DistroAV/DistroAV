#!/bin/bash
set -ex

SDK_ROOT="/tmp/ndisdk"
INCLUDES_PATH="${SDK_ROOT}/include"

cp ${INCLUDES_PATH}/*.h /usr/include/
