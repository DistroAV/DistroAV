/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

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

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "plugin-main.h"
#include "main-output.h"
#include "preview-output.h"
#include "Config.h"
#include "forms/output-settings.h"
#include "forms/obsndi-update.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

const char *obs_module_name()
{
	return "obs-ndi";
}

const char *obs_module_description()
{
	return Str("NDIPlugin.Description");
}

// Copied from OBS UI/obs-app.hpp
// Changed to use obs_module_text instead of ((OBSApp*)App())->GetString
const char *Str(const char *lookup)
{
	return obs_module_text(lookup);
}

// Copied from OBS UI/obs-app.hpp
// No change
QString QTStr(const char *lookupVal)
{
	return QString::fromUtf8(Str(lookupVal));
}

const NDIlib_v5 *ndiLib = nullptr;

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

QPointer<OutputSettings> output_settings = nullptr;

bool obs_module_load(void)
{
	Config::Current()->Load();

	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: you can haz obs-ndi (Version %s)",
	     PLUGIN_VERSION);

	auto versionQt = QVersionNumber::fromString(qVersion());
	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: Qt Version: %s (runtime), %s (compiled)",
	     versionQt.toString().toUtf8().constData(), QT_VERSION_STR);
	auto versionObs = QVersionNumber::fromString(obs_get_version_string());
	blog(LOG_INFO, "obs_module_load: OBS Version: %s",
	     versionObs.toString().toUtf8().constData());

	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	//versionQt = QVersionNumber::fromString("5.0.0"); // for testing purposes only
	auto versionQtMinimumRequired =
		QVersionNumber::fromString(PLUGIN_MIN_QT_VERSION);
	if (QVersionNumber::compare(versionQt, versionQtMinimumRequired) < 0) {
		//
		// Certain distros of Linux come with a runtime version of Qt that is too old to run this plugin.
		// Proof: https://github.com/obs-ndi/obs-ndi/issues/897#issuecomment-1806623157
		//
		QString message =
			QTStr("NDIPlugin.QtVersionError.Message")
				.arg(PLUGIN_DISPLAY_NAME, PLUGIN_VERSION,
				     versionQtMinimumRequired.toString(),
				     versionQt.toString());

		blog(LOG_ERROR, "[obs-ndi] obs_module_load: %s",
		     message.toUtf8().constData());

		QMessageBox::critical(main_window,
				      Str("NDIPlugin.QtVersionError.Title"),
				      message, QMessageBox::Ok,
				      QMessageBox::NoButton);
		return false;
	}

	//versionObs = QVersionNumber::fromString("29.0.0"); // for testing purposes only
	auto versionObsMinimumRequired =
		QVersionNumber::fromString(PLUGIN_MIN_OBS_VERSION);
	if (QVersionNumber::compare(versionObs, versionObsMinimumRequired) <
	    0) {
		//
		// This may seem a little redundant to the Qt version check above, but
		// it might still be theoretically possible for someone to successfully
		// launch an old version OBS on a system that has a new version of Qt.
		//
		QString message =
			QTStr("NDIPlugin.ObsVersionError.Message")
				.arg(PLUGIN_DISPLAY_NAME, PLUGIN_VERSION,
				     versionObsMinimumRequired.toString(),
				     versionObs.toString());

		blog(LOG_ERROR, "[obs-ndi] obs_module_load: %s",
		     message.toUtf8().constData());

		QMessageBox::critical(main_window,
				      Str("NDIPlugin.ObsVersionError.Title"),
				      message, QMessageBox::Ok,
				      QMessageBox::NoButton);
		return false;
	}

	ndiLib = load_ndilib();
	if (!ndiLib) {
		blog(LOG_ERROR,
		     "[obs-ndi] obs_module_load: load_ndilib() failed; Module won't load.");

		QString message = Str("NDIPlugin.LibError.Message");
		message += QString("<br><a href='%1'>%1</a>")
				   .arg(NDILIB_REDIST_URL);
		blog(LOG_ERROR, "obs_module_load: load_ndilib() message=%s",
		     message.toUtf8().constData());

		QMessageBox::critical(main_window,
				      Str("NDIPlugin.LibError.Title"), message,
				      QMessageBox::Ok, QMessageBox::NoButton);
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
		// Ui setup
		auto menu_action = static_cast<QAction *>(
			obs_frontend_add_tools_menu_qaction(
				Str("NDIPlugin.Menu.OutputSettings")));

		obs_frontend_push_ui_translation(obs_module_get_string);
		output_settings = new OutputSettings(main_window);
		obs_frontend_pop_ui_translation();

		menu_action->connect(menu_action, &QAction::triggered,
				     [] { output_settings->ToggleShowHide(); });
	}

	return true;
}

