/*
obs-ndi
Copyright (C) 2016-2023 Stéphane Lepin <stephane.lepin@gmail.com>

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

#include <QString>
#include <obs-module.h>

class Config {
public:
	Config();
	static void OBSSaveCallback(obs_data_t *save_data, bool saving,
				    void *private_data);
	static Config *Current();
	void Load();
	void Save();

	bool OutputEnabled;
	QString OutputName;
	bool PreviewOutputEnabled;
	QString PreviewOutputName;
	bool TallyProgramEnabled;
	bool TallyPreviewEnabled;

private:
	static Config *_instance;
};

#endif // CONFIG_H
