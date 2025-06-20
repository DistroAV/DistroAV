**NOTE:** `OBS-NDI` was renamed to `DistroAV` ~2024/06 per [obsproject.com](https://obsproject.com)'s request to drop `OBS` from our name.

DistroAV (Formerly OBS-NDI)
==============
<div align="center">
<h3>Network Audio/Video in OBS-Studio using NDI technology</h3>  

[![GitHub](https://img.shields.io/github/license/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/blob/master/LICENSE)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/DistroAV/DistroAV/push.yaml?label=master)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/releases/latest) ![GitHub Release Date](https://img.shields.io/github/release-date/distroav/distroav?display_date=published_at) 
[![Discord](https://discordapp.com/api/guilds/1082173788101279746/widget.png?style=banner3)](https://discord.gg/ZuTxbUK3ug)  
(English Speaking)  

[![Total downloads](https://img.shields.io/github/downloads/DistroAV/DistroAV/total)](https://github.com/DistroAV/DistroAV/releases)
[![Open Collective backers and sponsors](https://img.shields.io/opencollective/all/distroav)](https://opencollective.com/distroav/donate)  
[(Please consider making a donation)](https://opencollective.com/distroav)
</div>

## Features
- **NDI Source** : Receive NDI video and audio in OBS
- **NDI Output** : Transmit OBS video and audio to NDI
- **NDI Filter** (a.k.a. **NDI Dedicated Output**) : Transmit a single OBS source or scene audio to NDI

## Requirements
* OBS >= 31.0.0 (Qt6, x64/ARM64/AppleSilicon)
* [NDI Runtime >= 6](https://github.com/DistroAV/DistroAV/wiki/1.-Installation#required---ndi-runtime)  
* [Remove old OBS-NDI plugin](https://github.com/DistroAV/DistroAV/wiki/OBS%E2%80%90NDI-Is-Now-DistroAV)

# Installation
Windows via [Winget](https://learn.microsoft.com/en-us/windows/package-manager/winget/#use-winget) ![WinGet Package Version](https://img.shields.io/winget/v/DistroAV.DistroAV)
```
winget install --exact --id DistroAV.DistroAV
```

MacOS via [homebrew](https://brew.sh/) ![Homebrew Cask Version](https://img.shields.io/homebrew/cask/v/distroav)
```
brew install --cask distroav
```

Linux ([Flatpak](https://flatpak.org/)) ![Flathub Version](https://img.shields.io/flathub/v/com.obsproject.Studio.Plugin.DistroAV)
```
flatpak install com.obsproject.Studio com.obsproject.Studio.Plugin.DistroAV
obs --system-talk-name=org.freedesktop.Avahi
```

Manually from the latest [release page](https://distroav.org/download).

Any other options, or errors: See [Installation Wiki](https://github.com/DistroAV/DistroAV/wiki/1.-Installation)

# Troubleshooting

See [Troubleshooting Wiki](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting)

Conflict with OBS-NDI plugin : [Follow the instructions](https://github.com/DistroAV/DistroAV/wiki/OBS%E2%80%90NDI-Is-Now-DistroAV)

# Development

See [Development Wiki](https://github.com/DistroAV/DistroAV/wiki/3.-Development)

---

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date&theme=dark" />
  <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
</picture>
