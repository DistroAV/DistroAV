#!/usr/bin/env bash

mkdir package

cd ./installer

# Package app
echo "Generating .pkg"
packagesbuild obs-ndi.pkgproj
mv ./build/obs-ndi.pkg ../package/
