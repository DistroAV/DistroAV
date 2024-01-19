#!/bin/bash
set -e

LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v5_Linux"
LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"

#sudo apt-get install curl

mkdir -p /tmp
pushd /tmp

curl -L -o $LIBNDI_INSTALLER https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER -f --retry 5
tar -xf $LIBNDI_INSTALLER
yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh

rm -rf ./ndisdk
mv "./NDI SDK for Linux" ./ndisdk
echo
echo "Contents of $(pwd)/ndisdk/lib:"
ls -la ./ndisdk/lib
echo
echo "Contents of $(pwd)/ndisdk/lib/x86_64-linux-gnu:"
ls -la ./ndisdk/lib/x86_64-linux-gnu
echo

popd

#if [ "$1" == "install" ]; then
    sudo cp -P /tmp/ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
    sudo ldconfig

    echo "libndi installed to /usr/local/lib"
    ls -la /usr/local/lib/libndi*
    echo
#fi
