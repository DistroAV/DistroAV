#ifdef _WIN32
#include <Windows.h>
#endif
#include <sys/stat.h>
#include <QMainWindow>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "obs-ndi.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

bool obs_module_load(void)
{
	blog(LOG_INFO, "Hello! (Plugin Version %s | Linked LibNDI Version %s)", OBS_NDI_VERSION, LIBNDI_VERSION);

	QMainWindow* main_window = (QMainWindow*)obs_frontend_get_main_window();

	//blog(LOG_INFO, "NDI library initialized successfully (%s)", ndiLib->version());

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "Goodbye!");
}

const char* obs_module_description()
{
	return "NDI input/output integration for OBS Studio";
}
