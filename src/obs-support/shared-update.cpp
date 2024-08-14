/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
	Copyright (C) 2024 DistroAV <contact@distroav.org>

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

/******************************************************************************
Select methods copied from https://github.com/obsproject/obs-studio/blob/master/UI/update/shared-update.cpp
**The original file had/has no copyright header; added one.**
In some places just the method signature is [mostly] copied.
In some places [nearly] the full code implementation is copied.
******************************************************************************/

#include "shared-update.hpp"

#include "obs-app.hpp"
#include "plugin-support.h"

#include <util/config-file.h>

#include <QCryptographicHash>
#include <QFile>
#include <QRandomGenerator>

#include <mutex>

// Changed to use QCryptographicHash::Sha256 and QString
bool CalculateFileHash(const char *path, QString &hash)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		obs_log(LOG_WARNING,
			"CalculateFileHash: Failed to open file: `%s`", path);
		return false;
	}

	QCryptographicHash qhash(QCryptographicHash::Sha256);
	if (!qhash.addData(&file)) {
		obs_log(LOG_WARNING,
			"CalculateFileHash: Failed to read data from file: `%s`",
			path);
		return false;
	}

	hash = qhash.result().toHex();

	return true;
}

// Changed to use QString
void GenerateGUID(QString &guid)
{
	const char alphabet[] = "0123456789abcdef";
	QRandomGenerator *rng = QRandomGenerator::system();

	guid.resize(40);

	for (size_t i = 0; i < 40; i++) {
		guid[i] = alphabet[rng->bounded(0, 16)];
	}
}

// Changed to return QString
QString GetProgramGUID()
{
	static std::mutex m;
	std::lock_guard<std::mutex> lock(m);

	/* NOTE: this is an arbitrary random number that we use to count the
	 * number of unique OBS installations and is not associated with any
	 * kind of identifiable information */
	/* Stored in
	 * Linux: ~/.config/obs-studio/global.ini
	 * MacOS: ~/Library/Application Support/obs-studio/global.ini
	 * Windows: %APPDATA%\obs-studio\global.ini
	 */
	QString guid =
		config_get_string(GetGlobalConfig(), "General", "InstallGUID");
	if (guid.isEmpty()) {
		GenerateGUID(guid);
		if (!guid.isEmpty())
			config_set_string(GetGlobalConfig(), "General",
					  "InstallGUID", QT_TO_UTF8(guid));
	}
	return guid;
}
