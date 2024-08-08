set -e
./UninstallOBS-NDI.sh
echo "'Installing' OBS-NDI to OBS..."
cp -r release/RelWithDebInfo/obs-ndi.plugin ~/Library/Application\ Support/obs-studio/plugins/
