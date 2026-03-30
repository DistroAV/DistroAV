@ECHO off
pushd "%PROGRAMFILES%\obs-studio\bin\64bit"
cd
rem --distroav-update-force --distroav-update-local --distroav-update-last-check-ignore --distroav-debug
rem --distroav-check-ndilib-forcefail --distroav-check-obs-forcefail
rem --distroav-check-ndilib-ignore --distroav-check-obs-ignore
rem start obs64.exe --verbose --distroav-update-force --distroav-update-local --distroav-update-last-check-ignore --distroav-debug
start obs64.exe --distroav-update-last-check-ignore --distroav-debug --distroav-check-ndilib-ignore --distroav-check-obs-ignore
popd
