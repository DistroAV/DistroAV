obs-ndi
==============
Network A/V in OBS Studio with NewTek's NDI technology

## Build prerequisites
You need QT 5.7, CMake, and a working development environment for OBS Studio installed on your computer.

## How to build
You'll need to fill these CMake variables :
- **NDISDK_DIR** (path) : location of the NewTek NDI SDK
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source code of OBS Studio
- **LIBOBS_LIB** (filepath) : location of the obs.lib file
- **PTHREADS_LIB** (filepath) : location of the pthreads library (maybe Windows only)
