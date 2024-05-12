#!/bin/bash
set -e

LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v6_Linux"
LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"
LIBNDI_INSTALLER_URL=https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER

# Use temporary directory
LIBNDI_TMP=$(mktemp --tmpdir -d ndidisk.XXXXXXX)

# Check if the temp directory exists and is a directory.
if [[ -d "$LIBNDI_TMP" ]]; then
    echo "Temporary directory created at $LIBNDI_TMP"
else
    echo "Failed to create a temporary directory."
    exit 1
fi

# While most of the command are with the folder path, this is needed for the libndi install script to run properly
pushd $LIBNDI_TMP

# Download LIBNDI
# The follwoing should work with tmp folder in the user home directory - but not always... So we do not use it.
# curl -o "$LIBNDI_TMP/$LIBNDI_INSTALLER" $LIBNDI_INSTALLER_URL -f --retry 5

# The following is required if the temp directory is not in the user home directory.
curl -L $LIBNDI_INSTALLER_URL -f --retry 5 > "$LIBNDI_TMP/$LIBNDI_INSTALLER"


# Check if download was successful
if [ $? -ne 0 ]; then
    echo "Download failed."
    exit 1
fi

echo "Download complete."

# Step 3: Uncompress the file.
echo "Uncompressing..."
tar -xzvf "$LIBNDI_TMP/$LIBNDI_INSTALLER" -C "$LIBNDI_TMP"

# Check if uncompression was successful
if [ $? -ne 0 ]; then
    echo "Uncompression failed."
    exit 1
fi

echo "Uncompression complete."


yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh


rm -rf $LIBNDI_TMP/ndisdk
echo "Moving things to a folder with no space"
mv "$LIBNDI_TMP/NDI SDK for Linux" $LIBNDI_TMP/ndisdk
echo
echo "Contents of $LIBNDI_TMP/ndisdk/lib:"
ls -la $LIBNDI_TMP/ndisdk/lib
echo
echo "Contents of $LIBNDI_TMP/ndisdk/lib/x86_64-linux-gnu:"
ls -la $LIBNDI_TMP/ndisdk/lib/x86_64-linux-gnu
echo

popd

if [ "$1" == "install" ]; then
    echo "Copying the library files to the long-term location. You might be prompted for authentication."
    sudo cp -P $LIBNDI_TMP/ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
    sudo ldconfig

    echo "libndi installed to /usr/local/lib"
    ls -la /usr/local/lib/libndi*
    echo
fi

# Allow to keep the temporary files (to use with libndi-package.sh)
if [ "$1" == "nocleanup" ]; then
    echo "No Clean-up requested."
else
    echo "Clean-up : Removing temporary folder"
    rm -rf $LIBNDI_TMP
    if [[ ! -d "$LIBNDI_TMP" ]]; then
        echo "Temporary directory $LIBNDI_TMP does not exist anymore (good!)"
    else
        echo "Failed to clean-up temporary directory."
        echo "Please clean this up manully - All should be in $LIBNDI_TMP"
        exit 1
    fi
fi
