set -e
echo "Building..."
# --skip-deps --verbose --debug
# .github/scripts/build-macos ${@}

# rm -rf ../release/Release && 
./.github/scripts/build-macos -c Debug && ./.github/scripts/package-macos -c Debug

