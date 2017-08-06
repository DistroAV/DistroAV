#!/usr/bin/env bash

export QT_PREFIX="/usr/local/opt/qt5"
#export QT_PREFIX="$(find /usr/local/Cellar/qt -d 1 | tail -n 1)"

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
	-change "$QT_PREFIX/lib/QtWidgets.framework/Versions/5/QtWidgets" @rpath/QtWidgets \
	-change "$QT_PREFIX/lib/QtGui.framework/Versions/5/QtGui" @rpath/QtGui \
	-change "$QT_PREFIX/lib/QtCore.framework/Versions/5/QtCore" @rpath/QtCore \
	../build/obs-ndi.so

# Check if replacement worked
otool -l ../build/obs-ndi.so

# Package app
echo "Generating .pkg"
packagesbuild obs-ndi.pkgproj

mkdir ../package
mv ./build/obs-ndi.pkg ../package/$FILENAME
cp ../package/$FILENAME ../package/$LATEST_FILENAME
