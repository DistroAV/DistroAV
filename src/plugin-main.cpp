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
#include "obs-module.h"
#include "plugin-support.h"

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

#include <cstring>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

const char *obs_module_name()
{
	return PLUGIN_DISPLAY_NAME;
}

const char *obs_module_description()
{
	return Str("NDIPlugin.Description");
}

const NDIlib_v6 *ndiLib = nullptr;
static bool plugin_features_registered = false;

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

const NDIlib_v6 *load_ndilib();

typedef const NDIlib_v6 *(*NDIlib_v6_load_)(void);
QLibrary *loaded_lib = nullptr;

OutputSettings *output_settings = nullptr;

static constexpr const char *NDI_FILTER_ID = "ndi_filter";
static constexpr const char *NDI_AUDIOFILTER_ID = "ndi_audiofilter";

//
//
//

static bool is_ndi_filter_source(obs_source_t *source)
{
	if (!source) {
		return false;
	}

	const char *source_id = obs_source_get_id(source);
	return source_id && (strcmp(source_id, NDI_FILTER_ID) == 0 || strcmp(source_id, NDI_AUDIOFILTER_ID) == 0);
}

static void ensure_ndi_filters_started()
{
	size_t refreshed_filter_count = 0;

	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			if (obs_source_removed(source)) {
				return true;
			}

			auto count_ptr = static_cast<size_t *>(data);

			obs_source_enum_filters(
				source,
				[](obs_source_t *, obs_source_t *filter, void *param) {
					if (obs_source_removed(filter) || !is_ndi_filter_source(filter)) {
						return;
					}

					if (!obs_filter_get_parent(filter)) {
						// Parent can still be unavailable in this post-load pass; skip and keep diagnostics.
						obs_log(LOG_DEBUG,
							"OBS frontend finished loading: skipping NDI filter '%s' (parent not ready).",
							obs_source_get_name(filter));
						return;
					}

					auto settings = obs_source_get_settings(filter);
					if (!settings) {
						obs_log(LOG_DEBUG,
							"OBS frontend finished loading: skipping NDI filter '%s' (missing settings).",
							obs_source_get_name(filter));
						return;
					}

					obs_source_update(filter, settings);
					obs_data_release(settings);

					auto filter_count_ptr = static_cast<size_t *>(param);
					(*filter_count_ptr)++;
				},
				count_ptr);

			return true;
		},
		&refreshed_filter_count);

	obs_log(LOG_DEBUG, "OBS frontend finished loading: refreshed %zu NDI filter sender(s).",
		refreshed_filter_count);
}

/**
 * @param url The url to rehost
 * @return if command-line `--distroav-update-local[=port]` is defined then "https://127.0.0.1:[port]...", otherwise url
 */
