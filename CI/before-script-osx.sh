#!/usr/bin/env bash

#export QT_PREFIX="$(find /usr/local/Cellar/qt5 -d 1 | tail -n 1)"
export QT_PREFIX="/usr/local/opt/qt"

mkdir build
cd build

LIBOBS_PATH=$(grealpath "../../obs-studio/build/rundir/Debug/bin/")"/libobs.0.dylib"
FRONTEND_PATH=$(grealpath "../../obs-studio/build/rundir/RelWithDebInfo/bin/")"/libobs-frontend-api.dylib"

cmake -DNDISDK_DIR="../../NDI SDK for Apple" -DLIBOBS_INCLUDE_DIR="../../obs-studio/libobs" \
    -DLIBOBS_LIB=$LIBOBS_PATH -DOBS_FRONTEND_LIB=$FRONTEND_PATH \
    -DQt5Core_DIR="$QT_PREFIX/lib/cmake/Qt5Core" \
    -DQt5Widgets_DIR="$QT_PREFIX/lib/cmake/Qt5Widgets" ../
