set -e
echo "Building..."
# --skip-deps --verbose --debug
.github/scripts/build-macos ${@}
