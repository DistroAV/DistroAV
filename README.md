obs-ndi
==============
### Discord
[![Discord Shield](https://discordapp.com/api/guilds/1082173788101279746/widget.png?style=banner3)](https://discord.gg/ZuTxbUK3ug)
(English Speaking)

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

# Install

## REQUIRED: Install NDI Runtime
obs-ndi is not authorized to redistribute the NDI Runtime.  
You must download and install the NDI Runtime yourself:
* Linux:
  * Debian: [./CI/libndi-get.sh](./CI/libndi-get.sh)
  * Flatpak: Already included
* MacOS: http://ndi.link/NDIRedistV5Apple
* Windows: http://ndi.link/NDIRedistV5

## Linux Additional Requirements for NDI
```
sudo apt install avahi-daemon ffmpeg
sudo systemctl enable avahi-daemon
sudo systemctl start avahi-daemon
sudo ufw allow 5353/udp
sudo ufw allow 5959:5969/tcp
sudo ufw allow 5959:5969/udp
sudo ufw allow 6960:6970/tcp
sudo ufw allow 6960:6970/udp
sudo ufw allow 7960:7970/tcp
sudo ufw allow 7960:7970/udp
sudo ufw allow 5960/tcp
```
Firewall rules taken from https://ndi.tv/tools/education/networking/best-practices/networking-best-practice/

## Install OBS-NDI
Download and install the Linux, MacOS, or Windows version at [Releases](https://github.com/obs-ndi/obs-ndi/releases).

* Linux
    * Debian:
      1. Download [obs-ndi-4.14.0-x86_64-linux-gnu.deb](https://github.com/obs-ndi/obs-ndi/releases/download/4.14.0/obs-ndi-4.14.0-x86_64-linux-gnu.deb)
      2. `sudo dpkg -i obs-ndi-4.14.0-x86_64-linux-gnu.deb`
      3. If this does not work then try:
      ```
      sudo ln -s /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ndi.so /usr/local/lib/obs-plugins/obs-ndi.so
      sudo ln -s /usr/share/obs/obs-plugins/obs-ndi/ /usr/local/share/obs/obs-plugins/obs-ndi
      ```
    * Flatpak (per https://github.com/obs-ndi/obs-ndi/issues/724#issuecomment-1817782905):
      ```
      flatpak install com.obsproject.Studio com.obsproject.Studio.Plugin.NDI
      obs --system-talk-name=org.freedesktop.Avahi
      ```
      obs-ndi's flakpak is still a work in progress; please experiment and
      report on our [Discord server](#discord) any problems or improvements.
* MacOS:
    1. Download [obs-ndi-4.14.0-macos-universal.pkg](https://github.com/obs-ndi/obs-ndi/releases/download/4.14.0/obs-ndi-4.14.0-macos-universal.pkg)
    2. Run `obs-ndi-4.14.0-macos-universal.pkg`  
       If MacOS complains about the file, either:
        1. Allow it in `System Settings`->`Privacy & Security`  
          -or-
        2. `sudo xattr -r -d com.apple.quarantine obs-ndi-4.14.0-macos-universal.pkg`
* Windows:
    1. Download [obs-ndi-4.14.0-windows-x64-Installer.exe](https://github.com/obs-ndi/obs-ndi/releases/download/4.14.0/obs-ndi-4.14.0-windows-x64-Installer.exe)
    2. Run `obs-ndi-4.14.0-windows-x64-Installer.exe`
        
## Uninstall
Reference: https://obsproject.com/kb/plugins-guide#install-or-remove-plugins

### Linux

#### Debian
1. ```
   sudo rm /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ndi.so
   sudo rm -rf /usr/share/obs/obs-plugins/obs-ndi/
   ```
2. Optionally delete NDI Runtime:
   ```
   sudo rm /usr/local/lib/libndi*
   sudo ldconfig
   ```

#### Flatpak
WIP...

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

# Troubleshooting

## OBS Logs
In OBS select `Help`->`Log Files`.
Either:
* `View Current Log`
  -or- 
* `Upload Current Log File`, and then copy the URL and paste it somewhere to share.

## Matrix
| Symptom | Check |
| <ul><li>No `NDI Output Settings` in the OBS Tools menu.</li><li>No `NDI Source` listed when trying to add a Source.</li><li>No `NDI Filter` listed when trying to add a Filter.</li></ul> | <ul><li>OBS Compatibility: Ensure your OBS version is 30.0.0 or higher.</li><li>Verify OBS-NDI Installation: Check the installation paths for OBS-NDI files based on your operating system:<ul><li>Linux:<br>`/usr/lib/x86_64-linux-gnu/obs-plugins/obs-ndi.so`<br>`/usr/share/obs/obs-plugins/obs-ndi/`</li><li>MacOS: `~/Library/Application Support/obs-studio/plugins/obs-ndi.plugin`</li><li>Windows:<br>`%ProgramFiles%\obs-studio\obs-plugins\64bit\obs-ndi.*`<br>`%ProgramFiles%\obs-studio\data\obs-plugins\obs-ndi\​​`</li></ul></li><li>Verify NDI Runtime Installation: Ensure the NDI runtime is correctly installed:<ul><li>Linux: `/usr/local/lib/libndi*`</li><li>MacOS:<br>`/Library/Application Support/NewTek/NDI`<br>`/usr/local/lib/libndi*`</li><li>Windows: `%ProgramFiles%\NDI\NDI 5 Runtime`</li></ul><li>Check OBS Log: Go to `Help`->`Log Files`->`View Current Log` in OBS to check for obs-ndi related entries.</li><li>Check Firewall:<ul><li>Ensure that all NDI devices are on the same network.</li><li>Turn off all other device network interfaces.</li><li>Test with firewall temporarily turned off.</li><li>Windows:<ul><li>Check if network is `Private`</li></ul></li></ul></ul> |
| A transmitting **OBS-NDI** source is not listed in `NDI Source`->`Source name` | <ul><li>See if the NDI Source shows up in NDI Studio Monitor.</li><li>See if using NDI Screen Capture on the transmitting device shows up in NDI Studio Monitor or OBS-NDI.</li></ul> |
| A transmitting **NDI** source is not listed in `NDI Source`->`Source name` | <ul><li>See if the NDI Source shows up in NDI Studio Monitor.</li></ul> |

## Debugging
```
obs[64.exe] --obs-ndi-verbose --verbose
```

# Development

## Building

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

### MacOS
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
NOTE:(pv)
```
.github/scripts/Build-Windows.ps1 -SkipDeps && .github/scripts/Package-Windows.ps1 -BuildInstaller && release\obs-ndi-4.14.0-windows-x64-Installer.exe
```
-->

## Formatting
Requires [obsproject/tools/]clang-format@16, cmakelang, and zsh installed.
From https://brew.sh/ ...
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
...then follow the instructions printed out at the end of install.

From .github\actions\run-clang-format\action.yaml...
```
brew install --quiet obsproject/tools/clang-format@16
```
...then follow the instructions printed out at the end of install.zub

### Linux/MacOS

Requires [obsproject/tools/]clang-format@13, cmakelang, and zsh installed.

Open a terminal:
```
./build-aux/run-clang-format
./build-aux/run-cmake-format
```

### Windows (WSL)

Open a WSL Ubuntu terminal:
```
...
clang-format-16 -i src/obs-ndi-filter.cpp
clang-format-16 -i src/obs-ndi-source.cpp
clang-format-16 -i src/plugin-main.cpp
...
```

### Supress Formatting/Code Errors/Warnings

I keep forgetting how to do this, so adding a note here.
On rare/sparing/appropriate basis, suppress Lint errors with:
```
#if defined(__linux__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfoo"
#endif
    ...foo code...
#if defined(__linux__)
#pragma GCC diagnostic pop
#endif
```
Replace `__linux__` w/ `__GNUC__` or `WIN32` type of macros as appropriate.

## Pre-Release

### Seeking Testers
This plugin is based off of https://github.com/obsproject/obs-plugintemplate that supports CI packaging installers if the PR is labeled with `Seeking Testers`:
https://github.com/obsproject/obs-plugintemplate/blob/e385ac1863b8c658e7f6cc564f04040c9057c9f0/.github/workflows/build-project.yaml#L32C1-L36

Use this feature when wanting testing/feedback on a branch build.

### Release Candidate
...
Use this feature when wanting testing/feedback on a build imminent for release.

## Release
1. Update  [./buildspec.json](./buildspec.json) `"version"` to the new version number
  (ex: `"4.14.0"`).
2. Search this readme for the previous version number and replace all with the new version number
  (ex: `4.13.0` -> `4.14.0`)
3. Commit and push these changes to the branch to be merged to `master`.
4. Merge to `master` when ready.
5. Create tag for new version (ex: `4.14.0`) and push tag to obs-ndi origin.

NOTE: Per https://github.com/obs-ndi/obs-ndi/issues/724 this plugin also generates a flatpak via https://github.com/flathub/flathub/pull/4701 ...

## Post-Release
### MacOS
Similar to Windows, MacOS also complains about the installer.
...

### Windows
The Windows installer is not [yet?] signed.  
This can result in errors while downloading and running.  
To mitigate this:
1. Manually try a download and/or install
2. If it gives warnings then submit the installer to https://www.microsoft.com/en-us/wdsi/filesubmission/?persona=SoftwareDeveloper
  * Product: Microsoft Smart Scanner
  * Company: obs-ndi
  * What do you believe this file is:
    * Incorrectly detected as malware/malicious
    * Detection name: (Get from Windows Defender)
    * Definition version: (Get from Windows Defender)
    * Additional information:
      > I am a developer on the https://github.com/obs-ndi/obs-ndi project and our downloads are being flagged as a virus.

## NOTE: Non-Rewrite vs Rewrite Branch Translations
`master` [non-rewrite] branch vs/to<->from `rewrite` branch:
| =non-rewrite= | =rewrite= | =note= |
| preview-output? | aux-output | |
| config | config | |
| ? | input-utils | |
| obs-ndi-source | input | |
| plugin-main | obs-ndi | |
| | output-manager | |
| obs-ndi-output | output | |
| main-output | ? | |
| obs-ndi-filter | ? | |

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
