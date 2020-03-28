#!/bin/sh

set -e

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[obs-ndi - Error] macOS package script can be run on Darwin-type OS only."
    exit 1
fi

echo "[obs-ndi] Preparing package build"
export QT_CELLAR_PREFIX="$(/usr/bin/find /usr/local/Cellar/qt -d 1 | sort -t '.' -k 1,1n -k 2,2n -k 3,3n | tail -n 1)"

export GIT_HASH=$(git rev-parse --short HEAD)
export GIT_BRANCH_OR_TAG=$(git name-rev --name-only HEAD | awk -F/ '{print $NF}')

export VERSION="$GIT_HASH-$GIT_BRANCH_OR_TAG"
export LATEST_VERSION="$GIT_BRANCH_OR_TAG"

export FILENAME="obs-ndi-$VERSION.pkg"

echo "[obs-ndi] Modifying obs-ndi.so"
install_name_tool \
	-add_rpath @executable_path/../Frameworks/QtWidgets.framework/Versions/5/ \
	-add_rpath @executable_path/../Frameworks/QtGui.framework/Versions/5/ \
	-add_rpath @executable_path/../Frameworks/QtCore.framework/Versions/5/ \
	-change /usr/local/opt/qt/lib/QtWidgets.framework/Versions/5/QtWidgets @rpath/QtWidgets \
	-change /usr/local/opt/qt/lib/QtGui.framework/Versions/5/QtGui @rpath/QtGui \
	-change /usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore @rpath/QtCore \
	./build/obs-ndi.so

# Check if replacement worked
echo "[obs-ndi] Dependencies for obs-ndi"
otool -L ./build/obs-ndi.so

echo "[obs-ndi] Actual package build"
packagesbuild ./installer/obs-ndi.pkgproj

echo "[obs-ndi] Renaming obs-ndi.pkg to $FILENAME"
mv ./release/obs-ndi.pkg ./release/$FILENAME
