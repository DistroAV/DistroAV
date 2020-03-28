@echo off
SETLOCAL EnableDelayedExpansion

REM Check if obs-studio build exists.
REM If the obs-studio directory does exist, check if the last OBS tag built
REM matches the latest OBS tag.
REM If the tags match, do not build obs-studio.
REM If the tags do not match, build obs-studio.
REM If the obs-studio directory doesn't exist, build obs-studio.
echo Checking for obs-studio build...

set OBSLatestTagPrePull=0
set OBSLatestTagPostPull=0
echo Latest tag pre-pull: %OBSLatestTagPrePull%
echo Latest tag post-pull: %OBSLatestTagPostPull%

REM Set up the build flag as undefined.
set "BuildOBS="

REM Check the last tag successfully built by CI.
if exist "%OBSPath%\obs-studio-last-tag-built.txt" (
	set /p OBSLastTagBuilt=<"%OBSPath%\obs-studio-last-tag-built.txt"
) else (
	set OBSLastTagBuilt=0
)

REM If obs-studio directory exists, run git pull and get the latest tag number.
if exist %OBSPath% (
	echo obs-studio directory exists
	echo   Updating tag info
	cd /D %OBSPath%
	git describe --tags --abbrev=0 --exclude="*-rc*" > "%OBSPath%\latest-obs-studio-tag-pre-pull.txt"
	set /p OBSLatestTagPrePull=<"%OBSPath%\latest-obs-studio-tag-pre-pull.txt"
	git checkout master
	git pull
	git describe --tags --abbrev=0 --exclude="*-rc*" > "%OBSPath%\latest-obs-studio-tag-post-pull.txt"
	set /p OBSLatestTagPostPull=<"%OBSPath%\latest-obs-studio-tag-post-pull.txt"
	set /p OBSLatestTag=<"%OBSPath%\latest-obs-studio-tag-post-pull.txt"
	echo %OBSLatestTagPostPull%> "%OBSPath%\latest-obs-studio-tag.txt"
)

REM Check the obs-studio tags for mismatches.
REM If a new tag was pulled, set the build flag.
if not %OBSLatestTagPrePull%==%OBSLatestTagPostPull% (
	echo Latest tag pre-pull: %OBSLatestTagPrePull%
	echo Latest tag post-pull: %OBSLatestTagPostPull%
	echo Tags do not match.  Need to rebuild OBS.
	set BuildOBS=true
)

REM If the latest git tag doesn't match the last built tag, set the build flag.
if not %OBSLatestTagPostPull%==%OBSLastTagBuilt% (
	echo Last built OBS tag: %OBSLastTagBuilt%
	echo Latest tag post-pull: %OBSLatestTagPostPull%
	echo Tags do not match.  Need to rebuild OBS.
	set BuildOBS=true
)

REM If obs-studio directory does not exist, clone the git repo, get the latest
REM tag number, and set the build flag.
if not exist %OBSPath% (
	echo obs-studio directory does not exist
	git clone https://github.com/obsproject/obs-studio %OBSPath%
	cd /D %OBSPath%\
	git describe --tags --abbrev=0 --exclude="*-rc*" > "%OBSPath%\obs-studio-latest-tag.txt"
	set /p OBSLatestTag=<"%OBSPath%\obs-studio-latest-tag.txt"
	set BuildOBS=true
)

REM If the needed obs-studio libs for this build_config do not exist,
REM set the build flag.
if not exist %OBSPath%\build32\libobs\%build_config%\obs.lib (
	echo obs-studio\build32\libobs\%build_config%\obs.lib does not exist
	set BuildOBS=true
)
if not exist %OBSPath%\build32\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib (
	echo obs-studio\build32\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib does not exist
	set BuildOBS=true
)

REM Some debug info
echo:
echo Latest tag pre-pull: %OBSLatestTagPrePull%
echo Latest tag post-pull: %OBSLatestTagPostPull%
echo Latest tag: %OBSLatestTag%
echo Last built OBS tag: %OBSLastTagBuilt%

if defined BuildOBS (
	echo BuildOBS: true
) else (
	echo BuildOBS: false
)
echo:

REM If the build flag is set, build obs-studio.
if defined BuildOBS (
	echo Building obs-studio...
    cd /D %OBSPath%
	echo   git checkout %OBSLatestTag%
	git checkout %OBSLatestTag%
	echo:
	
    echo   Removing previous build dirs...
	if exist build32 rmdir /s /q "%OBSPath%\build32"
	if exist build64 rmdir /s /q "%OBSPath%\build64"
	
    echo   Making new build dirs...
	mkdir build32
	mkdir build64
	
    echo   Running cmake for obs-studio %OBSLatestTag% 32-bit...
	cd build32
	cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR32%" -DDepsPath="%DepsPath32%" -DBUILD_CAPTIONS=true -DDISABLE_PLUGINS=true -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true ..
	echo:
	echo:
	
    echo   Running cmake for obs-studio %OBSLatestTag% 64-bit...
	cd ..\build64
	cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR64%" -DDepsPath="%DepsPath64%" -DBUILD_CAPTIONS=true -DDISABLE_PLUGINS=true -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true ..
	echo:
	echo:
	
    REM echo   Building obs-studio %OBSLatestTag% 32-bit ^(Build Config: %build_config%^)...
	REM call msbuild /m /p:Configuration=%build_config% %OBSPath%\build32\obs-studio.sln
	
    REM echo   Building obs-studio %OBSLatestTag% 64-bit ^(Build Config: %build_config%^)...
	REM call msbuild /m /p:Configuration=%build_config% %OBSPath%\build64\obs-studio.sln
	
    cd ..
	git describe --tags --abbrev=0 > "%OBSPath%\obs-studio-last-tag-built.txt"
	set /p OBSLastTagBuilt=<"%OBSPath%\obs-studio-last-tag-built.txt"
) else (
	echo Last OBS tag built is:  %OBSLastTagBuilt%
	echo No need to rebuild OBS.
)

dir "%OBSPath%\libobs"