void obs_module_post_load(void)
{
	blog(LOG_INFO, "[obs-ndi] +obs_module_post_load()");

	preview_output_init();

	main_output_start();
	preview_output_start();

	blog(LOG_INFO, "[obs-ndi] obs_module_post_load: updateCheckStart()...");
	updateCheckStart();

	blog(LOG_INFO, "[obs-ndi] -obs_module_post_load()");
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[obs-ndi] +obs_module_unload()");

	blog(LOG_INFO, "[obs-ndi] obs_module_unload: updateCheckStop()...");
	updateCheckStop();

	preview_output_stop();
	main_output_stop();

	preview_output_deinit();

	if (output_settings) {
		output_settings->deleteLater();
		output_settings = nullptr;
	}

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
		loaded_lib = nullptr;
	}

	blog(LOG_INFO, "[obs-ndi] obs_module_unload: goodbye !");

	blog(LOG_INFO, "[obs-ndi] -obs_module_unload()");
}

const NDIlib_v5 *load_ndilib()
{
	QStringList locations;
	auto path = QString(qgetenv(NDILIB_REDIST_FOLDER));
	if (!path.isEmpty()) {
		locations << path;
	}
#if defined(__linux__) || defined(__APPLE__)
	locations << "/usr/lib";
	locations << "/usr/local/lib";
#endif
	auto verbose_log = Config::VerboseLog();
	for (auto location : locations) {
		path = QDir::cleanPath(
			QDir(location).absoluteFilePath(NDILIB_LIBRARY_NAME));
		if (verbose_log) {
			blog(LOG_INFO, "[obs-ndi] load_ndilib: Trying '%s'",
			     path.toUtf8().constData());
		}
		QFileInfo libPath(path);
		if (libPath.exists() && libPath.isFile()) {
			path = libPath.absoluteFilePath();
			blog(LOG_INFO,
			     "[obs-ndi] load_ndilib: Using NDI library at '%s'",
			     path.toUtf8().constData());
			loaded_lib = new QLibrary(path, nullptr);
			if (loaded_lib->load()) {
				if (verbose_log) {
					blog(LOG_INFO,
					     "[obs-ndi] load_ndilib: NDI runtime loaded successfully");
				}
				NDIlib_v5_load_ lib_load =
					(NDIlib_v5_load_)loaded_lib->resolve(
						"NDIlib_v5_load");
				if (lib_load != nullptr) {
					if (verbose_log) {
						blog(LOG_INFO,
						     "[obs-ndi] load_ndilib: NDIlib_v5_load found");
					}
					return lib_load();
				} else {
					blog(LOG_ERROR,
					     "[obs-ndi] load_ndilib: ERROR: NDIlib_v5_load not found in loaded library");
				}
			} else {
				blog(LOG_ERROR,
				     "[obs-ndi] load_ndilib: ERROR: QLibrary returned the following error: '%s'",
				     loaded_lib->errorString()
					     .toUtf8()
					     .constData());
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		}
	}
	blog(LOG_ERROR,
	     "[obs-ndi] load_ndilib: ERROR: Can't find the NDI library");
	return nullptr;
}
