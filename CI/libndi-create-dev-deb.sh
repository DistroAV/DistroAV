#!/bin/bash

# This script is called by CI/libndi-package.sh

set -ex

SDK_ROOT="/tmp/ndisdk"
INCLUDES_PATH="${SDK_ROOT}/include"

cp ${INCLUDES_PATH}/*.h /usr/include/
