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

#define DEFAULT_UPDATE_LOCAL_PORT 5002

/**
 * Loads and Saves configuration settings from/to:
 * Linux: ~/.config/obs-studio/global.ini
 * MacOS: ~/Library/Application Support/obs-studio/global.ini
 * Windows: %APPDATA%\obs-studio\global.ini
 * 
 * Example:
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

	/**
	 * -1 = `--DistroAV-update-force=-1` : force update less than current version
	 * 0 = update normally
	 * 1= `--DistroAV-update-force=1` : force update greater than current version
	 */
	static int UpdateForce;
	/**
	 * Default: DEFAULT_UPDATE_LOCAL_PORT
	 * `--DistroAV-update-local`
	 * `--DistroAV-update-local=5000`
	 */
	static int UpdateLocalPort;
	static bool UpdateLastCheckIgnore;
	/**
	 * -1 = `--DistroAV-detect-obsndi-force=off` : force OBS-NDI not detected
	 *  0 = detect normally
	 *  1 = `--DistroAV-detect-obsndi-force=on` : force OBS-NDI detected
	 */
	static int DetectObsNdiForce;

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

	void Save();

private:
	void Load();
	void SetDefaultsToGlobalStore();
	Config();
	static Config *_instance;
};
