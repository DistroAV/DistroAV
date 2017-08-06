#!/usr/bin/env bash

export GIT_HASH=$(git rev-parse --short HEAD)
export VERSION="$GIT_HASH-$TRAVIS_BRANCH"
export LATEST_VERSION="$TRAVIS_BRANCH"
if [ -n "${TRAVIS_TAG}" ]; then
	export VERSION="$TRAVIS_TAG"
	export LATEST_VERSION="$TRAVIS_TAG"
fi

export FILENAME="obs-ndi-$VERSION.pkg"
export LATEST_FILENAME="obs-ndi-latest-$LATEST_VERSION.pkg"

cd ./installer

install_name_tool \
	-change "/usr/local/opt/qt/lib/QtWidgets.framework/Versions/5/QtWidgets" @rpath/QtWidgets \
	-change "/usr/local/opt/qt/lib/QtGui.framework/Versions/5/QtGui" @rpath/QtGui \
	-change "/usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore" @rpath/QtCore \
	../build/obs-ndi.so

# Check if replacement worked
otool -L ../build/obs-ndi.so

# Package app
echo "Generating .pkg"
packagesbuild obs-ndi.pkgproj

mkdir ../package
mv ./build/obs-ndi.pkg ../package/$FILENAME
cp ../package/$FILENAME ../package/$LATEST_FILENAME
