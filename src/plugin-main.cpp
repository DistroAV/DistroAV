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

#include "plugin-main.h"
#include "forms/obsndi-update.h"
#include "forms/output-settings.h"
#include "main-output.h"
#include "preview-output.h"

#include <obs-frontend-api.h>

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

const char *obs_module_name()
{
	return PLUGIN_NAME;
}

const char *obs_module_description()
{
	return Str("NDIPlugin.Description");
}

const NDIlib_v5 *load_ndilib();
QLibrary *loaded_lib;
typedef const NDIlib_v5 *(*NDIlib_v5_load_)(void);
const NDIlib_v5 *ndiLib = nullptr;
NDIlib_find_instance_t ndi_finder = nullptr;

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

QPointer<OutputSettings> output_settings = nullptr;

//
//
//

/**
 * If PLUGIN_WEB_HOST == "127.0.0.1" then `rehostUrl` will rewrite the urls to point to a local firebase emulator/server.
 * If PLUGIN_WEB_HOST != "127.0.0.1" then `rehostUrl` will return the same url that was passed to it.
 * 
 * @param url The url to rehost
 * @return if PLUGIN_WEB_HOST == "127.0.0.1" then "https://127.0.0.1:5002..." else url
 */
QString rehostUrl(const char *url)
{
	auto result = QString::fromUtf8(url);
#ifdef USE_LOCALHOST
	result.replace("https://distroav.org",
		       QString("http://%1:5002").arg(PLUGIN_WEB_HOST));
#endif
	return result;
}

/**
 * If PLUGIN_WEB_HOST == "127.0.0.1" then `makeLink` will call `rehostUrl` to rewrite any url.
 * If PLUGIN_WEB_HOST != "127.0.0.1" then `makeLink` will return the same url that was passed to it.
 * 
 * @param url The url to link to
 * @param text The text to display for the link; if null, the url will be used
 * @return if PLUGIN_WEB_HOST == "127.0.0.1" then "<a href=\"127.0.0.1:5001...\">text|url</a>" else "<a href=\"url\">text|url</a>"
 */
QString makeLink(const char *url, const char *text)
{
	return QString("<a href=\"%1\">%2</a>")
		.arg(rehostUrl(url), QString::fromUtf8(text ? text : url));
}

/**
 * References:
 * * QMessageBox::showNewMessageBox
 *   https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/dialogs/qmessagebox.cpp
 */
void showUnloadingMessageBoxDelayed(const QString &title,
				    const QString &message)
{
	const QString newTitle =
		QString("%1 : %2").arg(PLUGIN_DISPLAY_NAME).arg(title);
	const QString newMessage =
		QString("%1<br><br>%2")
			.arg(message)
			.arg(QTStr("NDIPlugin.PluginCannotContinueAndWillBeUnloaded")
				     .arg(PLUGIN_DISPLAY_NAME,
					  rehostUrl(
						  PLUGIN_REDIRECT_REPORT_BUG_URL),
					  rehostUrl(
						  PLUGIN_REDIRECT_UNINSTALL_URL)));
	QTimer::singleShot(2000, [newTitle, newMessage] {
		auto dlg = new QMessageBox(QMessageBox::Critical, newTitle,
					   newMessage, QMessageBox::Ok,
					   nullptr);
#if defined(__APPLE__)
		// https://stackoverflow.com/a/22187538/25683720
		dlg->QDialog::setWindowTitle(newTitle);
#endif
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		dlg->setWindowFlags(dlg->windowFlags() |
				    Qt::WindowStaysOnTopHint);
		dlg->setWindowModality(Qt::NonModal);
		dlg->show();
	});
}

/**
 * Certain distros of Linux come with a Qt runtime that is too old for this plugin.
 * Proof: https://github.com/obs-ndi/obs-ndi/issues/897#issuecomment-1806623157
 * 
 * This function checks if the Qt runtime version is at least the minimum required version.
 * @return true if the Qt runtime version is at least the minimum required version, otherwise false
 */
