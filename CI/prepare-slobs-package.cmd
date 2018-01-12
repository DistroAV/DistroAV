@echo off

xcopy C:\projects\obs-ndi\release\* C:\projects\obs-ndi\release-slobs /E /I

rmdir /S /Q "C:\projects\obs-ndi\release-slobs\obs-plugins\32bit"
xcopy C:\projects\obs-ndi\release-slobs\obs-plugins\64bit\* C:\projects\obs-ndi\release-slobs\obs-plugins\ /E /I

copy %QTDIR32%\bin\Qt5Core.dll C:\projects\obs-ndi\release-slobs\obs-plugins\32bit\
copy %QTDIR32%\bin\Qt5Gui.dll C:\projects\obs-ndi\release-slobs\obs-plugins\32bit\
copy %QTDIR32%\bin\Qt5Widgets.dll C:\projects\obs-ndi\release-slobs\obs-plugins\32bit\

copy %QTDIR64%\bin\Qt5Core.dll C:\projects\obs-ndi\release-slobs\obs-plugins\64bit\
copy %QTDIR64%\bin\Qt5Gui.dll C:\projects\obs-ndi\release-slobs\obs-plugins\64bit\
copy %QTDIR64%\bin\Qt5Widgets.dll C:\projects\obs-ndi\release-slobs\obs-plugins\64bit\