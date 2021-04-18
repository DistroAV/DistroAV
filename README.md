obs-ndi
==============

Network A/V in OBS Studio with NewTek's NDI technology.

[![Build Status](https://dev.azure.com/Palakis/obs-ndi/_apis/build/status/Palakis.obs-ndi?branchName=master)](https://dev.azure.com/Palakis/obs-ndi/_build/latest?definitionId=1&branchName=master)
[![Twitter](https://img.shields.io/twitter/url/https/twitter.com/fold_left.svg?style=social&label=Follow%20%40LePalakis)](https://twitter.com/LePalakis)
[![Financial Contributors on Open Collective](https://opencollective.com/obs-websocket/all/badge.svg?label=financial+contributors)](https://opencollective.com/obs-websocket)

## Features
- **NDI Source** : receive NDI video and audio in OBS
- **NDI Output** : transmit video and audio from OBS to NDI
- **NDI Filter** (a.k.a NDI Dedicated Output) : transmit a single source or scene to NDI

## Downloads
Binaries for Windows, macOS and Linux are available in the [Releases](https://github.com/Palakis/obs-ndi/releases) section.

## Compiling
### Prerequisites
You'll need CMake and a working development environment for OBS Studio installed on your computer.

### Windows
In cmake-gui, you'll have to set these CMake variables:
- **QTDIR** (path) : location of the Qt environment suited for your compiler and architecture
- **LIBOBS_INCLUDE_DIR** (path) : location of the `libobs` subfolder in the source code of OBS Studio
- **LIBOBS_LIB** (filepath) : location of the obs.lib file
- **OBS_FRONTEND_LIB** (filepath) : location of the obs-frontend-api.lib file

### Linux
On Debian/Ubuntu:
```
# Start by installing the latest libndi deb available [here](https://github.com/Palakis/obs-ndi/releases).
git clone https://github.com/Palakis/obs-ndi.git
cd obs-ndi
mkdir build && cd build
# If you are on Ubuntu, add the `-DUSE_UBUNTU_FIX=true` flag to your cmake command
cmake -DLIBOBS_INCLUDE_DIR="<path to the libobs sub-folder in obs-studio's source code>" -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
```

### OS X
```
git clone https://github.com/Palakis/obs-ndi.git
cd obs-ndi
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR=<path to the libobs sub-folder in obs-studio's source code> -DLIBOBS_LIB=<path to libobs.0.dylib> -DOBS_FRONTEND_LIB=<path to libobs-frontend-api.dylib> -DQt5Core_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Core -DQt5Widgets_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Widgets ../
make -j4
# Copy libobs-ndi.so to the obs-plugins folder
# Copy libndi.dylib from the NDI SDK to the obs-plugins folder too
```