QString rehostUrl(const char *url)
{
	auto result = QString::fromUtf8(url);
	if (Config::UpdateLocalPort > 0) {
		result.replace(
			"https://distroav.org",
			QString("http://%1:%2").arg(PLUGIN_WEB_HOST_LOCAL_EMULATOR).arg(Config::UpdateLocalPort));
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
	return QString("<a href=\"%1\">%2</a>").arg(rehostUrl(url), QString::fromUtf8(text ? text : url));
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
void showCriticalMessageBoxDelayed(const QString &title, const QString &message, int milliseconds = 2000)
{
	auto newTitle = QString("%1 : %2").arg(PLUGIN_DISPLAY_NAME, title);

	auto newMessage = message;
	newMessage.remove(QRegularExpression("(\r?\n?<br>\r?\n?)+$"));
	auto newMessageFormat = QRegularExpression("(</ol>|</ul>)$").match(newMessage).hasMatch() ? "%1%2"
												  : "%1<br><br>%2";
	newMessage = QString(newMessageFormat)
			     .arg(newMessage,
				  QTStr("NDIPlugin.PluginLimitedFeatures")
					  .arg(PLUGIN_DISPLAY_NAME, rehostUrl(PLUGIN_REDIRECT_TROUBLESHOOTING_URL),
					       rehostUrl(PLUGIN_REDIRECT_REPORT_BUG_URL)));
	QTimer::singleShot(milliseconds, [newTitle, newMessage] {
		auto dlg = new QMessageBox(QMessageBox::Critical, newTitle, newMessage, QMessageBox::Ok);
#if defined(Q_OS_MACOS)
		// Make title bar show text: https://stackoverflow.com/a/22187538/25683720
		dlg->QDialog::setWindowTitle(newTitle);
#endif
		QObject::connect(dlg, &QMessageBox::finished, [](int result) {
			if (result != QMessageBox::Ok) {
				return;
			}

			if (output_settings) {
				output_settings->show();
				output_settings->raise();
				output_settings->activateWindow();
				obs_log(LOG_DEBUG,
					"showCriticalMessageBoxDelayed: opened DistroAV settings after OK click");
			} else {
				obs_log(LOG_DEBUG,
					"showCriticalMessageBoxDelayed: DistroAV settings are unavailable at OK click");
			}
		});
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowStaysOnTopHint);
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
			struct find_module_data *data_ = (struct find_module_data *)param;
			if (strcmp(data_->target_name, module_info->name) == 0) {

				// Privacy tweak : Replace the absolute path to the user's home directory with a relative path
				QString bin_path = QString::fromUtf8(module_info->bin_path);
				QString home_path = QDir::homePath();
				bin_path.replace(home_path, "[Redacted User Home Path]");

				obs_log(LOG_INFO, "is_module_found: '%s' found at '%s'", module_info->name,
					bin_path.toUtf8().constData());

				// DEBUG logging will output the absolute path.
				obs_log(LOG_DEBUG, "is_module_found: module_info->data_path='%s'",
					module_info->bin_path);
				data_->found = true;
				obs_log(LOG_DEBUG, "is_module_found: module_info->data_path='%s'",
					module_info->data_path);
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

bool is_version_supported(const char *version, const char *min_version)
{
	if (version == nullptr || min_version == nullptr) {
		return false;
	}

	auto version_parts = QString::fromUtf8(version).split(".");
	auto min_version_parts = QString::fromUtf8(min_version).split(".");

	for (int i = 0; i < qMax(version_parts.size(), min_version_parts.size()); ++i) {
		auto version_part = i < version_parts.size() ? version_parts[i].toInt() : 0;
		auto min_version_part = i < min_version_parts.size() ? min_version_parts[i].toInt() : 0;

		if (version_part > min_version_part) {
			return true;
		} else if (version_part < min_version_part) {
			return false;
		}
	}
	return true;
}

static void register_plugin_features()
{
	obs_log(LOG_DEBUG, "+register_plugin_features()");

	if (plugin_features_registered) {
		obs_log(LOG_DEBUG, "register_plugin_features: already registered");
		return;
	}

	// The plugin features below require the NDI library. They must be registered only if the NDI library is successfully loaded and initialized.
	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);

	ndi_filter_info = create_ndi_filter_info();
	obs_register_source(&ndi_filter_info);

	ndi_audiofilter_info = create_ndi_audiofilter_info();
	obs_register_source(&ndi_audiofilter_info);

	obs_log(LOG_DEBUG, "Plugin features registered: NDI source, NDI output, NDI filter, NDI audio filter");

	// The plugin features below do not require the NDI library. They can still be registered even if the NDI library fails to load or initialize. We do not leverage this split loading yet.
	alpha_filter_info = create_alpha_filter_info();
	obs_register_source(&alpha_filter_info);

	obs_log(LOG_DEBUG, "Plugin features registered: Alpha filter");

	plugin_features_registered = true;
	obs_log(LOG_INFO, "register_plugin_features: NDI plugin features registered");
	obs_log(LOG_DEBUG, "-register_plugin_features()");
}

bool obs_module_load(void)
{
	obs_log(LOG_DEBUG, "+obs_module_load()");
	obs_log(LOG_INFO, "obs_module_load: you can haz %s (Version %s)", PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
	// obs_log(LOG_DEBUG, "obs_module_load: Qt Version: %s (runtime), %s (compiled)", qVersion(), QT_VERSION_STR);

	Config::Initialize();

	// HARD requirement Check START
	// Process plugin's HARD requirements that would prevent the plugin from loading.

	// OBS-NDI conflict check
	// The plugin cannot work if the older OBS-NDI plugin is installed.
	if (is_obsndi_installed()) {
		obs_log(LOG_ERROR, "ERR-403 - OBS-NDI is detected and needs to be uninstalled before %s can work.",
			PLUGIN_DISPLAY_NAME);
		obs_log(LOG_DEBUG,
			"obs_module_load: OBS-NDI is detected and needs to be uninstalled before %s will load.",
			PLUGIN_DISPLAY_NAME);
		showCriticalMessageBoxDelayed(QTStr("NDIPlugin.ErrorObsNdiDetected.Title"),
					      QTStr("NDIPlugin.ErrorObsNdiDetected.Message")
						      .arg(rehostUrl(PLUGIN_REDIRECT_UNINSTALL_OBSNDI_URL)));
		return false;
	}
	obs_log(LOG_DEBUG, "obs_module_load: OBS-NDI leftover check complete. Continuing...");

	// Plugin's minimum functional OBS version check
	// The plugin cannot work if this minimum version requirement is not met.
	// This is a different requirement than the minimum OBS version required for features access.
	// Starting OBS 32+ this will not be required anymore. See PR : https://github.com/obsproject/obs-studio/pull/6916
	// TO DO : As soon as OBS 32 is used in the buildspec.json as the minimum OBS version, this check can be removed."
	if (!is_version_supported(obs_get_version_string(), PLUGIN_MIN_OBS_VERSION)) {

		// Bypass this check. Only for advanced users (not recommended)
		if (Config::CheckObsBypass) {
			obs_log(LOG_WARNING,
				"OBS version check ignore enabled - Continuing to load plugin even though OBS version detected (%s) is below the minimum required version (%s). This may lead to instability or crashes.",
				obs_get_version_string(), PLUGIN_MIN_OBS_VERSION);
		} else {
			// Default Beahvior
			obs_log(LOG_ERROR, "ERR-424 - %s requires at least OBS version %s.", PLUGIN_DISPLAY_NAME,
				PLUGIN_MIN_OBS_VERSION);
			obs_log(LOG_DEBUG,
				"obs_module_load: OBS version detected is not compatible. OBS version detected: %s. OBS version required: %s",
				obs_get_version_string(), PLUGIN_MIN_OBS_VERSION);

			auto title = "OBS version not supported";
			auto message =
				"Error-424: Plugin requires OBS " + QTStr(PLUGIN_MIN_OBS_VERSION) + " or higher <br>";
			showCriticalMessageBoxDelayed(title, message);

			return false;
		}
	}
	obs_log(LOG_DEBUG, "obs_module_load: Minimum API-level OBS version check complete. Continuing...");

	// HARD requirement Check END

	// Obtain OBS main window pointer for later use (error popup).
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	// SOFT requirement Check START
	// Process plugin's SOFT requirements that would limit functionality of the plugin but not fail the whole plugin.

	// OBS Version check (SOFT)
	// Plugin's minimum OBS version for feature access check
	// The minimum OBS version required to access all the features of the plugin.
	// TO DO : Leverage this to condionally enable/disable features based on the OBS version detected.
	if (Config::CheckObsForceFail || !is_version_supported(obs_get_version_string(), PLUGIN_MIN_OBS_VERSION)) {

		// Bypass this check. Only for advanced users (not recommended)
		if (Config::CheckObsBypass) {
			obs_log(LOG_WARNING,
				"OBS version requirement check for features ignore enabled - Continuing to load plugin with limited features even though OBS version detected (%s) is below the minimum required version (%s). This may lead to instability or crashes.",
				obs_get_version_string(), PLUGIN_MIN_OBS_VERSION);
		} else {
			// Default Beahvior
			obs_log(LOG_ERROR, "ERR-424 - %s requires at least OBS version %s.", PLUGIN_DISPLAY_NAME,
				PLUGIN_MIN_OBS_VERSION);
			obs_log(LOG_DEBUG,
				"obs_module_load: OBS version detected is not compatible. OBS version detected: %s. OBS version required: %s",
				obs_get_version_string(), PLUGIN_MIN_OBS_VERSION);

			auto title = "OBS version is too old for some features";
			auto message =
				"Error-424: Plugin requires OBS " + QTStr(PLUGIN_MIN_OBS_VERSION) + " or higher <br>";
			showCriticalMessageBoxDelayed(title, message);
		}
	}
	obs_log(LOG_DEBUG, "obs_module_load: Minimum Feature-level OBS version check complete. Continuing...");

	// NDI Library version Check (SOFT)
	// The plugin will be severely limited in functionality if the NDI library is not installed.
	// TO DO TBC Load library from a custom path defined by the user in the plugin settings.

	// This will force fail the lib load (for automated testing).
	ndiLib = Config::CheckNdiLibForceFail ? nullptr : load_ndilib();

	if (!ndiLib) {
		auto title = Str("NDIPlugin.LibError.Title");
		auto message = "Error-401: " + QTStr("NDIPlugin.LibError.Message") + "<br>";

		message += makeLink(PLUGIN_REDIRECT_NDI_REDIST_URL);

		obs_log(LOG_ERROR, "ERR-401 - NDI library failed to load with message: '%s'", QT_TO_UTF8(message));
		obs_log(LOG_DEBUG, "obs_module_load: ERROR - load_ndilib() failed; message=%s", QT_TO_UTF8(message));
		showCriticalMessageBoxDelayed(title, message);
	} else {
		obs_log(LOG_INFO, "obs_module_load: NDI library detected");

		// NDI Library Initialization check
		// The Library is found but might fail to load on unsupported hardware.
		auto initialized = ndiLib->initialize();
		if (!initialized) {
			obs_log(LOG_ERROR,
				"ERR-406 - NDI library could not initialize. Usually due to unsupported CPU.");
			obs_log(LOG_DEBUG,
				"obs_module_load: ndiLib->initialize() failed; CPU unsupported by NDI library.");
			// return false;
		} else {
			obs_log(LOG_INFO, "obs_module_load: NDI library initialized ('%s')", ndiLib->version());

			// NDI Library Minimum Version check
			// Check if the minimum NDI Runtime/SDK required by this plugin is used
			// Alternative Regex : R"((\d+\.\d+(?:\.\d+)?(?:\.\d+)?$))" (untested)
			QString ndi_version_short = QRegularExpression(R"((\d+\.\d+(\.\d+)?(\.\d+)?$))")
							    .match(ndiLib->version())
							    .captured(1);
			obs_log(LOG_INFO, "NDI Library Version detected: %s", QT_TO_UTF8(ndi_version_short));

			if (!is_version_supported(QT_TO_UTF8(ndi_version_short), PLUGIN_MIN_NDI_VERSION) &&
			    !Config::CheckNdiLibBypass) {
				obs_log(LOG_ERROR,
					"ERR-425 - %s requires at least NDI version %s. NDI Version detected: %s.",
					PLUGIN_DISPLAY_NAME, PLUGIN_MIN_NDI_VERSION, QT_TO_UTF8(ndi_version_short));
				obs_log(LOG_DEBUG,
					"obs_module_load: NDI minimum version not met (%s). NDI version detected: %s.",
					PLUGIN_MIN_NDI_VERSION, ndiLib->version());

				auto title = "NDI Library version not supported";
				auto message =
					"Error-425: Plugin requires NDI " + QTStr(PLUGIN_MIN_NDI_VERSION) +
					" or higher <br> <br> Version detected: " + QT_TO_UTF8(ndi_version_short) +
					"<br> Get the latest NDI library at: <br>";
				message += makeLink(PLUGIN_REDIRECT_NDI_REDIST_URL);
				showCriticalMessageBoxDelayed(title, message);
			} else {
				// Clearly inform that this check has been ignoreed. Only for advanced users (not recommended)
				if (Config::CheckNdiLibBypass) {
					obs_log(LOG_WARNING,
						"NDI Library version requirement check has been ignoreed - Continuing to load plugin. This may lead to instability or crashes.");
				} else {
					obs_log(LOG_INFO,
						"obs_module_load: NDI library version detected (%s) is compatible",
						ndiLib->version());
					obs_log(LOG_DEBUG,
						"obs_module_load: NDI minimum version required (%s). NDI version detected: %s.",
						PLUGIN_MIN_NDI_VERSION, ndiLib->version());
				}

				// All seems compatible, proceed to register plugin features.
				register_plugin_features();
			}
		}
	}
	// SOFT requirement Check END

	if (main_window) {
		auto menu_action = static_cast<QAction *>(
			obs_frontend_add_tools_menu_qaction(obs_module_text("NDIPlugin.Menu.OutputSettings")));

		obs_frontend_push_ui_translation(obs_module_get_string);
		output_settings = new OutputSettings(main_window);
		obs_frontend_pop_ui_translation();

		auto menu_cb = [] {
			output_settings->toggleShowHide();
		};
		menu_action->connect(menu_action, &QAction::triggered, menu_cb);

		obs_frontend_add_event_callback(
			[](enum obs_frontend_event event, void *) {
				if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
					if (!plugin_features_registered) {
						obs_log(LOG_WARNING,
							"obs_module_load: NDI features not registered; skipping output initialization.");
						return;
					}

					auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
					QMetaObject::invokeMethod(
						main_window,
						[] {
							main_output_init();
							preview_output_init();
							ensure_ndi_filters_started();
						},
						Qt::QueuedConnection);
				} else if (event == OBS_FRONTEND_EVENT_EXIT) {
					// Unknown why putting this in obs_module_unload causes a crash when closing OBS
					main_output_deinit();
					preview_output_deinit();
				} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING) {
					main_output_deinit();
					preview_output_deinit();
				} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
					if (plugin_features_registered) {
						main_output_init();
						preview_output_init();
					}
				}
			},
			nullptr);
	}
	if (plugin_features_registered) {
		obs_log(LOG_INFO, "plugin loaded (full NDI features) (version %s)", PLUGIN_VERSION);
	} else {
		obs_log(LOG_INFO, "plugin loaded (UI-only) (version %s)", PLUGIN_VERSION);
	}
	obs_log(LOG_DEBUG, "-obs_module_load()");
	return true;
}

void obs_module_post_load(void)
{
	obs_log(LOG_DEBUG, "+obs_module_post_load()");

	// Check for new updates after the plugin has loaded.
	updateCheckStart();

	obs_log(LOG_DEBUG, "-obs_module_post_load()");
}

void obs_module_unload(void)
{
	obs_log(LOG_DEBUG, "+obs_module_unload()");

	updateCheckStop();

	if (ndiLib) {
		ndiLib->destroy();
		ndiLib = nullptr;
	}

	if (loaded_lib) {
		delete loaded_lib;
	}

	obs_log(LOG_DEBUG, "-obs_module_unload(): goodbye!");
}

const NDIlib_v6 *load_ndilib()
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
	// Flatpak install will look for the NDI lib in /app/plugins/DistroAV/extra/lib
	locations << "/usr/lib";
	locations << "/usr/lib64";
	locations << "/usr/local/lib";
#if defined(Q_OS_LINUX)
	locations << "/app/plugins/DistroAV/extra/lib";
#endif
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
					lib_path = dir.absoluteFilePath(file_name);
				}
			}
		}
