obs-ndi
==============

Network A/V in OBS Studio with NewTek's NDI technology.

<!--
[![Build Status](https://dev.azure.com/Palakis/obs-ndi/_apis/build/status/Palakis.obs-ndi?branchName=master)](https://dev.azure.com/Palakis/obs-ndi/_build/latest?definitionId=1&branchName=master)
[![Twitter](https://img.shields.io/twitter/url/https/twitter.com/fold_left.svg?style=social&label=Follow%20%40LePalakis)](https://twitter.com/LePalakis)
[![Financial Contributors on Open Collective](https://opencollective.com/obs-websocket/all/badge.svg?label=financial+contributors)](https://opencollective.com/obs-websocket)
-->

## Features
- **NDI Source** : receive NDI video and audio in OBS
- **NDI Output** : transmit OBS video and audio to NDI
- **NDI Filter** (a.k.a NDI Dedicated Output) : transmit a single OBS source or scene audio to NDI

## Requirements
* OBS >=28
* NDI 5 Runtime  
  We are not allowed to directly distribute the NDI runtime here, but you can get it from either
  [the links below](#Download_&_Install_The_NDI_5_Runtime), or installing
  [NDI Tools](https://ndi.video/tools/), or the [NDI SDK](https://ndi.video/download-ndi-sdk/).

# Install OBS-NDI

Download and install the Linux, MacOS, or Windows version at [Releases](https://github.com/obs-ndi/obs-ndi/releases).

* Linux
    1. Download [obs-ndi-4.12.0-linux-x86_64.deb](https://github.com/obs-ndi/obs-ndi/releases/download/4.12.0/obs-ndi-4.12.0-linux-x86_64.deb)
    2. `sudo dpkg -i obs-ndi-4.12.0-linux-x86_64.deb`
    3. If this does not work then try:
    ```
    $ sudo ln -s /usr/local/lib/obs-plugins/obs-ndi.so /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ndi.so
    $ sudo ln -s /usr/local/share/obs/obs-plugins/obs-ndi/ /usr/share/obs/obs-plugins/obs-ndi
    ```
* MacOS:
    1. Download [obs-ndi-4.12.0-macos-universal.pkg](https://github.com/obs-ndi/obs-ndi/releases/download/4.12.0/obs-ndi-4.12.0-macos-universal.pkg)
    2. Run `obs-ndi-4.12.0-macos-universal.pkg`
* Windows:
    1. Download [obs-ndi-4.12.0-windows-x64-Installer.exe](https://github.com/obs-ndi/obs-ndi/releases/download/4.12.0/obs-ndi-4.12.0-windows-x64-Installer.exe)
    2. Run `obs-ndi-4.12.0-windows-x64-Installer.exe`

# Download & Install The NDI 5 Runtime
* Linux - there is no redist, so you have to do something like the following (also at [./CI/libndi-get.sh](./CI/libndi-get.sh))
```
#!/bin/bash

set -e

LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v5_Linux"
LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"
LIBNDI_INSTALLER_SHA256="7e5c54693d6aee6b6f1d6d49f48d4effd7281abd216d9ff601be2d55af12f7f5"

#sudo apt-get install curl

pushd /tmp
curl -L -o $LIBNDI_INSTALLER https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER -f --retry 5
echo "$LIBNDI_INSTALLER_SHA256 $LIBNDI_INSTALLER" | sha256sum -c
tar -xf $LIBNDI_INSTALLER
yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh

rm -rf /tmp/ndisdk
mv "/tmp/NDI SDK for Linux" /tmp/ndisdk
ls /tmp/ndisdk

if [ $1 == "install" ]; then
    # NOTE: This does an actual local install...
    sudo cp -P /tmp/ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
    sudo ldconfig
fi

popd
```
* MacOS: http://ndi.link/NDIRedistV5Apple
* Windows: http://ndi.link/NDIRedistV5
<!--
* MacOS: [libNDI_5.5.3_for_Mac.pkg](https://github.com/obs-ndi/obs-ndi/raw/d462e9f83f0e06837a83331b1f71053b2132e751/runtime/libNDI_5.5.3_for_Mac.pkg)
* Windows: [NDI 5.5.3 Runtime.exe](https://github.com/obs-ndi/obs-ndi/raw/d462e9f83f0e06837a83331b1f71053b2132e751/runtime/NDI%205.5.3%20Runtime.exe)
-->

        
## Uninstall

Reference: https://obsproject.com/kb/plugins-guide#install-or-remove-plugins

### Linux

1. `rm -rf ~/.config/obs-studio/plugins/obs-ndi`
2. Optionally delete NDI Runtime:
    1. ```
       sudo rm /usr/local/lib/libndi*.*
       sudo ldconfig
       ```

### MacOS

1. Open Finder
2. Show hidden files with `Command-Shift-.`
3. Delete `~/Library/Application Support/obs-studio/plugins/obs-ndi.plugin`
4. Optionally delete NDI Tools/Runtime:
    1. Finder->Applications: Delete all `NDI *` applications
    2. Delete `/Library/Application Support/NewTek/NDI`
    2. Delete `/usr/local/lib/libndi.*`

### Windows

1. Add/Remove Programs
2. Delete `%ProgramFiles%\obs-studio\obs-plugins\64-bit\obs-ndi.*`
3. Optionally delete NDI Tools/Runtime:
    1. Add/Remove Programs
    2. Delete `%ProgramFiles%\NDI\NDI 5 Runtime`
    3. Delete `%ProgramFiles%\NDI\NDI 5 Tools`

# Development

## Building

### Windows
In PowerShell Core 7+ terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
.github/scripts/Build-Windows.ps1
...
.github/scripts/Package-Windows.ps1 -BuildInstaller
```

If you get `SecurityError/PSSecurityException/UnauthorizedAccess` error then:
```
Set-ExecutionPolicy -ExecutionPolicy Unrestricted -Scope CurrentUser
```

Subsequent builds can be sped up by using `Build-Windows.ps1 -SkipDeps`.  
See `Help .github/scripts/Build-Windows.ps1` for more details.

### Linux
NOTE: Only Debian and Ubuntu are officially supported

In terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
.github/scripts/build-linux
...
.github/scripts/package-linux
...
sudo cp -r release/obs-plugins/64bit/* /usr/local/lib/x86_64-linux-gnu/obs-plugins/
...
sudo cp -r release/data/obs-plugins/* /usr/local/share/obs/obs-plugins/
...
sudo ldconfig
```
Subsequent builds can be sped up by using `build-linux --skip-deps`.  
See `build-linux --help` for more details.

### OS X
In terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
.github/scripts/build-macos
...
.github/scripts/package-macos
...
cp -r release/obs-ndi.plugin ~/Library/Application\ Support/obs-studio/plugins/
...
```

Subsequent builds can be sped up by using `build-macos --skip-deps`.  
See `build-macos --help` for more details.

## Formatting
Requires [obsproject/tools/]clang-format@13, cmakelang, and zsh installed.

From a Linux or MacOS terminal:
```
./build-aux/run-clang-format
./build-aux/run-cmake-format
```

<!--
# TODOs
- [ ] Generate readme from config file and set version number for install links.
  Similar to how https://github.com/cloudposse/build-harness does theirs.
- [ ] Get build badges working again.
- [ ] Expand usage of .github folder:
  Is supported content actually officially exhaustively documented anywhere?
  https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions
  https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/creating-a-default-community-health-file
  https://stackoverflow.com/a/61301254
  ...
- [ ] Set up CODEOWNERS
    https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners
-->
