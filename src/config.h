/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <QDateTime>
#include <QString>
#include <QVersionNumber>

enum UpdateHostEnum {
	Production,
	LocalEmulator,
};

/**
 * Loads and Saves configuration settings from/to:
 * Linux: ~/.config/obs-studio/global.ini
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
	static Config *Current(bool load = true);
	static void Destroy();

	static bool LogVerbose();
	static bool LogDebug();
	static bool UpdateForce();
	static UpdateHostEnum UpdateHost();
	static int UpdateLocalPort();
	static bool UpdateLastCheckIgnore();
	static int DetectObsNdiForce();

	std::atomic<bool> OutputEnabled;
	QString OutputName;
	QString OutputGroups;
	std::atomic<bool> PreviewOutputEnabled;
	QString PreviewOutputName;
	QString PreviewOutputGroups;
	std::atomic<bool> TallyProgramEnabled;
	std::atomic<bool> TallyPreviewEnabled;

	bool AutoCheckForUpdates();
	void AutoCheckForUpdates(bool value);
	void SkipUpdateVersion(const QVersionNumber &version);
	QVersionNumber SkipUpdateVersion();
	QDateTime LastUpdateCheck();
	void LastUpdateCheck(const QDateTime &dateTime);
	int MinAutoUpdateCheckIntervalSeconds();
	void MinAutoUpdateCheckIntervalSeconds(int seconds);

	void Save();

private:
	void Load();
	Config();
	void SetDefaultsToGlobalStore();
	static Config *_instance;
};
