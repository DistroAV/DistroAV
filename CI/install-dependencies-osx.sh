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

# OBS Studio + obs-ndi deps
brew install jack speexdsp ccache swig mbedtls
brew install https://gist.githubusercontent.com/DDRBoxman/b3956fab6073335a4bf151db0dcbd4ad/raw/ed1342a8a86793ea8c10d8b4d712a654da121ace/qt.rb

wget --quiet --retry-connrefused --waitretry=1 https://obs-nightly.s3.amazonaws.com/osx-deps-2018-08-09.tar.gz
tar -xf ./osx-deps-2018-08-09.tar.gz -C /tmp

# Build OBS Studio
git clone --recursive https://github.com/jp9000/obs-studio.git
cd ./obs-studio
git checkout 23.2.1
mkdir build && cd build
cmake .. \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 \
    -DDepsPath=/tmp/obsdeps \
    -DCMAKE_PREFIX_PATH=/usr/local/opt/qt/lib/cmake \
    -DENABLE_SCRIPTING=OFF \
&& make -j4
