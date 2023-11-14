obs-ndi
==============

[![Discord Shield](https://discordapp.com/api/guilds/1082173788101279746/widget.png?style=banner3)](https://discord.gg/ZuTxbUK3ug)

Network Audio/Video in OBS-Studio using NDI technology.

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
* OBS >= 30.0.0 (Qt6 & x64)
* NDI 5 Runtime  
  We are not allowed to directly distribute the NDI runtime here, but you can get it from either
  [the links below](#download--install-the-ndi-5-runtime), or installing
  [NDI Tools](https://ndi.video/tools/), or the [NDI SDK](https://ndi.video/download-ndi-sdk/).

# Install OBS-NDI
Download and install the Linux, MacOS, or Windows version at [Releases](https://github.com/obs-ndi/obs-ndi/releases).

* Linux
    1. Download [obs-ndi-4.13.0-x86_64-linux-gnu.deb](https://github.com/obs-ndi/obs-ndi/releases/download/4.13.0/obs-ndi-4.13.0-x86_64-linux-gnu.deb)
    2. `sudo dpkg -i obs-ndi-4.13.0-x86_64-linux-gnu.deb`
    3. If this does not work then try:
    ```
    sudo ln -s /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ndi.so /usr/local/lib/obs-plugins/obs-ndi.so
    sudo ln -s /usr/share/obs/obs-plugins/obs-ndi/ /usr/local/share/obs/obs-plugins/obs-ndi
    ```
    * Flatpak and similar installs of this plugin or OBS can cause complications; please experiment and report on our Discord server any problems and preferrably solutions.
* MacOS:
    1. Download [obs-ndi-4.13.0-macos-universal.pkg](https://github.com/obs-ndi/obs-ndi/releases/download/4.13.0/obs-ndi-4.13.0-macos-universal.pkg)
    2. Run `obs-ndi-4.13.0-macos-universal.pkg`  
       If MacOS complains about the file, either:
        1. Allow it in `System Settings`->`Privacy & Security`  
          -or-
        2. `sudo xattr -r -d com.apple.quarantine obs-ndi-4.13.0-macos-universal.pkg`
* Windows:
    1. Download [obs-ndi-4.13.0-windows-x64-Installer.exe](https://github.com/obs-ndi/obs-ndi/releases/download/4.13.0/obs-ndi-4.13.0-windows-x64-Installer.exe)
    2. Run `obs-ndi-4.13.0-windows-x64-Installer.exe`

# Download & Install The NDI 5 Runtime
* Linux: [./CI/libndi-get.sh](./CI/libndi-get.sh)
* MacOS: http://ndi.link/NDIRedistV5Apple
* Windows: http://ndi.link/NDIRedistV5
        
## Uninstall
Reference: https://obsproject.com/kb/plugins-guide#install-or-remove-plugins

### Linux
1. ```
   sudo rm /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ndi.so
   sudo rm -rf /usr/share/obs/obs-plugins/obs-ndi/
   ```
2. Optionally delete NDI Runtime:
   ```
   sudo rm /usr/local/lib/libndi*
   sudo ldconfig
   ```

### MacOS
1. Open Finder
2. Show hidden files with `Command-Shift-.`
3. Delete `~/Library/Application Support/obs-studio/plugins/obs-ndi.plugin`
4. Optionally delete NDI Tools/Runtime:
    1. Finder->Applications: Delete all `NDI *` applications
    2. Delete `/Library/Application Support/NewTek/NDI`
    3. Delete `/usr/local/lib/libndi*`

### Windows
1. Add/Remove Programs: obs-ndi
2. Delete `%ProgramFiles%\obs-studio\obs-plugins\64bit\obs-ndi.*`
3. Delete `%ProgramFiles%\obs-studio\data\obs-plugins\obs-ndi\`
4. Optionally delete NDI Tools/Runtime:
    1. Add/Remove Programs:
        1. NDI 5 Runtime
        2. NDI 5 Tools
        3. NDI 5 SDK
        4. NDI 5 Advanced SDK
    2. Delete `%ProgramFiles%\NDI\NDI 5 Runtime`
    3. Delete `%ProgramFiles%\NDI\NDI 5 Tools`
    4. Delete `%ProgramFiles%\NDI\NDI 5 SDK`
    5. Delete `%ProgramFiles%\NDI\NDI 5 Advanced SDK`

# Development

## Building

### Windows
In PowerShell Core 7+ terminal:
```
git clone https://github.com/obs-ndi/obs-ndi.git
cd obs-ndi
```
First build:
```
.github/scripts/Build-Windows.ps1 && .github/scripts/Package-Windows.ps1 -BuildInstaller
```
Subsequent builds:
```
.github/scripts/Build-Windows.ps1 -SkipDeps && .github/scripts/Package-Windows.ps1 -BuildInstaller
```
See `Help .github/scripts/Build-Windows.ps1` for more details.

If you get `SecurityError/PSSecurityException/UnauthorizedAccess` error then:
```
Set-ExecutionPolicy -ExecutionPolicy Unrestricted -Scope CurrentUser
```

<!--
```
.github/scripts/Build-Windows.ps1 -SkipDeps && .github/scripts/Package-Windows.ps1 -BuildInstaller && release\obs-ndi-4.13.0-windows-x64-Installer.exe
```
-->

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
