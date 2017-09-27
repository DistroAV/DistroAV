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
brew install qt5
#echo "Qt path: $(find /usr/local/Cellar/qt5 -d 1 | tail -n 1)"

# Fetch and untar prebuilt OBS deps that are compatible with older versions of OSX
wget --retry-connrefused --waitretry=1 https://s3-us-west-2.amazonaws.com/obs-nightly/osx-deps.tar.gz
tar -xf ./osx-deps.tar.gz -C /tmp

curl -kLO "https://slepin.fr/obs-ndi/ci/ndisdk-slim-v3-apple.zip" -f --retry 5
ls
unzip "./ndisdk-slim-v3-apple.zip"

git clone --recursive https://github.com/jp9000/obs-studio.git
cd ./obs-studio
git checkout 20.0.1
mkdir build
cd build
cmake -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 -DDepsPath=/tmp/obsdeps  ..
make -j4

cd ../../obs-ndi