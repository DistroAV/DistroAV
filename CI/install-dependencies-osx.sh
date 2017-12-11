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
# OBS Studio deps
brew install ffmpeg libav
# OBS and obs-websocket common dependancy
brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/2b121c9a96e58a5da14228630cb71d5bead7137e/Formula/qt.rb

#echo "Qt path: $(find /usr/local/Cellar/qt5 -d 1 | tail -n 1)"

# Build OBS Studio
git clone --recursive https://github.com/jp9000/obs-studio.git
cd ./obs-studio
git checkout 20.1.0
mkdir build && cd build
cmake .. \
    -DCMAKE_PREFIX_PATH=/usr/local/opt/qt/lib/cmake \
&& make -j4

cd ../../obs-ndi