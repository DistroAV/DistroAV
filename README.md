obs-ndi
==============

Network A/V in OBS Studio with NewTek's NDI technology.  

[![Gitter chat](https://badges.gitter.im/obs-ndi/obs-ndi.png)](https://gitter.im/obs-ndi/obs-ndi)


## Features
- **NDI Source** : receive NDI video and audio in OBS
- **NDI Output** : transmit video and audio from OBS to NDI
- **NDI Filter** (a.k.a NDI Dedicated Output) : transmit a single source or scene to NDI

## Downloads
Binaries for Windows are available in the [Releases](https://github.com/Palakis/obs-ndi/releases) section. Linux and OS X versions are not yet available, as compatibility with these systems is under development.

## Compiling
### Prerequisites
You'll need CMake, the NewTek NDI SDK V2 and a working development environment for OBS Studio installed on your computer.

### Windows
In cmake-gui, you'll have to set these CMake variables :
- **NDISDK_DIR** (path) : location of the NewTek NDI SDK
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source code of OBS Studio
- **LIBOBS_LIB** (filepath) : location of the obs.lib file
- **OBS_FRONTEND_LIB** (filepath) : location of the obs-frontend-api.lib file

### Linux
On Debian/Ubuntu :  
```
git clone https://github.com/Palakis/obs-ndi.git
cd obs-ndi
git checkout 0.3.2
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR="<path to the libobs sub-folder in obs-studio's source code>" -DNDISDK_DIR="<root dir of the NDI SDK>" -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
```

### OS X
```
git clone https://github.com/Palakis/obs-ndi.git
cd obs-ndi
mkdir build && cd build
cmake -DNDISDK_DIR="<path to the NewTek NDI SDK>" -DLIBOBS_INCLUDE_DIR=<path to the libobs sub-folder in obs-studio's source code> -DLIBOBS_LIB=<path to libobs.0.dylib> -DOBS_FRONTEND_LIB=<path to libobs-frontend-api.dylib> -DQt5Core_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Core -DQt5Widgets_DIR=/usr/local/opt/qt5/lib/cmake/Qt5Widgets ../
make -j4
# Copy libobs-ndi.so to the obs-plugins folder
# Copy libndi.dylib from the NDI SDK to the obs-plugins folder too
```

### Automated Builds
- Windows : [![Automated Build status for Windows](https://ci.appveyor.com/api/projects/status/github/Palakis/obs-ndi)](https://ci.appveyor.com/project/Palakis/obs-ndi/history)
- Linux & OS X : [![Automated Build status for Linux & OS X](https://travis-ci.org/Palakis/obs-ndi.svg?branch=master)](https://travis-ci.org/Palakis/obs-ndi)
