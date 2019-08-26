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

#ifdef _WIN32
#include <Windows.h>
#endif

#include <sys/stat.h>

#include <obs-module.h>
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

#include "obs-ndi.h"
#include "main-output.h"
#include "preview-output.h"
#include "Config.h"
#include "src/forms/ndi-settings.h"
#include "ndi-source-finder.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Stephane Lepin (Palakis)")
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

const NDIlib_v3* ndiLib = nullptr;

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

const NDIlib_v3* load_ndilib();

typedef const NDIlib_v3* (*NDIlib_v3_load_)(void);
QLibrary* loaded_lib = nullptr;

NDISettings* ndi_settings;

bool obs_module_load(void)
{
	ndiblog(LOG_INFO, "hello ! (version %s)", OBS_NDI_VERSION);

	QMainWindow* main_window = (QMainWindow*)obs_frontend_get_main_window();

	ndiLib = load_ndilib();
	if (!ndiLib) {
		const char* msg_string_name = "";
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

		QMessageBox::critical(main_window,
			obs_module_text("NDIPlugin.LibError.Title"),
			obs_module_text(msg_string_name),
			QMessageBox::Ok, QMessageBox::NoButton);
		return false;
	}

	if (!ndiLib->NDIlib_initialize()) {
		ndiblog(LOG_ERROR, "CPU unsupported by NDI library. Module won't load.");
		return false;
	}

	ndiblog(LOG_INFO, "NDI library initialized successfully (%s)", ndiLib->NDIlib_version());

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
		Config* conf = Config::Current();
		conf->Load();

		main_output_init(conf->OutputName.toUtf8().constData());
		preview_output_init(conf->PreviewOutputName.toUtf8().constData());

		// Ui setup
		QAction* menu_action = (QAction*)obs_frontend_add_tools_menu_qaction(
			obs_module_text("NDIPlugin.Menu.NDISettings"));

		obs_frontend_push_ui_translation(obs_module_get_string);
		ndi_settings = new NDISettings(main_window);
		obs_frontend_pop_ui_translation();

		auto menu_cb = [] {
			ndi_settings->ToggleShowHide();
		};
		menu_action->connect(menu_action, &QAction::triggered, menu_cb);

		obs_frontend_add_event_callback([](enum obs_frontend_event event, void *private_data) {
			Config* conf = (Config*)private_data;

			if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
				if (conf->OutputEnabled) {
					main_output_start(conf->OutputName.toUtf8().constData());
				}
				if (conf->PreviewOutputEnabled) {
					preview_output_start(conf->PreviewOutputName.toUtf8().constData());
				}
    		update_ndi_finder(conf->FinderExtraIps.toUtf8().constData());
			} else if (event == OBS_FRONTEND_EVENT_EXIT) {
				preview_output_stop();
				main_output_stop();

				preview_output_deinit();
				main_output_deinit();
			}
		}, (void*)conf);
	}

	return true;
}

void obs_module_unload()
{
	ndiblog(LOG_INFO, "goodbye !");

	if (ndiLib) {
        destroy_ndi_finder();
		ndiLib->NDIlib_destroy();
	}

	if (loaded_lib) {
		delete loaded_lib;
	}
}

const char* obs_module_name()
{
	return "obs-ndi";
}

const char* obs_module_description()
{
	return "NDI input/output integration for OBS Studio";
}

const NDIlib_v3* load_ndilib()
{
	QStringList locations;
	locations << QString(qgetenv(NDILIB_REDIST_FOLDER));
#if defined(__linux__) || defined(__APPLE__)
	locations << "/usr/lib";
	locations << "/usr/local/lib";
#endif

	for (QString path : locations) {
		ndiblog(LOG_INFO, "Trying '%s'", path.toUtf8().constData());
		QFileInfo libPath(QDir(path).absoluteFilePath(NDILIB_LIBRARY_NAME));

		if (libPath.exists() && libPath.isFile()) {
			QString libFilePath = libPath.absoluteFilePath();
			ndiblog(LOG_INFO, "Found NDI library at '%s'",
				libFilePath.toUtf8().constData());

			loaded_lib = new QLibrary(libFilePath, nullptr);
			if (loaded_lib->load()) {
				ndiblog(LOG_INFO, "NDI runtime loaded successfully");

				NDIlib_v3_load_ lib_load =
					(NDIlib_v3_load_)loaded_lib->resolve("NDIlib_v3_load");

				if (lib_load != nullptr) {
					return lib_load();
				}
				else {
					ndiblog(LOG_INFO, "ERROR: NDIlib_v3_load not found in loaded library");
				}
			}
			else {
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		}
	}

	ndiblog(LOG_ERROR, "Can't find the NDI library");
	return nullptr;
}
