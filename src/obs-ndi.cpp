#ifdef _WIN32
#include <Windows.h>
#endif
#include <sys/stat.h>
#include <QMainWindow>
#include <QLibrary>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QAction>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "obs-ndi.h"
#include "obs-ndi-input.h"
#include "obs-ndi-output.h"
#include "forms/SettingsDialog.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

SettingsDialog *_settingsDialog = nullptr;

// Global NDILib pointer
const NDIlib_v5 *ndiLib = nullptr;
// QLibrary pointer for the loaded NDILib binary file
QLibrary *loaded_lib = nullptr;
// Define NDILib load function
const NDIlib_v5 *load_ndilib();

NDIlib_find_instance_t ndi_finder = nullptr;

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs_module_load] Hello! (Plugin Version %s | Linked NDILib Version %s)", OBS_NDI_VERSION, NDILIB_HEADERS_VERSION);

	// Get main window pointer
	QMainWindow *mainWindow = (QMainWindow*)obs_frontend_get_main_window();
	if (!mainWindow) {
		blog(LOG_ERROR, "[obs_module_load] MainWindow not found! Cannot load.");
		return false;
	}

	// Create the Settings Dialog
	obs_frontend_push_ui_translation(obs_module_get_string);
	_settingsDialog = new SettingsDialog(mainWindow);
	obs_frontend_pop_ui_translation();

	// Add the settings dialog as a menu action the the Tools menu
	const char* menuActionText = obs_module_text("Settings.Title");
	QAction *menuAction = (QAction*)obs_frontend_add_tools_menu_qaction(menuActionText);
	QObject::connect(menuAction, &QAction::triggered, [] { _settingsDialog->ToggleShowHide(); });

	ndiLib = load_ndilib();
	if (ndiLib) { // TODO: Show messagebox after OBS finishes loading
		blog(LOG_DEBUG, "[obs_module_load] Loaded NDIlib binary.");
	} else {
		std::string error_string_id = "Plugin.Load.LibError.Message.";

#if defined(_MSC_VER)
		error_string_id += "Windows";
#elif defined(__APPLE__)
		error_string_id += "MacOS";
#else
		error_string_id += "Linux";
#endif

		QMessageBox::critical(mainWindow,
			obs_module_text("Plugin.Load.LibError.Title"),
			obs_module_text(error_string_id.c_str()),
			QMessageBox::Ok, QMessageBox::NoButton);
		return false;
	}

	if (ndiLib->NDIlib_initialize()) {
		blog(LOG_DEBUG, "[obs_module_load] Initialized NDIlib.");
	} else {
		blog(LOG_ERROR, "[obs_module_load] NDIlib failed to initialize. Plugin disabled. Your CPU may not be supported.");
		return false;
	}

	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	ndi_finder = ndiLib->NDIlib_find_create_v2(&find_desc);
	if (ndi_finder) {
		blog(LOG_DEBUG, "[obs_module_load] Initialized NDI Finder.");
	} else {
		blog(LOG_ERROR, "[obs_module_load] Failed to initialize NDI finder. Plugin disabled.");
		return false;
	}

	blog(LOG_INFO, "[obs_module_load] NDI runtime loaded. Version: %s", ndiLib->NDIlib_version());

	register_ndi_source_info();

	//register_ndi_output_info();

	blog(LOG_INFO, "[obs_module_load] Finished loading.");

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs_module_unload] Goodbye!");

	if (ndiLib) {
		if (ndi_finder)
			ndiLib->NDIlib_find_destroy(ndi_finder);
		ndiLib->NDIlib_destroy();
	}

	if (loaded_lib) {
		delete loaded_lib;
	}
}

const char* obs_module_description()
{
	return "NDI input/output integration for OBS Studio";
}

typedef const NDIlib_v5 *(*NDIlib_v5_load_)(void);

const NDIlib_v5 *load_ndilib()
{
	std::vector<std::string> libraryLocations;
	const char* redistFolder = std::getenv(NDILIB_REDIST_FOLDER);
	if (redistFolder)
		libraryLocations.push_back(redistFolder);
#if defined(__linux__) || defined(__APPLE__)
	libraryLocations.push_back("/usr/lib");
	libraryLocations.push_back("/usr/lib64");
	libraryLocations.push_back("/usr/lib/x86_64-linux-gnu");
	libraryLocations.push_back("/usr/local/lib");
	libraryLocations.push_back("/usr/local/lib64");
#endif

	for (std::string path : libraryLocations) {
		blog(LOG_DEBUG, "[load_ndilib] Trying library path: '%s'", path.c_str());
		QFileInfo libPath(QDir(QString::fromStdString(path)).absoluteFilePath(NDILIB_LIBRARY_NAME));

		if (libPath.exists() && libPath.isFile()) {
			QString libFilePath = libPath.absoluteFilePath();
			blog(LOG_INFO, "[load_ndilib] Found NDI library file at '%s'",
				libFilePath.toUtf8().constData());

			loaded_lib = new QLibrary(libFilePath, nullptr);
			if (loaded_lib->load()) {
				blog(LOG_INFO, "[load_ndilib] NDI runtime loaded successfully.");

				NDIlib_v5_load_ lib_load = (NDIlib_v5_load_)loaded_lib->resolve("NDIlib_v5_load");

				if (lib_load != nullptr)
					return lib_load();
				else
					blog(LOG_ERROR, "[load_ndilib] NDIlib_v5_load not found in loaded library.");
			}
			else {
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		}
	}

	blog(LOG_ERROR, "[load_ndilib] Can't find the NDI 5 library!");
	return nullptr;
}
