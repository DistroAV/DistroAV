set -e
./UninstallDistroAV.sh
echo "'Installing' DistroAV to OBS..."
cp -r release/RelWithDebInfo/distroav.plugin ~/Library/Application\ Support/obs-studio/plugins/
