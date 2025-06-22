<div align="center">
<h1>DistroAV (Formerly OBS-NDI)</h1>
<h3>Network Audio/Video in OBS-Studio using NDI technology</h3>  

<img src="https://github.com/DistroAV/DistroAV/blob/master/assets/distroav-logo-512x512.png?raw=true" width="256px" />

[![GitHub](https://img.shields.io/github/license/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/blob/master/LICENSE)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/DistroAV/DistroAV/push.yaml?label=master)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/releases/latest) ![GitHub Release Date](https://img.shields.io/github/release-date/distroav/distroav?display_date=published_at)

[![Total downloads](https://img.shields.io/github/downloads/DistroAV/DistroAV/total)](https://github.com/DistroAV/DistroAV/releases)
![Discord](https://img.shields.io/discord/1082173788101279746?style=social&logo=discord&label=Discord&link=https%3A%2F%2Fdiscord.gg%2FZuTxbUK3ug)
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
Windows ![WinGet Package Version](https://img.shields.io/winget/v/DistroAV.DistroAV)
```
winget install --exact --id DistroAV.DistroAV
```

MacOS ![Homebrew Cask Version](https://img.shields.io/homebrew/cask/v/distroav?link=https%3A%2F%2Fformulae.brew.sh%2Fcask%2Fdistroav)
```
brew install --cask distroav
```

Linux ([Flatpak](https://flatpak.org/)) ![Flathub Version](https://img.shields.io/flathub/v/com.obsproject.Studio.Plugin.DistroAV?link=https%3A%2F%2Fflathub.org%2Fapps%2Fcom.obsproject.Studio.Plugin.DistroAV)
```
flatpak install com.obsproject.Studio com.obsproject.Studio.Plugin.DistroAV
obs --system-talk-name=org.freedesktop.Avahi
```

Any other options, or errors: See [release page](https://distroav.org/download) and [installation Wiki](https://github.com/DistroAV/DistroAV/wiki/1.-Installation)

# Troubleshooting

See [Troubleshooting Wiki](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting)

Conflict with OBS-NDI plugin : [Follow the instructions](https://github.com/DistroAV/DistroAV/wiki/OBS%E2%80%90NDI-Is-Now-DistroAV)

# Development

See [Development Wiki](https://github.com/DistroAV/DistroAV/wiki/3.-Development)

**NOTE:** `OBS-NDI` was renamed to `DistroAV` ~2024/06 per [obsproject.com](https://obsproject.com)'s request to drop `OBS` from our name.

---

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date&theme=dark" />
  <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
</picture>
