#ifdef _WIN32
#include <Windows.h>
#endif
#include <obs-module.h>
#include <Processing.NDI.Lib.h>
#include "obs-ndi.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

NDIlib_find_instance_t ndi_finder;

bool obs_module_load(void) 
{
	blog(LOG_INFO, "[obs-ndi] hello ! (version %.1f)", OBS_NDI_VERSION);

	bool ndi_init = NDIlib_initialize();
	if(!ndi_init) {
		blog(LOG_ERROR, "[obs-ndi] CPU unsupported by NDI library. Module won't load.");
		return false;
	}

	NDIlib_find_create_t find_desc = { 0 };
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;

	ndi_finder = NDIlib_find_create(&find_desc);

	DWORD no_sources;
	NDIlib_find_get_sources(ndi_finder, &no_sources, 5000);

	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs-ndi] goodbye !");
	NDIlib_find_destroy(ndi_finder);
	NDIlib_destroy();
}