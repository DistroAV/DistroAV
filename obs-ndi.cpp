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
#include <util/platform.h>
#include "obs-ndi.h"
#include "Config.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

const NDIlib_v2* ndiLib = nullptr;

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

extern struct obs_source_info create_ndi_filter_info();
struct obs_source_info ndi_filter_info;

void* loaded_lib = nullptr;

NDIlib_find_instance_t ndi_finder;
obs_output_t *main_out;
bool main_output_running;

bool obs_module_load(void)
{
	blog(LOG_INFO, "hello ! (version %s)", OBS_NDI_VERSION);

	ndiLib = NDIlib_v2_load();
	if (!ndiLib)
	{
		blog(LOG_ERROR, "Error when loading the NDI library. Are the NDI redistributables installed?");
		return false;
	}

	if (!ndiLib->NDIlib_initialize())
	{
		blog(LOG_ERROR, "CPU unsupported by NDI library. Module won't load.");
		return false;
	}

	main_output_running = false;

	NDIlib_find_create_t find_desc = { 0 };
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	ndi_finder = ndiLib->NDIlib_find_create(&find_desc);

	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);
	
	ndi_filter_info = create_ndi_filter_info();
	obs_register_source(&ndi_filter_info);

	Config* conf = Config::Current();
	conf->Load();

	obs_frontend_add_tools_menu_item(obs_module_text("NDIPlugin.Tools.StartNDIOutput"), [conf](void *private_data) {
		main_output_start(conf->OutputName);
	}, nullptr);

	obs_frontend_add_tools_menu_item(obs_module_text("NDIPlugin.Tools.StopNDIOutput"), [](void *private_data) {
		main_output_stop();
	}, nullptr);

	obs_frontend_add_event_callback([](enum obs_frontend_event event, void *private_data) {
		if (event == OBS_FRONTEND_EVENT_EXIT) {
			main_output_stop();
		}
	}, nullptr);

	if (conf->OutputEnabled)
		main_output_start(conf->OutputName);

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "goodbye !");

	if (ndiLib)
	{
		ndiLib->NDIlib_find_destroy(ndi_finder);
		ndiLib->NDIlib_destroy();
	}

	if (loaded_lib)
		os_dlclose(loaded_lib);
}

void main_output_start(const char* output_name) {
	if (!main_output_running) {
		blog(LOG_INFO, "starting main NDI output with name '%s'", Config::Current()->OutputName);

		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "ndi_name", output_name);
		
		main_out = obs_output_create("ndi_output", "main_ndi_output", output_settings, nullptr);
		obs_output_start(main_out);
		obs_data_release(output_settings);
		
		main_output_running = true;
	}
}

void main_output_stop() {
	if (main_output_running) {
		blog(LOG_INFO, "stopping main NDI output");

		obs_output_stop(main_out);
		obs_output_release(main_out);
		main_output_running = false;
	}
}

bool main_output_is_running() {
	return main_output_running;
}

const char* GetNDILibPath()
{
	char *path = "";

	#if defined(_WIN32) || defined(_WIN64)
		const char* runtime_dir = getenv("NDI_RUNTIME_DIR_V2");

		const char* dll_file;

		#if defined(_WIN64)
		dll_file = "Processing.NDI.Lib.x64.dll";
		#elif defined(_WIN32)
		dll_file = "Processing.NDI.Lib.x86.dll";
		#endif

		int buf_size = strlen(runtime_dir) + strlen(dll_file) + 3;
		path = (char*)bmalloc(buf_size);
		memset(path, 0, buf_size);

		strcat(path, runtime_dir);
		strcat(path, "\\");
		strcat(path, dll_file);
	#endif

	return path;
}

