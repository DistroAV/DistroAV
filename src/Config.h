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

#ifndef CONFIG_H
#define CONFIG_H

#include <atomic>
#include <QString>
#include <util/config-file.h>

struct Config {
	Config();
	void Load();
	void Save();
	void SetDefaultsToGlobalStore();
	static config_t *GetConfigStore();

	std::atomic<bool> OutputEnabled = false;
	QString OutputName;
	std::atomic<bool> PreviewOutputEnabled = false;
	QString PreviewOutputName;
	std::atomic<bool> TallyProgramEnabled = true;
	std::atomic<bool> TallyPreviewEnabled = true;

	// Do not persist this to storage
	std::atomic<bool> VerboseLog = false;
};

#endif // CONFIG_H
