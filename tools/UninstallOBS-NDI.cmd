rem @ECHO off
echo "'Uninstalling' OBS-NDI from OBS..."
del "%ProgramFiles%\obs-studio\obs-plugins\64bit\obs-ndi.*"
rd /q /s "%ProgramFiles%\obs-studio\data\obs-plugins\obs-ndi"
