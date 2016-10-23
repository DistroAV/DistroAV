#ifdef _WIN32
#include <Windows.h>
#endif
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <Processing.NDI.Lib.h>
#include "obs-ndi.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

NDIlib_find_instance_t ndi_finder;
obs_output_t *ndi_out;

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs-ndi] hello ! (version %.1f)", OBS_NDI_VERSION);

	bool ndi_init = NDIlib_initialize();
	if (!ndi_init) {
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

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);
	
	obs_frontend_add_tools_menu_item("Start NDI Output", [](void *private_data) {
		if (!ndi_out || !obs_output_active(ndi_out)) {
			obs_output_release(ndi_out);

			obs_data_t *output_settings = obs_data_create();
			obs_data_set_string(output_settings, "ndi_name", "OBS");
			ndi_out = obs_output_create("ndi_output", "simple_ndi_output", output_settings, nullptr);
			obs_output_start(ndi_out);
			obs_data_release(output_settings);
		}
	}, nullptr);

	obs_frontend_add_tools_menu_item("Stop NDI Output", [](void *private_data) {
		if (ndi_out) {
			obs_output_stop(ndi_out);
		}
	}, nullptr);

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs-ndi] goodbye !");
	obs_output_release(ndi_out);
	NDIlib_find_destroy(ndi_finder);
	NDIlib_destroy();
}