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

#include <obs.hpp>
#include <QMainWindow>

#include "PreviewSceneChangedWatcher.h"

PreviewSceneChangedWatcher::PreviewSceneChangedWatcher(void(*func)(enum obs_frontend_event event, void* param), void* param, QObject* parent) :
	QObject(parent),
	_pHandlerFunc(func),
	_pFuncParam(param)
{
	QMainWindow* main = (QMainWindow*)obs_frontend_get_main_window();
	_pScenesList = main->findChild<QListWidget*>("scenes");
	connect(
		_pScenesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
		this, SLOT(onCurrentItemChanged(QListWidgetItem*, QListWidgetItem*))
	);

	obs_frontend_add_event_callback(PreviewSceneChangedWatcher::onFrontendEvent, this);
}

PreviewSceneChangedWatcher::~PreviewSceneChangedWatcher()
{
	disconnect(
		_pScenesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
		this, SLOT(onCurrentItemChanged(QListWidgetItem*, QListWidgetItem*))
	);
	obs_frontend_remove_event_callback(PreviewSceneChangedWatcher::onFrontendEvent, this);
}

void PreviewSceneChangedWatcher::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
	if (obs_frontend_preview_program_mode_active() && _pHandlerFunc != nullptr) {
		_pHandlerFunc(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED, _pFuncParam);
	}
}

void PreviewSceneChangedWatcher::onFrontendEvent(enum obs_frontend_event event, void* param)
{
	PreviewSceneChangedWatcher* instance = (PreviewSceneChangedWatcher*)param;
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED && obs_frontend_preview_program_mode_active()) {
		// Swapping disables events on the scenes we're hooked on, so we need this hack
		instance->onCurrentItemChanged(nullptr, nullptr);
	}
}
