#!/bin/bash
set -e

LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v5_Linux"
LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"

#sudo apt-get install curl

pushd /tmp

curl -L -o $LIBNDI_INSTALLER https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER -f --retry 5
tar -xf $LIBNDI_INSTALLER
yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh

rm -rf /tmp/ndisdk
mv "/tmp/NDI SDK for Linux" /tmp/ndisdk
ls /tmp/ndisdk

if [ $1 == "install" ]; then
    # NOTE: This should do an actual local install...
    sudo cp -P /tmp/ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
    sudo ldconfig
fi

popd
