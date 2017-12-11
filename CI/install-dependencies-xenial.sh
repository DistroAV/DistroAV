#!/bin/sh
set -ex

# OBS Studio deps
apt-get -qq update
apt-get install -y \
        libc-dev-bin libc6-dev \
        git \
        build-essential \
        unzip

apt-get install -y \
        build-essential \
        checkinstall \
        cmake \
        libasound2-dev \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libcurl4-openssl-dev \
        libfontconfig-dev \
        libfreetype6-dev \
        libgl1-mesa-dev \
        libjack-jackd2-dev \
        libjansson-dev \
        libpulse-dev \
        libqt5x11extras5-dev \
        libspeexdsp-dev \
        libswresample-dev \
        libswscale-dev \
        libudev-dev \
        libv4l-dev \
        libvlc-dev \
        libx11-dev \
        libx264-dev \
        libxcb-shm0-dev \
        libxcb-xinerama0-dev \
        libxcomposite-dev \
        libxinerama-dev \
        pkg-config \
        qtbase5-dev


cd /root

curl -kLO https://slepin.fr/obs-ndi/ci/ndisdk-slim-v3-linux.zip -f --retry 5
unzip ./ndisdk-slim-v3-linux.zip

# Build obs-studio
git clone https://github.com/jp9000/obs-studio ./obs-studio
cd obs-studio
git checkout 20.1.0s
mkdir build && cd build
cmake -DUNIX_STRUCTURE=1 -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
make install

ldconfig
