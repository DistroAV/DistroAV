#!/bin/sh

set -e

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[obs-ndi - Error] macOS package script can be run on Darwin-type OS only."
    exit 1
fi

echo "[obs-ndi] Preparing package build"
export QT_CELLAR_PREFIX="$(/usr/bin/find /usr/local/Cellar/qt -d 1 | sort -t '.' -k 1,1n -k 2,2n -k 3,3n | tail -n 1)"

GIT_HASH=$(git rev-parse --short HEAD)
GIT_BRANCH_OR_TAG=$(git name-rev --name-only HEAD | awk -F/ '{print $NF}')

VERSION="$GIT_HASH-$GIT_BRANCH_OR_TAG"
LATEST_VERSION="$GIT_BRANCH_OR_TAG"

FILENAME_UNSIGNED="obs-ndi-$VERSION-Unsigned.pkg"
FILENAME="obs-ndi-$VERSION.pkg"

RELEASE_MODE=0
if [[ $BRANCH_FULL_NAME =~ ^refs\/tags\/ ]]; then
	KEYCHAIN_PASSWORD=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 64 | head -n 1)

	# Setup keychain (I have to admit, this is almost straight up copied from OBS' CI)
	hr "Decrypting Cert"
	openssl aes-256-cbc -K $CERTIFICATES_KEY -iv $CERTIFICATES_IV -in ../CI/macos/Certificates.p12.enc -out Certificates.p12 -d
	hr "Creating Keychain"
	security create-keychain -p $KEYCHAIN_PASSWORD build.keychain
	security default-keychain -s build.keychain
	security unlock-keychain -p $KEYCHAIN_PASSWORD build.keychain
	security set-keychain-settings -t 3600 -u build.keychain
	hr "Importing certs into keychain"
	security import ./Certificates.p12 -k build.keychain -T /usr/bin/productsign -P ""
	# macOS 10.12+
	security set-key-partition-list -S apple-tool:,apple: -s -k $KEYCHAIN_PASSWORD build.keychain

	# Enable release mode
	RELEASE_MODE=1
fi

echo "[obs-ndi] Modifying obs-ndi.so"
install_name_tool \
	-change /usr/local/opt/qt/lib/QtWidgets.framework/Versions/5/QtWidgets \
		@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets \
	-change /usr/local/opt/qt/lib/QtGui.framework/Versions/5/QtGui \
		@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui \
	-change /usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore \
		@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore \
	./build/obs-ndi.so

# Check if replacement worked
echo "[obs-ndi] Dependencies for obs-ndi"
otool -L ./build/obs-ndi.so

if [[ "$RELEASE_MODE" == "1" ]]; then
	echo "[obs-ndi] Signing plugin binary: obs-ndi.so"
	codesign --sign "$CODE_SIGNING_IDENTITY" ./build/obs-ndi.so
else
	echo "[obs-ndi] Skipped plugin codesigning"
fi

echo "[obs-ndi] Actual package build"
packagesbuild ./installer/obs-ndi.pkgproj

echo "[obs-ndi] Renaming obs-ndi.pkg to $FILENAME"
mkdir release
mv ./installer/build/obs-ndi.pkg ./release/$FILENAME_UNSIGNED

if [[ "$RELEASE_MODE" == "1" ]]; then
	echo "[obs-ndi] Signing installer: $FILENAME"
	productsign \
		--sign "$INSTALLER_SIGNING_IDENTITY" \
		./release/$FILENAME_UNSIGNED \
		./release/$FILENAME

	echo "[obs-ndi] Submitting installer $FILENAME for notarization"
	zip -r ./release/$FILENAME.zip ./release/$FILENAME
	xcrun altool \
		--notarize-app \
		--primary-bundle-id "fr.palakis.obs-ndi"
		--username $AC_USERNAME
		--password $AC_PASSWORD
		--asc-provider $AC_PROVIDER_SHORTNAME
		--file ./release/$FILENAME.zip

	rm ./release/$FILENAME_UNSIGNED ./release/$FILENAME.zip
else
	echo "[obs-ndi] Skipped installer codesigning and notarization"
fi
