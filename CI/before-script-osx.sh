#!/usr/bin/env bash

mkdir build
cd build

LIBOBS_PATH=$(grealpath "../../obs-studio/build/rundir/Debug/bin/")"/libobs.0.dylib"
FRONTEND_PATH=$(grealpath "../../obs-studio/build/rundir/RelWithDebInfo/bin/")"/libobs-frontend-api.dylib"

cmake -DNDISDK_DIR="../../NDI SDK for Apple" -DLIBOBS_INCLUDE_DIR="../../obs-studio/libobs" -DLIBOBS_LIB=$LIBOBS_PATH -DOBS_FRONTEND_LIB=$FRONTEND_PATH -DQt5Core_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Core -DQt5Widgets_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Widgets ../