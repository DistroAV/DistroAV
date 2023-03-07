#!/bin/bash

set -e

INSTALL=false

while [ -n "$1" ]
do
case "$1" in
        -i) INSTALL=true ;;
esac
shift
done

LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v5_Linux"
LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"
LIBNDI_INSTALLER_SHA256="00d0bedc2c72736d82883fc0fd6bc1a544e7958c7e46db79f326633d44e15153"

pushd /tmp
#sudo apt-get install curl
curl -L -o $LIBNDI_INSTALLER https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER -f --retry 5
echo "$LIBNDI_INSTALLER_SHA256 $LIBNDI_INSTALLER" | sha256sum -c
tar -xf $LIBNDI_INSTALLER
yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh

rm -rf ndisdk
mv "NDI SDK for Linux" ndisdk

if [ "$INSTALL" == true ] ; then
    echo sudo cp -P ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
    echo sudo ldconfig
    echo libndi installed to /usr/local/lib/
    ls -la /usr/local/lib/libndi*
    rm -rf ndisdk
else
    echo NDI SDK copied to `pwd`/ndisdk
    ls -la ndisdk
fi

popd
