set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$SCRIPT_DIR/UninstallDistroAV-macOS.sh"

CONFIG=${1:-RelWithDebInfo}

echo "'Installing' DistroAV to OBS..."
cp -r release/$CONFIG/distroav.plugin ~/Library/Application\ Support/obs-studio/plugins/