bool checkMinimumQtVersion()
{
#if 0
	// for testing purposes only
	auto versionQt = QVersionNumber::fromString("5.0.0");
#else
	auto versionQt = QVersionNumber::fromString(qVersion());
#endif
	blog(LOG_INFO,
	     "[obs-ndi] checkMinimumQtVersion: Qt Version: Runtime:qVersion()=%s, Compiled:QT_VERSION_STR=%s)",
	     versionQt.toString().toUtf8().constData(), QT_VERSION_STR);
	auto versionQtMinimumRequired =
		QVersionNumber::fromString(PLUGIN_MIN_QT_VERSION);
	if (QVersionNumber::compare(versionQt, versionQtMinimumRequired) < 0) {
		auto title = Str("NDIPlugin.QtVersionError.Title");
		auto message = QTStr("NDIPlugin.QtVersionError.Message")
				       .arg(PLUGIN_DISPLAY_NAME, PLUGIN_VERSION,
					    versionQtMinimumRequired.toString(),
					    versionQt.toString());
		blog(LOG_ERROR, "[obs-ndi] checkMinimumQtVersion: %s",
		     message.toUtf8().constData());
		showUnloadingMessageBoxDelayed(title, message);
		return false;
	}
	return true;
}

/**
 * This may seem a little redundant to checkMinimumQtVersion, but it
 * might still be theoretically possible for someone to successfully
 * launch an old version OBS on a system that has a new version of Qt.
 * 
 * This function checks if the OBS version is at least the minimum required version.
 * @return true if the OBS version is at least the minimum required version, otherwise false
 */
bool checkMinimumObsVersion()
{
#if 0
	// for testing purposes only
	auto versionObs = QVersionNumber::fromString("29.0.0");
#else
	auto versionObs = QVersionNumber::fromString(obs_get_version_string());
#endif
	blog(LOG_INFO,
	     "[obs-ndi] checkMinimumObsVersion: OBS Version: obs_get_version_string()=%s",
	     versionObs.toString().toUtf8().constData());
	auto versionObsMinimumRequired =
		QVersionNumber::fromString(PLUGIN_MIN_OBS_VERSION);
	if (QVersionNumber::compare(versionObs, versionObsMinimumRequired) <
	    0) {
		auto title = Str("NDIPlugin.ObsVersionError.Title");
		auto message =
			QTStr("NDIPlugin.ObsVersionError.Message")
				.arg(PLUGIN_DISPLAY_NAME, PLUGIN_VERSION,
				     versionObsMinimumRequired.toString(),
				     versionObs.toString());
		blog(LOG_ERROR, "[obs-ndi] checkMinimumObsVersion: %s",
		     message.toUtf8().constData());
		showUnloadingMessageBoxDelayed(title, message);
		return false;
	}
	return true;
}

bool checkLoadingNdiLibrary()
{
#if 0
	// for testing purposes only
	ndiLib = nullptr;
#else
	ndiLib = load_ndilib();
#endif
	if (!ndiLib) {
		auto title = Str("NDIPlugin.NdiLibError.Title");
		auto message =
			QTStr("NDIPlugin.NdiLibError.Message")
				.arg(PLUGIN_DISPLAY_NAME,
#ifdef NDI_OFFICIAL_REDIST_URL
				     makeLink(NDI_OFFICIAL_REDIST_URL),
#else
				     makeLink(PLUGIN_REDIRECT_NDI_REDIST_URL),
#endif
				     makeLink(
					     PLUGIN_REDIRECT_TROUBLESHOOTING_URL));
		blog(LOG_ERROR,
		     "checkLoadingNdiLibrary: ERROR - load_ndilib() failed; message=%s",
		     message.toUtf8().constData());
		showUnloadingMessageBoxDelayed(title, message);
		return false;
	}

#if 0
	// for testing purposes only
	auto initialized = false;
#else
	auto initialized = ndiLib->initialize();
#endif
	if (!initialized) {
		auto title = Str("NDIPlugin.NdiInitializeError.Title");
		auto message =
			QTStr("NDIPlugin.NdiInitializeError.Message")
				.arg(PLUGIN_DISPLAY_NAME,
				     makeLink(
					     PLUGIN_REDIRECT_NDI_SDK_CPU_REQUIREMENTS_URL,
					     NDI_OFFICIAL_CPU_REQUIREMENTS_URL));
		blog(LOG_ERROR,
		     "checkLoadingNdiLibrary: ERROR - ndiLib->initialize() failed; message=%s",
		     message.toUtf8().constData());
		showUnloadingMessageBoxDelayed(title, message);
		return false;
	}

	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
#if 0
	// for testing purposes only
	ndi_finder = nullptr;
#else
	ndi_finder = ndiLib->find_create_v2(&find_desc);
#endif
	if (!ndi_finder) {
		auto title = Str("NDIPlugin.NdiFinderError.Title");
		auto message = QTStr("NDIPlugin.NdiFinderError.Message")
				       .arg(PLUGIN_DISPLAY_NAME);
		blog(LOG_ERROR,
		     "checkLoadingNdiLibrary: ERROR - ndiLib->find_create_v2() failed; message=%s",
		     message.toUtf8().constData());
		showUnloadingMessageBoxDelayed(title, message);
		return false;
	}

	blog(LOG_INFO,
	     "[obs-ndi] checkLoadingNdiLibrary: NDI '%s' loaded and initialized successfully",
	     ndiLib->version());

	return true;
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs-ndi] +obs_module_load(): hello! PLUGIN_VERSION=%s",
	     PLUGIN_VERSION);
