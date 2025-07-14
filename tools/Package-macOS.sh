set -e

# Needed since 2025/01/17 refactor to get ./.github/scripts/package-* to work
# https://github.com/obsproject/obs-plugintemplate/commit/153962b3a2b0e7715db81dd61d82650f42287116
export CI=
export GITHUB_EVENT_NAME=

echo "Packaging..."
.github/scripts/package-macos
