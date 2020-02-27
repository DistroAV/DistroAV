/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <QtCore/QString>
#include <obs-module.h>

class Config {
  public:
	Config();
	static void OBSSaveCallback(obs_data_t* save_data,
		bool saving, void* private_data);
	void load();
	void save();

	bool mainOutputEnabled() {
		return _mainOutputEnabled;
	}
	void setMainOutputEnabled(bool enabled) {
		_mainOutputEnabled = enabled;
	}

	const char* mainOutputName() {
		return _mainOutputName.c_str();
	}
	void setMainOutputName(const char* name) {
		_mainOutputName = name;
	}

	bool previewOutputEnabled() {
		return _previewOutputEnabled;
	}
	void setPreviewOutputEnabled(bool enabled) {
		_previewOutputEnabled = enabled;
	}

	const char* previewOutputName() {
		return _previewOutputName.c_str();
	}
	void setPreviewOutputName(const char* name) {
		_previewOutputName = name;
	}

  private:
	bool _mainOutputEnabled;
	std::string _mainOutputName;
	bool _previewOutputEnabled;
	std::string _previewOutputName;
};

#endif // CONFIG_H
