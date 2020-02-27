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

// TODO turn into a QObject to provide signals
class Config {
  public:
	Config();
	static void OBSSaveCallback(obs_data_t* save_data,
		bool saving, void* private_data);
	void load();
	void save();

	// TODO add config change signal

	bool mainOutputEnabled() {
		return _mainOutputEnabled;
	}
	void setMainOutputEnabled(bool enabled) {
		_mainOutputEnabled = enabled;
	}

	const QString& mainOutputName() {
		return _mainOutputName;
	}
	void setMainOutputName(const QString& name) {
		_mainOutputName = name;
	}

	bool previewOutputEnabled() {
		return _previewOutputEnabled;
	}
	void setPreviewOutputEnabled(bool enabled) {
		_previewOutputEnabled = enabled;
	}

	const QString& previewOutputName() {
		return _previewOutputName;
	}
	void setPreviewOutputName(const QString& name) {
		_previewOutputName = name;
	}

  private:
	bool _mainOutputEnabled;
	QString _mainOutputName;
	bool _previewOutputEnabled;
	QString _previewOutputName;
};

#endif // CONFIG_H
