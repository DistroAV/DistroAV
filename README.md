<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="./assets/distroav-full-clean-white.svg">
  <source media="(prefers-color-scheme: light)" srcset="./assets/distroav-full-clean.svg">
  <img width="560" alt="DistroAV logo" src="./assets/distroav-full-clean.svg">
</picture>

### Network audio & video for OBS Studio, powered by NDI®

DistroAV is an OBS Studio plugin that sends and receives video and audio over your network using NDI®.<br>
Bring in feeds from cameras, computers, or other apps, and share your OBS scenes with any NDI-compatible device — no capture cards needed.

[![License](https://img.shields.io/github/license/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/blob/master/LICENSE)
[![Build](https://img.shields.io/github/actions/workflow/status/DistroAV/DistroAV/push.yaml?label=master)](https://github.com/DistroAV/DistroAV/actions)
[![Latest release](https://img.shields.io/github/v/release/DistroAV/DistroAV)](https://github.com/DistroAV/DistroAV/releases/latest)
[![Release date](https://img.shields.io/github/release-date/distroav/distroav?display_date=published_at)](https://github.com/DistroAV/DistroAV/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/DistroAV/DistroAV/total)](https://github.com/DistroAV/DistroAV/releases)
[![Discord](https://img.shields.io/discord/1082173788101279746?logo=discord&logoColor=white&label=Discord&color=5865F2)](https://discord.gg/ZuTxbUK3ug)
[![Backers](https://opencollective.com/distroav/all/badge.svg?label=Backers&color=brightgreen)](https://opencollective.com/distroav/donate)

[**Download**](https://distroav.org/download) ·
[**Wiki**](https://github.com/DistroAV/DistroAV/wiki) ·
[**Troubleshooting**](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting) ·
[**Discord**](https://discord.gg/ZuTxbUK3ug) ·
[**Donate**](https://distroav.org/donate)

</div>

---

## ✨ Features

<table>
  <tr>
    <th width="33%">⬅️ NDI Source</th>
    <th width="33%">➡️ NDI Output</th>
    <th width="33%">↗️ NDI Filter</th>
  </tr>
  <tr>
    <td>Receive NDI video and audio in OBS.</td>
    <td>Transmit OBS program video and audio to NDI.</td>
    <td>A.k.a. <b>NDI Dedicated Output</b> — transmit a single OBS source or scene to NDI.</td>
  </tr>
  <tr>
    <td><img alt="NDI Source settings window" src="https://github.com/user-attachments/assets/fe8a3942-b4bf-42b5-84e6-e9971be95216" /></td>
    <td><img alt="NDI Output settings window" src="https://github.com/user-attachments/assets/4ee4dd5c-ff95-4b65-a32d-2064eaacc4c2" /></td>
    <td><img alt="NDI Filter settings window" src="https://github.com/user-attachments/assets/6952da7a-a621-4b42-b737-857de83f5615" /></td>
  </tr>
</table>

## 📋 Requirements

| Dependency | Version |
| --- | --- |
| [OBS Studio](https://github.com/obsproject/obs-studio/releases) | **31.1.1** or higher (Qt6 — x64, ARM64, Apple Silicon) |
| [NDI Runtime](https://github.com/DistroAV/DistroAV/wiki/1.-Installation#required---ndi-runtime) | **6.3** or higher |

## 🚀 Installation

### Windows &nbsp;[![WinGet](https://img.shields.io/winget/v/DistroAV.DistroAV)](https://winstall.app/apps/DistroAV.DistroAV)

```powershell
winget install DistroAV.DistroAV
```

Or with [Chocolatey](https://community.chocolatey.org/packages/distroav) &nbsp;[![Chocolatey](https://img.shields.io/chocolatey/v/distroav)](https://community.chocolatey.org/packages/distroav)

```powershell
choco install distroav
```

### macOS &nbsp;![Homebrew](https://img.shields.io/badge/Homebrew-FBB040?logo=homebrew&logoColor=fff)

```sh
brew tap distroav/tap
brew trust distroav/tap
brew install --cask distroav/tap/distroav
```

### Linux — Flatpak &nbsp;[![Flathub](https://img.shields.io/flathub/v/com.obsproject.Studio.Plugin.DistroAV)](https://flathub.org/apps/com.obsproject.Studio.Plugin.DistroAV)

```sh
flatpak install com.obsproject.Studio com.obsproject.Studio.Plugin.DistroAV
sudo flatpak override com.obsproject.Studio --system-talk-name=org.freedesktop.Avahi
```

<sub>Maintained by [@tytan652](https://github.com/tytan652)</sub>

### Linux — Debian / Ubuntu (.deb) &nbsp;[![.deb](https://img.shields.io/github/v/release/DistroAV/DistroAV?label=.deb&logo=ubuntu)](https://github.com/DistroAV/DistroAV/releases/latest)

Download `distroav-<version>-x86_64-linux-gnu.deb` from the [latest release](https://github.com/DistroAV/DistroAV/releases/latest), then:

```sh
sudo dpkg -i distroav-*-x86_64-linux-gnu.deb
```

If it reports missing dependencies, run `sudo apt -f install` to fix them.

### Linux — Ubuntu &nbsp;[![Ubuntu](https://img.shields.io/ubuntu/v/distroav)](https://packages.ubuntu.com/source/resolute/distroav)

```sh
sudo apt install distroav
```

<sub>Maintained by [@eeickmeyer](https://github.com/eeickmeyer)</sub>


### Other options

Manual installers and other platforms are on the [download page](https://distroav.org/download). For step-by-step help, see the [Installation wiki](https://github.com/DistroAV/DistroAV/wiki/1.-Installation).

## 🛠️ Troubleshooting

- **Error code in your OBS log?** → [DistroAV error code reference](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting#error--warning-code---obs-log)
- **Something not working?** → [Troubleshooting wiki](https://github.com/DistroAV/DistroAV/wiki/2.-Troubleshooting)
- **Coming from OBS-NDI?** → DistroAV is its new name (since June 2024). Follow the [migration instructions](https://github.com/DistroAV/DistroAV/wiki/OBS%E2%80%90NDI-Is-Now-DistroAV) to avoid conflicts.

## 👩‍💻 Development

Build instructions and contribution guidelines live in the [Development wiki](https://github.com/DistroAV/DistroAV/wiki/3.-Development) and [CONTRIBUTING.md](CONTRIBUTING.md).

Also see [DistroAV AI usage guideline & disclosure](AI_DISCLOSURE.md)

## 💖 Sponsors

<a href="https://distroav.org/sponsors/epeakstudio" target="_blank" rel="noopener">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://epeakstudio.com/wp-content/uploads/2020/04/LogoEpeak_BlackBG-thin.png" width="360" />
    <source media="(prefers-color-scheme: light)" srcset="https://epeakstudio.com/wp-content/uploads/2024/01/LogoEpeak_WhiteBG-e1756350774910.png" width="360" />
    <img alt="EPEAK Studio logo — DistroAV sponsor" src="https://epeakstudio.com/wp-content/uploads/2020/04/LogoEpeak_BlackBG-thin.png" width="360" />
  </picture>
</a>

Project management, partner relations, Apple/Microsoft code signing, code contributions, and release lab-testing are provided by [EPEAK Studio](https://distroav.org/sponsors/epeakstudio) for the official [DistroAV](https://distroav.org/) upstream project.

### Backers

DistroAV keeps running thanks to everyone who helps cover its ongoing expenses. **[Become a backer →](https://distroav.org/donate)**

[![DistroAV backers](https://opencollective.com/DistroAV/backers.svg?avatarHeight=36&width=830&button=false)](https://distroav.org/donate)

## 🤝 Contributors

DistroAV exists thanks to everyone who has contributed code and reviews over the years. **[Start contributing →](https://github.com/DistroAV/DistroAV/wiki/3.-Development)**

[![DistroAV code contributors](https://opencollective.com/DistroAV/contributors.svg?button=false&limit=93&width=830)](https://github.com/DistroAV/DistroAV/graphs/contributors)

## ⭐ Star history

<a href="https://star-history.com/#DistroAV/DistroAV&Date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
    <img alt="DistroAV star history chart" src="https://api.star-history.com/svg?repos=DistroAV/DistroAV&type=Date" />
  </picture>
</a>

---

<div align="center">
<sub>NDI® is a registered trademark of Vizrt NDI AB. DistroAV is not affiliated with or endorsed by the OBS Project or Vizrt.</sub>
</div>
