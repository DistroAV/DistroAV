@echo off

xcopy C:\projects\obs-ndi\release\* C:\projects\obs-ndi\release-slobs /E /I
copy %QTDIR32%\bin\Qt5Core.dll %QTDIR32%\bin\Qt5Gui.dll %QTDIR32%\bin\Qt5Widgets.dll C:\projects\obs-ndi\release-slobs\obs-plugins\32bit\
copy %QTDIR64%\bin\Qt5Core.dll %QTDIR64%\bin\Qt5Gui.dll %QTDIR64%\bin\Qt5Widgets.dll C:\projects\obs-ndi\release-slobs\obs-plugins\64bit\