obs-ndi
==============

Network A/V in OBS Studio with NewTek's NDI technology.

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
In cmake-gui, you'll have to set these CMake variables :
- **QTDIR** (path) : location of the Qt environment suited for your compiler and architecture
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source code of OBS Studio
- **LIBOBS_LIB** (filepath) : location of the obs.lib file
- **OBS_FRONTEND_LIB** (filepath) : location of the obs-frontend-api.lib file

### Linux
On Debian/Ubuntu :  
```
git clone https://github.com/Palakis/obs-ndi.git
cd obs-ndi
mkdir build && cd build
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

### Automated Builds
- Windows: [![Automated Build status for Windows](https://ci.appveyor.com/api/projects/status/github/Palakis/obs-ndi)](https://ci.appveyor.com/project/Palakis/obs-ndi/history)
- Linux: [![Automated Build status for Linux](https://travis-ci.org/Palakis/obs-ndi.svg?branch=master)](https://travis-ci.org/Palakis/obs-ndi)
- macOS: [![Automated Build status for macOS](https://img.shields.io/azure-devops/build/Palakis/obs-ndi/Palakis.obs-ndi.svg)](https://dev.azure.com/Palakis/obs-ndi/_build)
