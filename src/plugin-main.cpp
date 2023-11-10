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

#ifdef _WIN32
#include <Windows.h>
#endif

#include <sys/stat.h>

#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QLibrary>
#include <QMainWindow>
#include <QAction>
#include <QMessageBox>
#include <QString>
#include <QStringList>

#include "plugin-main.h"
#include "main-output.h"
#include "preview-output.h"
#include "Config.h"
#include "forms/output-settings.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

const char *obs_module_name()
{
	return "obs-ndi";
}

const char *obs_module_description()
{
	return "NDI input/output integration for OBS Studio";
}

const NDIlib_v4 *ndiLib = nullptr;

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

extern struct obs_source_info create_ndi_filter_info();
struct obs_source_info ndi_filter_info;

extern struct obs_source_info create_ndi_audiofilter_info();
struct obs_source_info ndi_audiofilter_info;

extern struct obs_source_info create_alpha_filter_info();
struct obs_source_info alpha_filter_info;

const NDIlib_v5 *load_ndilib();

typedef const NDIlib_v5 *(*NDIlib_v5_load_)(void);
QLibrary *loaded_lib = nullptr;

NDIlib_find_instance_t ndi_finder = nullptr;

OutputSettings *output_settings = nullptr;

bool obs_module_load(void)
{
	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: you can haz obs-ndi (Version %s)",
	     PLUGIN_VERSION);
	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: Qt version (compile-time): %s | Qt version (run-time): %s",
	     QT_VERSION_STR, qVersion());

	QMainWindow *main_window =
		(QMainWindow *)obs_frontend_get_main_window();

	ndiLib = load_ndilib();
	if (!ndiLib) {
		blog(LOG_ERROR,
		     "[obs-ndi] obs_module_load: load_ndilib() failed; Module won't load.");

		const char *msg_string_name = "";
#ifdef _MSC_VER
		// Windows
		msg_string_name = "NDIPlugin.LibError.Message.Win";
#else
#ifdef __APPLE__
		// macOS / OS X
		msg_string_name = "NDIPlugin.LibError.Message.macOS";
#else
		// Linux
		msg_string_name = "NDIPlugin.LibError.Message.Linux";
#endif
#endif

		QMessageBox::critical(
			main_window,
			obs_module_text("NDIPlugin.LibError.Title"),
			obs_module_text(msg_string_name), QMessageBox::Ok,
			QMessageBox::NoButton);
		return false;
	}

	if (!ndiLib->initialize()) {
		blog(LOG_ERROR,
		     "[obs-ndi] obs_module_load: ndiLib->initialize() failed; CPU unsupported by NDI library. Module won't load.");
		return false;
	}

	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: NDI library initialized successfully ('%s')",
	     ndiLib->version());

	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	ndi_finder = ndiLib->find_create_v2(&find_desc);

	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);

	ndi_filter_info = create_ndi_filter_info();
	obs_register_source(&ndi_filter_info);

	ndi_audiofilter_info = create_ndi_audiofilter_info();
	obs_register_source(&ndi_audiofilter_info);

	alpha_filter_info = create_alpha_filter_info();
	obs_register_source(&alpha_filter_info);

	if (main_window) {
		Config *conf = Config::Current();
		conf->Load();

		preview_output_init(
			conf->PreviewOutputName.toUtf8().constData());

		// Ui setup
		QAction *menu_action =
			(QAction *)obs_frontend_add_tools_menu_qaction(
				obs_module_text(
					"NDIPlugin.Menu.OutputSettings"));

		obs_frontend_push_ui_translation(obs_module_get_string);
		output_settings = new OutputSettings(main_window);
		obs_frontend_pop_ui_translation();

		auto menu_cb = [] { output_settings->ToggleShowHide(); };
		menu_action->connect(menu_action, &QAction::triggered, menu_cb);

		obs_frontend_add_event_callback(
			[](enum obs_frontend_event event, void *private_data) {
				if (event ==
				    OBS_FRONTEND_EVENT_FINISHED_LOADING) {
#if defined(__linux__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
					Config *conf = static_cast<Config *>(
						private_data);
#if defined(__linux__)
#pragma GCC diagnostic pop
#endif
					if (conf->OutputEnabled) {
						main_output_start(
							conf->OutputName
								.toUtf8()
								.constData());
					}
					if (conf->PreviewOutputEnabled) {
						preview_output_start(
							conf->PreviewOutputName
								.toUtf8()
								.constData());
					}
				} else if (event == OBS_FRONTEND_EVENT_EXIT) {
					preview_output_stop();
					main_output_stop();

					preview_output_deinit();
				}
			},
			static_cast<void *>(conf));
	}

	return true;
}

void obs_module_post_load(void)
{
	blog(LOG_INFO, "[obs-ndi] obs_module_post_load: ...");
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[obs-ndi] +obs_module_unload()");

	if (ndiLib) {
		if (ndi_finder) {
			ndiLib->find_destroy(ndi_finder);
			ndi_finder = nullptr;
		}
		ndiLib->destroy();
		ndiLib = nullptr;
	}

	if (loaded_lib) {
		delete loaded_lib;
	}

	blog(LOG_INFO, "[obs-ndi] obs_module_unload: goodbye !");

	blog(LOG_INFO, "[obs-ndi] -obs_module_unload()");
}

const NDIlib_v4 *load_ndilib()
{
	QStringList locations;
	QString path = QString(qgetenv(NDILIB_REDIST_FOLDER));
	if (!path.isEmpty()) {
		locations << path;
	}
#if defined(__linux__) || defined(__APPLE__)
	locations << "/usr/lib";
	locations << "/usr/local/lib";
#endif
	for (QString location : locations) {
		path = QDir::cleanPath(
			QDir(location).absoluteFilePath(NDILIB_LIBRARY_NAME));
		blog(LOG_INFO, "[obs-ndi] load_ndilib: Trying '%s'",
		     path.toUtf8().constData());
		QFileInfo libPath(path);
		if (libPath.exists() && libPath.isFile()) {
			path = libPath.absoluteFilePath();
			blog(LOG_INFO,
			     "[obs-ndi] load_ndilib: Found NDI library at '%s'",
			     path.toUtf8().constData());
			loaded_lib = new QLibrary(path, nullptr);
			if (loaded_lib->load()) {
				blog(LOG_INFO,
				     "[obs-ndi] load_ndilib: NDI runtime loaded successfully");
				NDIlib_v5_load_ lib_load =
					(NDIlib_v5_load_)loaded_lib->resolve(
						"NDIlib_v5_load");
				if (lib_load != nullptr) {
					blog(LOG_INFO,
					     "[obs-ndi] load_ndilib: NDIlib_v5_load found");
					return lib_load();
				} else {
					blog(LOG_ERROR,
					     "[obs-ndi] load_ndilib: ERROR: NDIlib_v5_load not found in loaded library");
				}
			} else {
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		}
	}
	blog(LOG_ERROR,
	     "[obs-ndi] load_ndilib: ERROR: Can't find the NDI library");
	return nullptr;
}
