#!/usr/bin/env bash

export GIT_HASH=$(git rev-parse --short HEAD)
export GIT_BRANCH_OR_TAG=$(git name-rev --name-only HEAD | awk -F/ '{print $NF}')

export VERSION="$GIT_HASH-$GIT_BRANCH_OR_TAG"
export LATEST_VERSION="$GIT_BRANCH_OR_TAG"

export FILENAME="obs-ndi-$VERSION.pkg"
export LATEST_FILENAME="obs-ndi-latest-$LATEST_VERSION.pkg"

cd ./installer

install_name_tool \
	-add_rpath @executable_path/../Frameworks/QtWidgets.framework/Versions/5/ \
	-add_rpath @executable_path/../Frameworks/QtGui.framework/Versions/5/ \
	-add_rpath @executable_path/../Frameworks/QtCore.framework/Versions/5/ \
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
