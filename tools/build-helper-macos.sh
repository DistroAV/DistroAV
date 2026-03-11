set -e

echo "Set CI Mac OS..."
cmake --preset macos
export CI=
export GITHUB_EVENT_NAME=push

trap 'unset CI GITHUB_EVENT_NAME' EXIT

echo "Building..."
# --skip-deps --verbose --debug
# .github/scripts/build-macos ${@}

# rm -rf ../release/Release && 
echo "Building DistroAV for Mac OS..."
./.github/scripts/build-macos -c Debug

echo "Packaging DistroAV for Mac OS..."
./.github/scripts/package-macos -c Debug