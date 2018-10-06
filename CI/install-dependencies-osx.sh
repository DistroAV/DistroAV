#!/usr/bin/env bash

# Exit if something fails
set -e
# Echo all commands before executing
set -v

cd ../

# Install Packages app so we can build a package later
# http://s.sudre.free.fr/Software/Packages/about.html
wget --retry-connrefused --waitretry=1 https://s3-us-west-2.amazonaws.com/obs-nightly/Packages.pkg
sudo installer -pkg ./Packages.pkg -target /

brew update
# OBS Studio + obs-websocket deps
brew install ffmpeg libav
brew install ./obs-ndi/CI/macos/qt.rb

# Build OBS Studio
git clone --recursive https://github.com/jp9000/obs-studio.git
cd ./obs-studio
git checkout 21.0.0
mkdir build && cd build
cmake .. \
    -DCMAKE_PREFIX_PATH=/usr/local/opt/qt/lib/cmake \
&& make -j4

cd ../../obs-ndi
