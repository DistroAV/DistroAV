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

# Install

1. Download installer for Linux, MacOS, or Windows in [Releases](https://github.com/obs-ndi/obs-ndi/releases).
2. Run the installer:  
   Replace `x.y.z` with the version number you downloaded.
    * Linux: `sudo dpkg -i obs-ndi-x.y.z-linux-x86_64.deb`
    * MacOS: Run `obs-ndi-x.y.z-macos-universal.pkg`
    * Windows: Run `obs-ndi-x.y.z-windows-x64-Installer.exe`
2. Download the NDI 5 runtime from:
    * Linux:
      ```
      #!/bin/bash
      set -e
      LIBNDI_INSTALLER_NAME="Install_NDI_SDK_v5_Linux"
      LIBNDI_INSTALLER="$LIBNDI_INSTALLER_NAME.tar.gz"
      LIBNDI_INSTALLER_SHA256="00d0bedc2c72736d82883fc0fd6bc1a544e7958c7e46db79f326633d44e15153"
      pushd /tmp
      sudo apt-get install curl
      curl -L -o $LIBNDI_INSTALLER https://downloads.ndi.tv/SDK/NDI_SDK_Linux/$LIBNDI_INSTALLER -f --retry 5
      echo "$LIBNDI_INSTALLER_SHA256 $LIBNDI_INSTALLER" | sha256sum -c
      tar -xf $LIBNDI_INSTALLER
      yes | PAGER="cat" sh $LIBNDI_INSTALLER_NAME.sh
      rm -rf ndisdk
      mv "NDI SDK for Linux" ndisdk
      sudo cp -P ndisdk/lib/x86_64-linux-gnu/* /usr/local/lib/
      sudo ldconfig
      echo libndi installed to /usr/local/lib/
      ls -la /usr/local/lib/libndi*
      rm -rf ndisdk
      popd
      ```
    * MacOS: http://new.tk/NDIRedistV5Apple
    * Windows: http://new.tk/NDIRedistV5

NOTE: The above official `NDI Runtime` links for MacOS and Windows are currently down. :/  
The only other official way to get the NDI Runtime is by installing the NDI 5 SDK.  
Unofficially you can get them from this PR:
  * MacOS: [libNDI_5.5.3_for_Mac.pkg](https://github.com/obs-ndi/obs-ndi/raw/d462e9f83f0e06837a83331b1f71053b2132e751/runtime/libNDI_5.5.3_for_Mac.pkg)
  * Windows: [NDI 5.5.3 Runtime.exe](https://github.com/obs-ndi/obs-ndi/raw/d462e9f83f0e06837a83331b1f71053b2132e751/runtime/NDI%205.5.3%20Runtime.exe)
        
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
    1. TBD...


# Development

## Compiling

### Windows
In PowerShell v5+ terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
.github/scripts/Build-Windows.ps1
...
tbd...
```

### Linux
NOTE: Only Debian and Ubuntu are officially supported

In terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
.github/scripts/build-linux.sh
...
sudo cp -r release/obs-plugins/64bit/* /usr/local/lib/x86_64-linux-gnu/obs-plugins/
...
sudo cp -r release/data/obs-plugins/* /usr/local/share/obs/obs-plugins/
...
sudo ldconfig
```

### OS X
In terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
.github/scripts/build-macos.zsh
...
cp -r release/obs-ndi.plugin ~/Library/Application\ Support/obs-studio/plugins/
...
```

Subsequent builds can be sped up by using `build-macos.zsh --skip-deps`.  
[For some reason `--skip-all` doesn't work.]  
See `build-macos.zsh --help` for more details.

## Formatting
From a bash shell (confirmed also works on WSL):
```
.github/scripts/check-format.sh 1
```
NOTE: `obs-ndi` is based on [`obsplugin-template`](https://github.com/obsproject/obs-plugintemplate) that [requires `clang-format-13`](https://github.com/obsproject/obs-plugintemplate/blob/525650f97209450cf2dcc06ff28ad941cc1bbd7b/.github/scripts/check-format.sh#L29-L42):
```
if type clang-format-13 2> /dev/null ; then
    CLANG_FORMAT=clang-format-13
elif type clang-format 2> /dev/null ; then
    # Clang format found, but need to check version
    CLANG_FORMAT=clang-format
    V=$(clang-format --version)
    if [[ $V != *"version 13.0"* ]]; then
        echo "clang-format is not 13.0 (returned ${V})"
        exit 1
    fi
else
    echo "No appropriate clang-format found (expected clang-format-13.0.0, or clang-format)"
    exit 1
fi
```
[MacOS brew only has formulaes for clang-format 15, 11, or 8.](https://formulae.brew.sh/formula/clang-format)
If you want to check-format on MacOS then you will need to:
1. brew install clang-format
2. edit `.github/scripts/check-format.sh` and change all `13` to `15`
3. !!DO NOT COMMIT THESE CHANGES!!
