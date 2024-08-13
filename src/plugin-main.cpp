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
OutputSettings *output_settings = nullptr;

//
//
//

/**
 * @param url The url to rehost
 * @return if macro `USE_LOCALHOST` is defined then "https://127.0.0.1:5002...", otherwise url
 */
QString rehostUrl(const char *url)
{
	auto result = QString::fromUtf8(url);
	if (Config::UpdateHost() == UpdateHostEnum::LocalEmulator) {
		result.replace("https://distroav.org",
			       QString("http://%1:%2")
				       .arg(PLUGIN_WEB_HOST_LOCAL_EMULATOR)
				       .arg(Config::UpdateLocalPort()));
	}
	return result;
}

/**
 * @param url The url to link to
 * @param text The text to display for the link; if null, the url will be used
 * @return if macro `USE_LOCALHOST` is defined then "<a href=\"127.0.0.1:5001...\">text|url</a>", otherwise "<a href=\"url\">text|url</a>"
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
	const QString newTitle = QString("%1 : %2").arg(PLUGIN_NAME).arg(title);
	const QString newMessage =
		QString("%1<br><br>%2")
			.arg(message)
			.arg(QTStr("NDIPlugin.PluginCannotContinueAndWillBeUnloaded")
				     .arg(PLUGIN_NAME,
					  rehostUrl(
						  PLUGIN_REDIRECT_REPORT_BUG_URL),
					  rehostUrl(
						  PLUGIN_REDIRECT_UNINSTALL_URL)));
	QTimer::singleShot(2000, [newTitle, newMessage] {
		auto dlg = new QMessageBox(QMessageBox::Critical, newTitle,
					   newMessage, QMessageBox::Ok,
					   nullptr);
#if defined(__APPLE__)
		// Make title bar show text: https://stackoverflow.com/a/22187538/25683720
		dlg->QDialog::setWindowTitle(newTitle);
#endif
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		dlg->setWindowFlags(dlg->windowFlags() |
				    Qt::WindowStaysOnTopHint);
		dlg->setWindowModality(Qt::NonModal);
		dlg->show();
	});
}

//
//
//

bool obs_module_load(void)
{
	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: you can haz obs-ndi (Version %s)",
	     PLUGIN_VERSION);
	blog(LOG_INFO,
	     "[obs-ndi] obs_module_load: Qt Version: %s (runtime), %s (compiled)",
	     qVersion(), QT_VERSION_STR);

	Config::Current()->Load();

	QMainWindow *main_window =
		(QMainWindow *)obs_frontend_get_main_window();

#if 0
	// For testing purposes only
	ndiLib = nullptr;
#else
	ndiLib = load_ndilib();
#endif
	if (!ndiLib) {
		auto title = Str("NDIPlugin.LibError.Title");
		auto message = QTStr("NDIPlugin.LibError.Message") + "<br>";
#ifdef NDI_OFFICIAL_REDIST_URL
		message += makeLink(NDI_OFFICIAL_REDIST_URL);
#else
		message += makeLink(PLUGIN_REDIRECT_NDI_REDIST_URL);
#endif
		blog(LOG_ERROR,
		     "[obs-ndi] obs_module_load: ERROR - load_ndilib() failed; message=%s",
		     QT_TO_UTF8(message));
		showUnloadingMessageBoxDelayed(title, message);
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
		auto conf = Config::Current();

		preview_output_init(QT_TO_UTF8(conf->PreviewOutputName),
				    QT_TO_UTF8(conf->PreviewOutputGroups));
		main_output_init(QT_TO_UTF8(conf->OutputName),
				 QT_TO_UTF8(conf->OutputGroups));

		// Ui setup
		QAction *menu_action =
			(QAction *)obs_frontend_add_tools_menu_qaction(
				obs_module_text(
					"NDIPlugin.Menu.OutputSettings"));

		obs_frontend_push_ui_translation(obs_module_get_string);
		output_settings = new OutputSettings(main_window);
		obs_frontend_pop_ui_translation();

		auto menu_cb = [] {
			output_settings->toggleShowHide();
		};
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
							QT_TO_UTF8(
								conf->OutputName),
							QT_TO_UTF8(
								conf->OutputGroups));
					}
					if (conf->PreviewOutputEnabled) {
						preview_output_start(
							QT_TO_UTF8(
								conf->PreviewOutputName),
							QT_TO_UTF8(
								conf->PreviewOutputGroups));
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
	blog(LOG_INFO, "[obs-ndi] +obs_module_post_load()");

	updateCheckStart();

	blog(LOG_INFO, "[obs-ndi] -obs_module_post_load()");
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[obs-ndi] +obs_module_unload()");

	updateCheckStop();

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
		     QT_TO_UTF8(path));
		QFileInfo libPath(path);
		if (libPath.exists() && libPath.isFile()) {
			path = libPath.absoluteFilePath();
			blog(LOG_INFO,
			     "[obs-ndi] load_ndilib: Found '%s'; attempting to load NDI library...",
			     QT_TO_UTF8(path));
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
				     QT_TO_UTF8(loaded_lib->errorString()));
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		}
	}
	blog(LOG_ERROR,
	     "[obs-ndi] load_ndilib: ERROR: Can't find the NDI library");
	return nullptr;
}
