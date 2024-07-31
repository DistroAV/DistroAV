set -e
./UninstallDistroAV.sh
echo "'Installing' DistroAV to OBS..."
cp -r release/RelWithDebInfo/DistroAV.plugin ~/Library/Application\ Support/obs-studio/plugins/
