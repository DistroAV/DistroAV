/*
	obs-ndi (NDI I/O in OBS Studio)
	Copyright (C) 2016 Stéphane Lepin <stephane.lepin@gmail.com>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#ifdef _WIN32
#include <Windows.h>
#endif

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <Processing.NDI.Lib.h>
#include "obs-ndi.h"
#include "Config.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

extern struct obs_source_info create_ndi_filter_info();
struct obs_source_info ndi_filter_info;

NDIlib_find_instance_t ndi_finder;
obs_output_t *main_out;
bool main_output_running;

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs-ndi] hello ! (version %s)", OBS_NDI_VERSION);

	bool ndi_init = NDIlib_initialize();
	if (!ndi_init) {
		blog(LOG_ERROR, "[obs-ndi] CPU unsupported by NDI library. Module won't load.");
		return false;
	}

	main_output_running = false;

	NDIlib_find_create_t find_desc = { 0 };
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	ndi_finder = NDIlib_find_create(&find_desc);

	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);
	
	ndi_filter_info = create_ndi_filter_info();
	obs_register_source(&ndi_filter_info);

	obs_frontend_add_save_callback(Config::OBSSaveCallback, Config::Current());

	obs_frontend_add_tools_menu_item(obs_module_text("Tools_StartNDIOutput"), [](void *private_data) {
		main_output_start();
	}, nullptr);

	obs_frontend_add_tools_menu_item(obs_module_text("Tools_StopNDIOutput"), [](void *private_data) {
		main_output_stop();
	}, nullptr);

	obs_frontend_add_event_callback([](enum obs_frontend_event event, void *private_data) {
		if (event == OBS_FRONTEND_EVENT_EXIT) {
			main_output_stop();
		}
	}, nullptr);

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs-ndi] goodbye !");

	NDIlib_find_destroy(ndi_finder);
	NDIlib_destroy();
}

void main_output_start() {
	if (!main_output_running) {
		blog(LOG_INFO, "[obs-ndi] starting main NDI output with name '%s'", Config::Current()->OutputName);

		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "ndi_name", Config::Current()->OutputName);
		
		main_out = obs_output_create("ndi_output", "main_ndi_output", output_settings, nullptr);
		obs_output_start(main_out);
		obs_data_release(output_settings);
		
		main_output_running = true;
	}
}

void main_output_stop() {
	if (main_output_running) {
		blog(LOG_INFO, "[obs-ndi] stopping main NDI output");

		obs_output_stop(main_out);
		obs_output_release(main_out);
		main_output_running = false;
	}
}

bool main_output_is_running() {
	return main_output_running;
}