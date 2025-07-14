set -e

# Needed since 2025/01/17 refactor to get ./.github/scripts/build-* to work
# https://github.com/obsproject/obs-plugintemplate/commit/153962b3a2b0e7715db81dd61d82650f42287116
export CI=
export GITHUB_EVENT_NAME=

# The options after the 2025/01/17 refactor are:
#  -c|--config <config>  # Build configuration, e.g. Release, Debug
#  -s|--codesign         # Enable code signing
#  --debug               # Enable debug output
# See .github/scripts/build-macos `while (( # )) {` block for more details.

# Optionally uncomment to force a clean build
# ./Clean-macOS.sh

echo "Building..."
.github/scripts/build-macos ${@}
