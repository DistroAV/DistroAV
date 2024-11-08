@ECHO off
pushd "%PROGRAMFILES%\obs-studio\bin\64bit"
cd
rem --distroav-update-force --distroav-update-local --distroav-update-last-check-ignore --distroav-debug
start obs64.exe --verbose %*
popd
