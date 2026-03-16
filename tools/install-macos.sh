#!/usr/bin/env bash
set -e
echo "'Uninstalling' DistroAV from OBS..."
rm -rf ~/Library/Application\ Support/obs-studio/plugins/distroav.*

echo "'Installing' DistroAV to OBS...
cp -r ./release/Debug/distroav.plugin ~/Library/Application\ Support/obs-studio/plugins/

echo "Done."
