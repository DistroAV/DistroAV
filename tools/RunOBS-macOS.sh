set -e
#/Applications/OBS.app/Contents/MacOS/OBS ${@}

# Dev debug options; Comment out the above wildcard
/Applications/OBS.app/Contents/MacOS/OBS --distroav-debug
#/Applications/OBS.app/Contents/MacOS/OBS --distroav-debug --distroav-update-force --distroav-update-last-check-ignore
#/Applications/OBS.app/Contents/MacOS/OBS --distroav-debug --distroav-update-force --distroav-update-last-check-ignore --distroav-update-local

# Deprecated, but here for old "OBS-NDI" reasons
#/Applications/OBS.app/Contents/MacOS/OBS --obs-ndi-debug --obs-ndi-update-force --obs-ndi-update-last-check-ignore --obs-ndi-update-local 
