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
	--pkglicense="GPLv2" --maintainer="stephane.lepin@gmail.com" \
	--requires="libndi3 (>= 3.5.1)" --pkggroup="video" \
	--pkgsource="https://github.com/Palakis/obs-ndi" \
	--pakdir="/package"

PAGER=cat checkinstall -y --type=debian --fstrans=no --nodoc \
        --backup=no --deldoc=yes --install=no \
        --pkgname=libndi3 --pkgversion="3.5.1" \
        --pkglicense="Proprietary" --maintainer="stephane.lepin@gmail.com" \
       	--pkggroup="video" \
        --pkgsource="http://ndi.newtek.com" \
        --pakdir="/package" ../CI/create-libndi-deb.sh

PAGER=cat checkinstall -y --type=debian --fstrans=no --nodoc \
        --backup=no --deldoc=yes --install=no \
        --pkgname=libndi3-dev --pkgversion="3.5.1" --requires="libndi3 (>= 3.5.1)" \
        --pkglicense="Proprietary" --maintainer="stephane.lepin@gmail.com" \
        --pkggroup="video" \
        --pkgsource="http://ndi.newtek.com" \
        --pakdir="/package" ../CI/create-libndi-dev-deb.sh

chmod ao+r /package/*
