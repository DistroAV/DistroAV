obs-ndi
==============
Network A/V in OBS Studio with NewTek's NDI technology.  

## Downloads
Binaries for Windows are available in the [Releases](https://github.com/Palakis/obs-ndi/releases) section. Linux and OS X versions are not yet available, as compatibility with these systems is under development.

## Compiling
### Prerequisites
You'll need CMake, the NewTek NDI SDK and a working development environment for OBS Studio installed on your computer.

### Windows
In cmake-gui, you'll have to set these CMake variables :
- **NDISDK_DIR** (path) : location of the NewTek NDI SDK
- **LIBOBS_INCLUDE_DIR** (path) : location of the libobs subfolder in the source code of OBS Studio
- **LIBOBS_LIB** (filepath) : location of the obs.lib file

### OS X
*To do*

### Linux
*To do*