const NDIlib_v2* NDIlib_v2_load()
{
	const char* dll_file = GetNDILibPath();
	blog(LOG_INFO, "%s", dll_file);

	loaded_lib = os_dlopen(dll_file);

	bfree((void*)dll_file);

	if (loaded_lib)
	{
		NDIlib_v2* sct = (NDIlib_v2*)bmalloc(sizeof(NDIlib_v2));
		
		// General lib
		sct->NDIlib_initialize = (bool(*)())os_dlsym(loaded_lib, "NDIlib_initialize");
		sct->NDIlib_destroy = (void(*)())os_dlsym(loaded_lib, "NDIlib_destroy");
		sct->NDIlib_version = (const char*(*)())os_dlsym(loaded_lib, "NDIlib_version");
		sct->NDIlib_is_supported_CPU = (bool(*)())os_dlsym(loaded_lib, "NDIlib_is_supported_cpu");
		
		// Find
		sct->NDIlib_find_create = (NDIlib_find_instance_t(*)(const NDIlib_find_create_t* p_create_settings))os_dlsym(loaded_lib, "NDIlib_find_create");
		sct->NDIlib_find_create2 = (NDIlib_find_instance_t(*)(const NDIlib_find_create_t* p_create_settings))os_dlsym(loaded_lib, "NDIlib_find_create2");
		sct->NDIlib_find_destroy = (void(*)(NDIlib_find_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_find_destroy");
		sct->NDIlib_find_get_sources = (const NDIlib_source_t*(*)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources, uint32_t timeout_in_ms))os_dlsym(loaded_lib, "NDIlib_find_get_sources");
		
		// Send
		sct->NDIlib_send_create = (NDIlib_send_instance_t(*)(const NDIlib_send_create_t* p_create_settings))os_dlsym(loaded_lib, "NDIlib_send_create");
		sct->NDIlib_send_destroy = (void(*)(NDIlib_send_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_send_destroy");
		sct->NDIlib_send_send_video = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data))os_dlsym(loaded_lib, "NDIlib_send_send_video");
		sct->NDIlib_send_send_video_async = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data))os_dlsym(loaded_lib, "NDIlib_send_send_video_async");
		sct->NDIlib_send_send_audio = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data))os_dlsym(loaded_lib, "NDIlib_send_send_audio");
		sct->NDIlib_send_send_metadata = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata))os_dlsym(loaded_lib, "NDIlib_send_send_metadata");
		sct->NDIlib_send_capture = (NDIlib_frame_type_e(*)(NDIlib_send_instance_t p_instance, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms))os_dlsym(loaded_lib, "NDIlib_send_capture");
		sct->NDIlib_send_free_metadata = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata))os_dlsym(loaded_lib, "NDIlib_send_free_metadata");
		sct->NDIlib_send_get_tally = (bool(*)(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms))os_dlsym(loaded_lib, "NDIlib_send_get_tally");
		sct->NDIlib_send_get_no_connections = (int(*)(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms))os_dlsym(loaded_lib, "NDIlib_send_get_no_connections");
		sct->NDIlib_send_clear_connection_metadata = (void(*)(NDIlib_send_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_send_clear_connection_metadata");
		sct->NDIlib_send_add_connection_metadata = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata))os_dlsym(loaded_lib, "NDIlib_send_add_connection_metadata");
		sct->NDIlib_send_set_failover = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_source_t* p_failover_source))os_dlsym(loaded_lib, "NDIlib_send_set_failover");

		// Recv
		sct->NDIlib_recv_create2 = (NDIlib_recv_instance_t(*)(const NDIlib_recv_create_t* p_create_settings))os_dlsym(loaded_lib, "NDIlib_recv_create2");
		sct->NDIlib_recv_create = (NDIlib_recv_instance_t(*)(const NDIlib_recv_create_t* p_create_settings))os_dlsym(loaded_lib, "NDIlib_recv_create");
		sct->NDIlib_recv_destroy = (void(*)(NDIlib_recv_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_recv_destroy");
		sct->NDIlib_recv_capture = (NDIlib_frame_type_e(*)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_t* p_video_data, NDIlib_audio_frame_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms))os_dlsym(loaded_lib, "NDIlib_recv_capture");
		sct->NDIlib_recv_free_video = (void(*)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_t* p_video_data))os_dlsym(loaded_lib, "NDIlib_recv_free_video");
		sct->NDIlib_recv_free_audio = (void(*)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data))os_dlsym(loaded_lib, "NDIlib_recv_free_audio");
		sct->NDIlib_recv_free_metadata = (void(*)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata))os_dlsym(loaded_lib, "NDIlib_recv_free_metadata");
		sct->NDIlib_recv_send_metadata = (bool(*)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata))os_dlsym(loaded_lib, "NDIlib_recv_send_metadata");
		sct->NDIlib_recv_set_tally = (bool(*)(NDIlib_recv_instance_t p_instance, const NDIlib_tally_t* p_tally))os_dlsym(loaded_lib, "NDIlib_recv_set_tally");
		sct->NDIlib_recv_get_performance = (void(*)(NDIlib_recv_instance_t p_instance, NDIlib_recv_performance_t* p_total, NDIlib_recv_performance_t* p_dropped))os_dlsym(loaded_lib, "NDIlib_recv_get_performance");
		sct->NDIlib_recv_get_queue = (void(*)(NDIlib_recv_instance_t p_instance, NDIlib_recv_queue_t* p_total))os_dlsym(loaded_lib, "NDIlib_recv_get_queue");
		sct->NDIlib_recv_clear_connection_metadata = (void(*)(NDIlib_recv_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_recv_clear_connection_metadata");
		sct->NDIlib_recv_add_connection_metadata = (void(*)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata))os_dlsym(loaded_lib, "NDIlib_recv_add_connection_metadata");
		sct->NDIlib_recv_is_connected = (bool(*)(NDIlib_recv_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_recv_is_connected");
		
		// Routing
		sct->NDIlib_routing_create = (NDIlib_routing_instance_t(*)(const NDIlib_routing_create_t* p_create_settings))os_dlsym(loaded_lib, "NDIlib_routing_create");
		sct->NDIlib_routing_destroy = (void(*)(NDIlib_routing_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_routing_destroy");
		sct->NDIlib_routing_change = (bool(*)(NDIlib_routing_instance_t p_instance, const NDIlib_source_t* p_source))os_dlsym(loaded_lib, "NDIlib_routing_change");
		sct->NDIlib_routing_clear = (bool(*)(NDIlib_routing_instance_t p_instance))os_dlsym(loaded_lib, "NDIlib_routing_clear");
		
		// Util
		sct->NDIlib_util_send_send_audio_interleaved_16s = (void(*)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_16s_t* p_audio_data))os_dlsym(loaded_lib, "NDIlib_util_send_send_audio_interleaved_16s");
		sct->NDIlib_util_audio_to_interleaved_16s = (void(*)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst))os_dlsym(loaded_lib, "NDIlib_util_audio_to_interleaved_16s");
		sct->NDIlib_util_audio_from_interleaved_16s = (void(*)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_t* p_dst))os_dlsym(loaded_lib, "NDIlib_util_audio_from_interleaved_16s");

		// v2
		sct->NDIlib_find_wait_for_sources = (bool(*)(NDIlib_find_instance_t p_instance, uint32_t timeout_in_ms))os_dlsym(loaded_lib, "NDIlib_find_wait_for_sources");
		sct->NDIlib_find_get_current_sources = (const NDIlib_source_t* (*)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources))os_dlsym(loaded_lib, "NDIlib_find_get_current_sources");
		
		return sct;
	}

	return nullptr;
}