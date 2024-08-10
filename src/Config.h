/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#pragma once

#include <QString>
#include <QVersionNumber>
#include <obs-module.h>

enum UpdateHostEnum {
	Production,
	LocalEmulator,
};

/**
 * Loads and Saves configuration settings from/to:
 * Linux: TBD...
 * MacOS: ~/Library/Application Support/obs-studio/global.ini
 * Windows: %APPDATA%\obs-studio\global.ini
 * 
 * ```
 * [NDIPlugin]
 * MainOutputEnabled=true
 * MainOutputName=OBS
 * PreviewOutputEnabled=false
 * PreviewOutputName=OBS Preview
 * TallyProgramEnabled=false
 * TallyPreviewEnabled=false
 * CheckForUpdates=true
 * AutoCheckForUpdates=true
 * MainOutputGroups=
 * PreviewOutputGroups=
 * ```
 */
class Config {
public:
	static Config *Current();
	static void Destroy();

	static bool LogVerbose();
	static bool LogDebug();
	static bool UpdateForce();
	static UpdateHostEnum UpdateHost();
	static int UpdateLocalPort();
	static bool UpdateLastCheckIgnore();

	Config *Load();
	Config *Save();

	bool OutputEnabled;
	QString OutputName;
	QString OutputGroups;
	bool PreviewOutputEnabled;
	QString PreviewOutputName;
	QString PreviewOutputGroups;
	bool TallyProgramEnabled;
	bool TallyPreviewEnabled;

	bool AutoCheckForUpdates();
	void AutoCheckForUpdates(bool value);
	void SkipUpdateVersion(const QVersionNumber &version);
	QVersionNumber SkipUpdateVersion();
	QDateTime LastUpdateCheck();
	void LastUpdateCheck(const QDateTime &dateTime);
	int MinAutoUpdateCheckIntervalSeconds();
	void MinAutoUpdateCheckIntervalSeconds(int seconds);

private:
	Config();
	static Config *_instance;
};
