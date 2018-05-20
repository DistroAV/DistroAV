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

#pragma once

#include <obs-frontend-api.h>
#include <QObject>
#include <QListWidget>

class PreviewSceneChangedWatcher : public QObject
{
Q_OBJECT
public:
	explicit PreviewSceneChangedWatcher(void(*func)(enum obs_frontend_event event, void* param), void* param, QObject* parent = Q_NULLPTR);
	~PreviewSceneChangedWatcher();
	static void onFrontendEvent(enum obs_frontend_event event, void* param);
private slots:
	void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
private:
	QListWidget * _pScenesList;
	void (*_pHandlerFunc)(enum obs_frontend_event event, void* param);
	void* _pFuncParam;
};