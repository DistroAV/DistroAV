@ECHO off
pushd "%PROGRAMFILES%\obs-studio\bin\64bit"
cd
rem --obs-ndi-update-force --obs-ndi-update-local --obs-ndi-update-last-check-ignore --obs-ndi-debug
start obs64.exe --verbose %*
popd
