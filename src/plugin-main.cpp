/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

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

#include "plugin-main.h"

#include "forms/output-settings.h"
#include "forms/update.h"
#include "main-output.h"
#include "preview-output.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
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
 * @return if command-line `--distroav-update-local[=port]` is defined then "https://127.0.0.1:[port]...", otherwise url
 */
QString rehostUrl(const char *url)
{
	auto result = QString::fromUtf8(url);
	if (Config::UpdateLocalPort > 0) {
		result.replace("https://distroav.org",
			       QString("http://%1:%2")
				       .arg(PLUGIN_WEB_HOST_LOCAL_EMULATOR)
				       .arg(Config::UpdateLocalPort));
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
 * Similar to `QMessageBox::critical` but with the following changes:
 *  1. The title is prefixed with the plugin name
 *  2. MacOS: Shows text in the title bar
 *  3. MacOS: The message is not bold by default
 *  4. A common footer is appended to the message
 *  5. The message box is shown after a delay (default 2000ms)
 *  6. Shows the dialog as WindowStaysOnTopHint and NonModal
 *  7. Deletes the dialog when closed
 * 
 * References:
 * * QMessageBox::showNewMessageBox
 *   https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/dialogs/qmessagebox.cpp
 *   * `showNewMessageBox` internals...
 *     https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/dialogs/qmessagebox.cpp#n1754
 *   * MacOS defaults text to bold
 *     https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/dialogs/qmessagebox.cpp#n284
 *     ```
 * void QMessageBoxPrivate::init(const QString &title, const QString &text)
 * ... 
 * #ifdef Q_OS_MAC
 *     QFont f = q->font();
 *     f.setBold(true);
 *     label->setFont(f);
 * #endif
 * ...
 *     ```
 * * MacOS guidelines say that dialog title bars have no text.
 *   https://stackoverflow.com/a/22187538/25683720
 * 
 * @param title The title of the message box
 * @param message The message to display in the message box
 * @param milliseconds The delay in milliseconds before the message box is shown
 */
void showCriticalUnloadingMessageBoxDelayed(const QString &title,
					    const QString &message,
					    int milliseconds = 2000)
{
	auto newTitle = QString("%1 : %2").arg(PLUGIN_DISPLAY_NAME, title);

	auto newMessage = message;
	newMessage.remove(QRegularExpression("(\r?\n?<br>\r?\n?)+$"));
	auto newMessageFormat =
		QRegularExpression("(</ol>|</ul>)$").match(newMessage).hasMatch()
			? "%1%2"
			: "%1<br><br>%2";
	newMessage =
		QString(newMessageFormat)
			.arg(newMessage,
			     QTStr("NDIPlugin.PluginCannotContinueAndWillBeUnloaded")
				     .arg(PLUGIN_DISPLAY_NAME,
					  rehostUrl(
						  PLUGIN_REDIRECT_REPORT_BUG_URL),
					  rehostUrl(
						  PLUGIN_REDIRECT_UNINSTALL_URL)));
	QTimer::singleShot(milliseconds, [newTitle, newMessage] {
		auto dlg = new QMessageBox(QMessageBox::Critical, newTitle,
					   newMessage, QMessageBox::Ok);
#if defined(Q_OS_MACOS)
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

struct find_module_data {
	const char *target_name;
	bool found;
};

bool is_module_found(const char *module_name)
{
	struct find_module_data data = {0};
	data.target_name = module_name;
	obs_find_modules2(
		[](void *param, const struct obs_module_info2 *module_info) {
			struct find_module_data *data_ =
				(struct find_module_data *)param;
			if (strcmp(data_->target_name, module_info->name) ==
			    0) {
				obs_log(LOG_INFO,
					"is_module_found: Found module_info->name == `%s`",
					module_info->name);
#if 0
				obs_log(LOG_INFO,
				     "is_module_found: module_info->bin_path=`%s`",
				     module_info->bin_path);
				obs_log(LOG_INFO,
				     "is_module_found: module_info->data_path=`%s`",
				     module_info->data_path);
#endif
				data_->found = true;
			}
		},
		&data);
	return data.found;
}

bool is_obsndi_installed()
{
	auto force = Config::DetectObsNdiForce;
	if (force != 0) {
		return force > 0;
	}
	return is_module_found("obs-ndi");
}

//
//
//

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "obs_module_load: you can haz %s (Version %s)",
		PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
	obs_log(LOG_INFO,
		"obs_module_load: Qt Version: %s (runtime), %s (compiled)",
		qVersion(), QT_VERSION_STR);

	auto config = Config::Current();

	if (is_obsndi_installed()) {
		obs_log(LOG_INFO,
			"obs_module_load: OBS-NDI is detected and needs to be uninstalled before %s will load.",
			PLUGIN_DISPLAY_NAME);
		showCriticalUnloadingMessageBoxDelayed(
			QTStr("NDIPlugin.ErrorObsNdiDetected.Title"),
			QTStr("NDIPlugin.ErrorObsNdiDetected.Message")
				.arg(rehostUrl(
					PLUGIN_REDIRECT_UNINSTALL_OBSNDI_URL)));
		return false;
	}

	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

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
		obs_log(LOG_ERROR,
			"obs_module_load: ERROR - load_ndilib() failed; message=%s",
			QT_TO_UTF8(message));
		showCriticalUnloadingMessageBoxDelayed(title, message);
		return false;
	}

#if 0
	// for testing purposes only
	auto initialized = false;
#else
	auto initialized = ndiLib->initialize();
#endif
	if (!initialized) {
		obs_log(LOG_ERROR,
			"obs_module_load: ndiLib->initialize() failed; CPU unsupported by NDI library. Module won't load.");
		return false;
	}

	obs_log(LOG_INFO,
		"obs_module_load: NDI library initialized successfully ('%s')",
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
		auto menu_action = static_cast<QAction *>(
			obs_frontend_add_tools_menu_qaction(obs_module_text(
				"NDIPlugin.Menu.OutputSettings")));

		obs_frontend_push_ui_translation(obs_module_get_string);
		output_settings = new OutputSettings(main_window);
		obs_frontend_pop_ui_translation();

		auto menu_cb = [] {
			output_settings->toggleShowHide();
		};
		menu_action->connect(menu_action, &QAction::triggered, menu_cb);

		obs_frontend_add_event_callback(
			[](enum obs_frontend_event event, void *) {
				if (event ==
				    OBS_FRONTEND_EVENT_FINISHED_LOADING) {
					main_output_init();
					preview_output_init();
				} else if (event == OBS_FRONTEND_EVENT_EXIT) {
					// Unknown why putting this in obs_module_unload causes a crash when closing OBS
					main_output_deinit();
					preview_output_deinit();
				}
			},
			nullptr);
	}

	return true;
}

void obs_module_post_load(void)
{
	obs_log(LOG_INFO, "+obs_module_post_load()");

	updateCheckStart();

	obs_log(LOG_INFO, "-obs_module_post_load()");
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "+obs_module_unload()");

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

	obs_log(LOG_INFO, "-obs_module_unload(): goodbye!");
}

const NDIlib_v5 *load_ndilib()
{
	auto locations = QStringList();
	auto temp_path = QString(qgetenv(NDILIB_REDIST_FOLDER));
	if (!temp_path.isEmpty()) {
		locations << temp_path;
	}
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	// Linux, MacOS
	// https://github.com/DistroAV/DistroAV/blob/master/lib/ndi/NDI%20SDK%20Documentation.pdf
	// "6.1 LOCATING THE LIBRARY
	// ... the redistributable on MacOS is installed within `/usr/local/lib` ..."
	locations << "/usr/lib";
	locations << "/usr/local/lib";
#endif
	auto lib_path = QString();
#if defined(Q_OS_LINUX)
	// Linux
	auto regex = QRegularExpression("libndi\\.so\\.(\\d+)");
	int max_version = 0;
#endif
	for (const auto &location : locations) {
		auto dir = QDir(location);
#if defined(Q_OS_LINUX)
		// Linux
		auto filters = QStringList("libndi.so.*");
		dir.setNameFilters(filters);
		auto file_names = dir.entryList(QDir::Files);
		for (const auto &file_name : file_names) {
			auto match = regex.match(file_name);
			if (match.hasMatch()) {
				int version = match.captured(1).toInt();
				if (version > max_version) {
					max_version = version;
					lib_path =
						dir.absoluteFilePath(file_name);
				}
			}
		}
#else
		// MacOS, Windows
		temp_path = QDir::cleanPath(
			dir.absoluteFilePath(NDILIB_LIBRARY_NAME));
		obs_log(LOG_INFO, "load_ndilib: Trying '%s'",
			QT_TO_UTF8(QDir::toNativeSeparators(temp_path)));
		auto file_info = QFileInfo(temp_path);
		if (file_info.exists() && file_info.isFile()) {
			lib_path = temp_path;
			break;
		}
#endif
	}
	if (!lib_path.isEmpty()) {
		obs_log(LOG_INFO,
			"load_ndilib: Found '%s'; attempting to load NDI library...",
			QT_TO_UTF8(QDir::toNativeSeparators(lib_path)));
		loaded_lib = new QLibrary(lib_path, nullptr);
		if (loaded_lib->load()) {
			obs_log(LOG_INFO,
				"load_ndilib: NDI library loaded successfully");
			NDIlib_v5_load_ lib_load =
				reinterpret_cast<NDIlib_v5_load_>(
					loaded_lib->resolve("NDIlib_v5_load"));
			if (lib_load != nullptr) {
				obs_log(LOG_INFO,
					"load_ndilib: NDIlib_v5_load found");
				return lib_load();
			} else {
				obs_log(LOG_ERROR,
					"load_ndilib: ERROR: NDIlib_v5_load not found in loaded library");
			}
		} else {
			obs_log(LOG_ERROR,
				"load_ndilib: ERROR: QLibrary returned the following error: '%s'",
				QT_TO_UTF8(loaded_lib->errorString()));
			delete loaded_lib;
			loaded_lib = nullptr;
		}
	}

	obs_log(LOG_ERROR, "load_ndilib: ERROR: Can't find the NDI library");
	return nullptr;
}
