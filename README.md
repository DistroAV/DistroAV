<div align="center">
<h1>DistroAV</h1>
<h3>Network Audio/Video in OBS-Studio using NDI technology</h3>  

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="./assets/distroav-full-clean-white.svg">
  <source media="(prefers-color-scheme: light)" srcset="./assets/distroav-full-clean.svg">
  <img width="650" alt="Fallback DistroAV logo when no light or dark theme is detected" src="./assets/distroav-full-clean.svg">
</picture>

[![GitHub](https://img.shields.io/github/license/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/blob/master/LICENSE)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/DistroAV/DistroAV/push.yaml?label=master)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/releases/latest) ![GitHub Release Date](https://img.shields.io/github/release-date/distroav/distroav?display_date=published_at)

[![Total downloads](https://img.shields.io/github/downloads/DistroAV/DistroAV/total)](https://github.com/DistroAV/DistroAV/releases)
![Discord](https://img.shields.io/discord/1082173788101279746?style=social&logo=discord&label=Discord&link=https%3A%2F%2Fdiscord.gg%2FZuTxbUK3ug)
[![Open Collective backers and sponsors](https://opencollective.com/distroav/all/badge.svg?label=Backers&color=brightgreen)](https://opencollective.com/distroav/donate)  
</div>

## DistroAV Features

### NDI Source

Receive NDI video and audio in OBS

<img width="400" alt="DistroAV-NDI-Source-Feature-Window" src="https://github.com/user-attachments/assets/fe8a3942-b4bf-42b5-84e6-e9971be95216" />

### NDI Output

Transmit OBS video and audio to NDI

<img width="400" alt="DistroAV-NDI-Output-Feature-Window" src="https://github.com/user-attachments/assets/4ee4dd5c-ff95-4b65-a32d-2064eaacc4c2" />

### NDI Filter

(a.k.a. **NDI Dedicated Output**)
Transmit a single OBS source or scene audio to NDI.

<img width="400" alt="DistroAV-NDI-Filters-Feature-Window" src="https://github.com/user-attachments/assets/6952da7a-a621-4b42-b737-857de83f5615" />

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

## Requirements

* [OBS v31.0 or higher](https://github.com/obsproject/obs-studio/releases) (Qt6, x64/ARM64/AppleSilicon)
* [NDI Runtime v6.0 or higher](https://github.com/DistroAV/DistroAV/wiki/1.-Installation#required---ndi-runtime)

# Troubleshooting

Got a DistroAV Error Code in your OBS log? [See the list of Error Code on the Wiki](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting#error--warning-code---obs-log)

Having trouble with DistroAV? See [Troubleshooting Wiki](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting)

Conflict with OBS-NDI plugin? [Follow the instructions](https://github.com/DistroAV/DistroAV/wiki/OBS%E2%80%90NDI-Is-Now-DistroAV)

# Development

See [Development Wiki](https://github.com/DistroAV/DistroAV/wiki/3.-Development)

**NOTE:** `OBS-NDI` was renamed to `DistroAV` ~2024/06 per [obsproject.com](https://obsproject.com)'s request to drop `OBS` from our name.

# Project Sponsors

<a href="https://distroav.org/sponsors/epeakstudio" target="_blank" rel="noopener">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://epeakstudio.com/wp-content/uploads/2020/04/LogoEpeak_BlackBG-thin.png"  width="400" />
    <source media="(prefers-color-scheme: light)" srcset="https://epeakstudio.com/wp-content/uploads/2024/01/LogoEpeak_WhiteBG-e1756350774910.png"  width="400" />
    <img alt="EPEAK Studio Logo - DistroAV Sponsor" src="https://epeakstudio.com/wp-content/uploads/2020/04/LogoEpeak_BlackBG-thin.png"  width="400" />
  </picture>
</a>

Project Management & Partners Relations & Apple/Microsoft Codesigning & Code contribution & Lab-testing for Release provided by [EPEAK Studio](https://distroav.org/sponsors/epeakstudio) for the official [DistroAV](https://distroav.org/) upstream project.

# Backers (Financial Contributiors)

This project can continue to exists thanks to all the people who help us cover regular expenses. [[Contribute](https://distroav.org/donate)].

[![DistroAV Backers](https://opencollective.com/DistroAV/backers.svg?avatarHeight=36&width=830&button=false)](https://distroav.org/donate)

# Lifetime Code Contributors

This project exists thanks to all the people who contributed code & reviews over the years. [[Contribute as a developer](https://github.com/DistroAV/DistroAV/wiki/3.-Development)].

[![DistroAV code contributors](https://opencollective.com/DistroAV/contributors.svg?button=false&limit=93&width=830)](https://github.com/DistroAV/DistroAV/graphs/contributors)

---

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date&theme=dark" />
  <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
</picture>
