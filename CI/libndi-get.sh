#!/bin/bash

set -e

LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v5_Linux"
LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"
LIBNDI_INSTALLER_SHA256="00d0bedc2c72736d82883fc0fd6bc1a544e7958c7e46db79f326633d44e15153"

#sudo apt-get install curl

pushd /tmp
curl -L -o $LIBNDI_INSTALLER https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER -f --retry 5
echo "$LIBNDI_INSTALLER_SHA256 $LIBNDI_INSTALLER" | sha256sum -c
tar -xf $LIBNDI_INSTALLER
yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh

rm -rf /tmp/ndisdk
mv "/tmp/NDI SDK for Linux" /tmp/ndisdk
ls /tmp/ndisdk

# NOTE: This does an actual local install...
#sudo cp -P ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
#sudo ldconfig

popd
