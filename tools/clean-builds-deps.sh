#!/bin/bash
set -e

echo "Cleaning previous build..."
rm -rf build_macos
rm -rf release

echo "Removing obs-deps..."
rm -rf obs-deps

echo "Done."
