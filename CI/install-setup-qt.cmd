@echo off

REM Set default values to use AppVeyor's built-in Qt.
set QTDIR32=C:\Qt\5.7\msvc2013
set QTDIR64=C:\Qt\5.7\msvc2013_64
set QTCompileVersion=5.7.1

REM If the AppVeyor cache couldn't recover qt570.7z,
REM try to fetch Qt 5.7.0 from slepin.fr.
if not exist qt570.7z (
  curl -kLO https://www.slepin.fr/obs-plugins/deps/qt570.7z -f --retry 5 -C -
)

REM If qt570.7z exists now, use that instead of AppVeyor's built-in Qt.
if exist qt570.7z (
  7z x qt570.7z -o"Qt5.7.0"
  set QTDIR32=%CD%\Qt5.7.0\msvc2013
  set QTDIR64=%CD%\Qt5.7.0\msvc2013_64
  set QTCompileVersion=5.7.0
)
