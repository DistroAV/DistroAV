obs-ndi
==============

Network A/V in OBS Studio with NewTek's NDI technology.

[![Build Status](https://dev.azure.com/Palakis/obs-ndi/_apis/build/status/Palakis.obs-ndi?branchName=master)](https://dev.azure.com/Palakis/obs-ndi/_build/latest?definitionId=1&branchName=master)
[![Twitter](https://img.shields.io/twitter/url/https/twitter.com/fold_left.svg?style=social&label=Follow%20%40LePalakis)](https://twitter.com/LePalakis)
[![Financial Contributors on Open Collective](https://opencollective.com/obs-websocket/all/badge.svg?label=financial+contributors)](https://opencollective.com/obs-websocket)

## Features
- **NDI Source** : receive NDI video and audio in OBS
- **NDI Output** : transmit OBS video and audio to NDI
- **NDI Filter** (a.k.a NDI Dedicated Output) : transmit a single OBS source or scene audio to NDI

## Install

1. Download binaries/installers for Windows, MacOS, and Linux in [Releases](https://github.com/obs-ndi/obs-ndi/releases).
2. Download NDI Tools from:
    * Windows: https://go.ndi.tv/tools-for-windows
    * Mac: https://go.ndi.tv/tools-for-mac
    * Linux:
        * https://downloads.ndi.tv/SDK/NDI_SDK_Linux/Install_NDI_SDK_v5_Linux.tar.gz and manually install or...
        * https://github.com/obs-ndi/obs-ndi/blob/master/CI/libndi-get.sh (this will be improved in the next version)

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

### Formatting
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
