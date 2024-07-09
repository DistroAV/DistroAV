**NOTE:** `OBS-NDI` was renamed to `DistroAV` ~2024/06 per [obsproject.com](https://obsproject.com)'s request to drop `OBS` from our name.

DistroAV (Formerly OBS-NDI)
==============
Network Audio/Video in OBS-Studio using NDI technology

## Discord
[![Discord Shield](https://discordapp.com/api/guilds/1082173788101279746/widget.png?style=banner3)](https://discord.gg/ZuTxbUK3ug)  
(English Speaking)

[Donations](https://opencollective.com/obs-ndi)

## Status
`master`: [![master: Push](https://github.com/obs-ndi/obs-ndi/actions/workflows/push.yaml/badge.svg)](https://github.com/obs-ndi/obs-ndi/actions/workflows/push.yaml)
`develop`: [![develop: Pull Request](https://github.com/obs-ndi/obs-ndi/actions/workflows/pr-pull.yaml/badge.svg?branch=develop)](https://github.com/obs-ndi/obs-ndi/actions/workflows/pr-pull.yaml)

## Features
- **NDI Source** : receive NDI video and audio in OBS
- **NDI Output** : transmit OBS video and audio to NDI
- **NDI Filter** (a.k.a NDI Dedicated Output) : transmit a single OBS source or scene audio to NDI

DistroAV supports NDI High Bandwidth (encodes/decodes/sends/receives) and can decodes/receives NDI HX2.

## What is NDI ?
- NDI is a network protocol that allows low-latency, high-quality Audio-Video stream on local network
- A growing number of devices integrate NDI (PTZ Camera, Encoder, Decoder, etc)

This it NOT a stream distribution protocol. It is mostly aimed at local network use due to bndwidth requirement

## Requirements
* OBS >= 30.0.0 (Qt6 & x64)
* NDI 6 Runtime  
  We are not allowed to directly distribute the NDI runtime here.  
  See [#Installation](#installation).

# Installation

See [Installation Wiki](https://github.com/obs-ndi/obs-ndi/wiki/1.-Installation)

# Troubleshooting

See [Troubleshooting Wiki](https://github.com/obs-ndi/obs-ndi/wiki/2.-Troubleshooting)

# Development

See [Development Wiki](https://github.com/obs-ndi/obs-ndi/wiki/3.-Development)
