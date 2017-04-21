#!/bin/sh

set -e

cd /root/obs-ndi

export GIT_HASH=$(git rev-parse --short HEAD)
export PKG_VERSION="1-$GIT_HASH-$TRAVIS_BRANCH-git"

if [ -n "${TRAVIS_TAG}" ]; then
	export PKG_VERSION="$TRAVIS_TAG"
fi

cd /root/obs-ndi/build

PAGER=cat checkinstall -y --type=debian --fstrans=no --nodoc \
	--backup=no --deldoc=yes --install=no \
	--pkgname=obs-ndi --pkgversion="$PKG_VERSION" \
	--pkglicense="LGPLv2.1" --maintainer="contact@slepin.fr" \
	--requires="libndi" --pkggroup="video" \
	--pkgsource="https://github.com/Palakis/obs-ndi" \
	--pakdir="/package"

chmod ao+r /package/*

ls -lh /package