#else
		// MacOS, Windows
		temp_path = QDir::cleanPath(dir.absoluteFilePath(NDILIB_LIBRARY_NAME));
		obs_log(LOG_DEBUG, "load_ndilib: Trying '%s'", QT_TO_UTF8(QDir::toNativeSeparators(temp_path)));
		auto file_info = QFileInfo(temp_path);
		if (file_info.exists() && file_info.isFile()) {
			lib_path = temp_path;
			break;
		}
#endif
	}
	if (!lib_path.isEmpty()) {
		obs_log(LOG_DEBUG, "load_ndilib: Found '%s'; attempting to load NDI library...",
			QT_TO_UTF8(QDir::toNativeSeparators(lib_path)));
		loaded_lib = new QLibrary(lib_path, nullptr);
		if (loaded_lib->load()) {
			obs_log(LOG_DEBUG, "load_ndilib: NDI library loaded successfully");
			NDIlib_v6_load_ lib_load =
				reinterpret_cast<NDIlib_v6_load_>(loaded_lib->resolve("NDIlib_v6_load"));
			if (lib_load != nullptr) {
				obs_log(LOG_DEBUG, "load_ndilib: NDIlib_v6_load found");
				return lib_load();
			} else {
				obs_log(LOG_ERROR, "ERR-405 - Error loading the NDI Library from path: '%s'",
					QT_TO_UTF8(QDir::toNativeSeparators(lib_path)));
				obs_log(LOG_DEBUG, "load_ndilib: ERROR: NDIlib_v6_load not found in loaded library");
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		} else {
			obs_log(LOG_ERROR, "ERR-402 - Error loading QLibrary with error: '%s'",
				QT_TO_UTF8(loaded_lib->errorString()));
			obs_log(LOG_DEBUG, "load_ndilib: ERROR: QLibrary returned the following error: '%s'",
				QT_TO_UTF8(loaded_lib->errorString()));
			delete loaded_lib;
			loaded_lib = nullptr;
		}
	}

	obs_log(LOG_ERROR,
		"ERR-404 - NDI library not found, DistroAV features will not be available. Read the wiki and install the NDI Libraries.");
	obs_log(LOG_DEBUG, "load_ndilib: ERROR: Can't find the NDI library");
	return nullptr;
}
