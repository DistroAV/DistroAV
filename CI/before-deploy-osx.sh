#!/usr/bin/env bash

cd ./build

# Package app
echo "Generating .pkg"
packagesbuild ../install/obs-ndi.pkgproj
mkdir /home/travis/package
mv ../install/build/obs-ndi.pkg /home/travis/package/
