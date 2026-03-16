#!/usr/bin/env bash
set -e
# /Applications/OBS.app/Contents/MacOS/OBS --verbose --distroav-verbose --distroav-debug --distroav-update-local --distroav-update-force --distroav-update-last-check-ignore
/Applications/OBS.app/Contents/MacOS/OBS --distroav-verbose --distroav-debug --distroav-update-local --distroav-update-force --distroav-update-last-check-ignore
# /Applications/OBS.app/Contents/MacOS/OBS ${@}
