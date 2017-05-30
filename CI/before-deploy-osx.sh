#!/usr/bin/env bash

export QT_PREFIX="$(brew --prefix qt5)"

mkdir package

cd ./installer

install_name_tool \
	-change "$QT_PREFIX/lib/QtWidgets.framework/Versions/5/QtWidgets" @rpath/QtWidgets \
	-change "$QT_PREFIX/lib/QtGui.framework/Versions/5/QtGui" @rpath/QtGui \
	-change "$QT_PREFIX/lib/QtCore.framework/Versions/5/QtCore" @rpath/QtCore \
	../build/libobs-ndi.so

# Package app
echo "Generating .pkg"
packagesbuild obs-ndi.pkgproj
mv ./build/obs-ndi.pkg ../package/