#if 0
	// for testing purposes only
	bool success = false;
#else
	Config::Current()->Load();
	// clang-format off
	bool success = checkMinimumQtVersion()
		       && checkMinimumObsVersion()
		       && checkLoadingNdiLibrary();
	// clang-format on
#endif
	if (!success) {
		obs_module_unload();
	}
	blog(LOG_INFO, "[obs-ndi] -obs_module_load(): success=%s",
	     success ? "true" : "false");
	return success;
}

void obs_module_post_load(void)
{
	blog(LOG_INFO, "[obs-ndi] +obs_module_post_load()");

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

	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (main_window) {
		// UI setup
		auto menu_action = static_cast<QAction *>(
			obs_frontend_add_tools_menu_qaction(
				Str("NDIPlugin.Menu.OutputSettings")));

		obs_frontend_push_ui_translation(obs_module_get_string);
		output_settings = new OutputSettings(main_window);
		obs_frontend_pop_ui_translation();

		menu_action->connect(menu_action, &QAction::triggered,
				     [] { output_settings->ToggleShowHide(); });
	} else {
		blog(LOG_ERROR,
		     "[obs-ndi] obs_module_post_load: ERROR: main_window is null; this should never happen.");
	}

	auto conf = Config::Current();

	preview_output_init(conf->PreviewOutputName.toUtf8().constData(),
			    conf->PreviewOutputGroups.toUtf8().constData());

	if (conf->OutputEnabled) {
		main_output_start(conf->OutputName.toUtf8().constData(),
				  conf->OutputGroups.toUtf8().constData());
	}
	if (conf->PreviewOutputEnabled) {
		preview_output_start(
			conf->PreviewOutputName.toUtf8().constData(),
			conf->PreviewOutputGroups.toUtf8().constData());
	}

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

	Config::Destroy();

	blog(LOG_INFO, "[obs-ndi] -obs_module_unload(): goodbye!");
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
	for (auto location : locations) {
		path = QDir::cleanPath(
			QDir(location).absoluteFilePath(NDILIB_LIBRARY_NAME));
		blog(LOG_INFO, "[obs-ndi] load_ndilib: Trying '%s'",
		     path.toUtf8().constData());
		QFileInfo libPath(path);
		if (libPath.exists() && libPath.isFile()) {
			path = libPath.absoluteFilePath();
			blog(LOG_INFO,
			     "[obs-ndi] load_ndilib: Found '%s'; attempting to load NDI library...",
			     path.toUtf8().constData());
			loaded_lib = new QLibrary(path, nullptr);
			if (loaded_lib->load()) {
				blog(LOG_INFO,
				     "[obs-ndi] load_ndilib: NDI library loaded successfully");
				NDIlib_v5_load_ lib_load =
					reinterpret_cast<NDIlib_v5_load_>(
						loaded_lib->resolve(
							"NDIlib_v5_load"));
				if (lib_load != nullptr) {
					blog(LOG_INFO,
					     "[obs-ndi] load_ndilib: NDIlib_v5_load found");
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
