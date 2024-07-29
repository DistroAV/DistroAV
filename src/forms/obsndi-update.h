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
#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

#include "ui_obsndi-update.h"

#define DEFAULT_UI_DELAY_MILLIS 1000
#define DEFAULT_MIN_AUTO_UPDATE_CHECK_INTERVAL_SECONDS (24 * 60 * 60)

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
	int uiDelayMillis = DEFAULT_UI_DELAY_MILLIS;
	int minAutoUpdateCheckIntervalSeconds =
		DEFAULT_MIN_AUTO_UPDATE_CHECK_INTERVAL_SECONDS;

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
