#!/usr/bin/env bash
set -ex

#export QT_PREFIX="$(find /usr/local/Cellar/qt5 -d 1 | tail -n 1)"

mkdir build && cd build
cmake .. \
  -DQTDIR=/usr/local/opt/qt \
  -DLIBOBS_INCLUDE_DIR=../../obs-studio/libobs \
  -DLIBOBS_LIB=../../obs-studio/libobs \
  -DOBS_FRONTEND_LIB="$(pwd)/../../obs-studio/build/UI/obs-frontend-api/libobs-frontend-api.dylib" \
  -DCMAKE_INSTALL_PREFIX=/usr \

