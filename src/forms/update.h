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

#include "ui_update.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

class PluginUpdateInfo {
public:
	PluginUpdateInfo(const QString &responseData, const QString &errorData);

	QString responseData;
	QString errorData;

	QJsonDocument jsonDocument;
	QJsonObject jsonObject;

	int infoVersion = -1;
	QString releaseTag;
	QString releaseName;
	QString releaseUrl;
	QString releaseDate;
	QString releaseNotes;
	int uiDelayMillis = 1000;

	bool fakeVersionLatest = false;
	QVersionNumber versionLatest;
	QVersionNumber versionCurrent;
};

/**
 * @return true if the callback handled the update check response, otherwise false
 */
typedef std::function<bool(const PluginUpdateInfo &pluginUpdateInfo)>
	UserRequestCallback;

void updateCheckStop();
bool updateCheckStart(UserRequestCallback userRequestCallback = nullptr);
