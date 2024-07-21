/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <distroav@distroav.org>

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

#include <QString>
#include <QVersionNumber>

class Config {
public:
	static Config *Current();
	static void Destroy();

	static bool LogVerbose();
	static bool LogDebug();
	static bool UpdateForce();

	void Load();
	void Save();

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

private:
	Config();
	static Config *_instance;
};
