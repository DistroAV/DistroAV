#include <cmath>
#include <chrono>
#include <obs-module.h>

#include "obs-ndi.h"
#include "obs-ndi-input.h"

#define do_log(level, format, ...) blog(level, "[ndi_input_v5: '%s'] " format, obs_source_get_name(source), ##__VA_ARGS__)

void fill_ndi_sources(obs_property_t *list)
{
	uint32_t source_count = 0;
	const NDIlib_source_t* sources = ndiLib->NDIlib_find_get_current_sources(ndi_finder, &source_count);

	obs_property_list_clear(list);
	for (uint32_t i = 0; i < source_count; ++i) {
		obs_property_list_add_string(list, sources[i].p_ndi_name, sources[i].p_ndi_name);
		blog(LOG_DEBUG, "[fill_ndi_sources] NDI Source: %s", sources[i].p_ndi_name);
	}
}

ndi_input::ndi_input(obs_data_t *settings, obs_source_t *source) :
	source(source),
	ndi_recv(nullptr),
	running(false),
	perf_token(nullptr)
{
	update(settings);
}

ndi_input::~ndi_input()
{
	stop_recv();
}

void ndi_input::defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, P_BANDWIDTH, OBS_NDI_BANDWIDTH_HIGHEST);
	obs_data_set_default_bool(settings, P_INPUT_UNBUFFERED, false);
	obs_data_set_default_bool(settings, P_HARDWARE_ACCEL, false);
	obs_data_set_default_bool(settings, P_AUDIO, true);
	obs_data_set_default_int(settings, P_COLOR_RANGE, OBS_NDI_COLOR_RANGE_PARTIAL);
}

obs_properties_t *ndi_input::properties()
{
	obs_properties_t* props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_property_t* source_list = obs_properties_add_list(props, P_SOURCE_NAME, T_SOURCE_NAME, OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	fill_ndi_sources(source_list);

	obs_properties_add_button(props, P_REFRESH_SOURCES, T_REFRESH_SOURCES, [](obs_properties_t *ppts, obs_property_t *, void*)
	{
		obs_property_t* source_list = obs_properties_get(ppts, P_SOURCE_NAME);
		fill_ndi_sources(source_list);
		return true;
	});

	obs_property_t *bw_list = obs_properties_add_list(props, P_BANDWIDTH, T_BANDWIDTH, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(bw_list, T_BANDWIDTH_HIGHEST, OBS_NDI_BANDWIDTH_HIGHEST);
	obs_property_list_add_int(bw_list, T_BANDWIDTH_LOWEST, OBS_NDI_BANDWIDTH_LOWEST);
	obs_property_list_add_int(bw_list, T_BANDWIDTH_AUDIO_ONLY, OBS_NDI_BANDWIDTH_AUDIO_ONLY);

	obs_properties_add_bool(props, P_INPUT_UNBUFFERED, T_INPUT_UNBUFFERED);
	obs_properties_add_bool(props, P_HARDWARE_ACCEL, T_HARDWARE_ACCEL);
	obs_properties_add_bool(props, P_AUDIO, T_AUDIO);

	obs_property_t *color_range_list = obs_properties_add_list(props, P_COLOR_RANGE, T_COLOR_RANGE, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(color_range_list, T_COLOR_RANGE_PARTIAL, OBS_NDI_COLOR_RANGE_PARTIAL);
	obs_property_list_add_int(color_range_list, T_COLOR_RANGE_FULL, OBS_NDI_COLOR_RANGE_FULL);

	return props;
}

void ndi_input::update(obs_data_t *settings)
{
	stop_recv();

	bool input_unbuffered = obs_data_get_bool(settings, P_INPUT_UNBUFFERED);
	obs_source_set_async_unbuffered(source, input_unbuffered);

	enum ndi_input_color_range color_range = (ndi_input_color_range)obs_data_get_int(settings, P_COLOR_RANGE);
	yuv_range = input_color_range_to_obs(color_range);
	
	NDIlib_recv_create_v3_t recv_desc;
	recv_desc.source_to_connect_to.p_ndi_name = obs_data_get_string(settings, P_SOURCE_NAME);
	recv_desc.color_format = NDIlib_recv_color_format_UYVY_BGRA;
	enum ndi_input_bandwidth input_bandwidth = (ndi_input_bandwidth)obs_data_get_int(settings, P_BANDWIDTH);
	recv_desc.bandwidth = input_bandwidth_to_ndi(input_bandwidth);
	recv_desc.allow_video_fields = true;
	recv_desc.p_ndi_recv_name = obs_source_get_name(source);

	ndi_recv = ndiLib->NDIlib_recv_create_v3(&recv_desc);
	if (!ndi_recv) {
		do_log(LOG_ERROR, "[ndi_input::update] Failed to create new NDI receiver.");
		return;
	}

	// TODO: Hardware accel
	//bool enable_hardware_accel = obs_data_get_bool(settings, P_HARDWARE_ACCEL);

	running = true;
	video_thread = std::thread([this](){
		blog(LOG_INFO, "[ndi_input_v5: '%s'] [ndi_input::ndi_video_thread] Video thread started.", obs_source_get_name(source));
		this->ndi_video_thread();
		blog(LOG_INFO, "[ndi_input_v5: '%s'] [ndi_input::ndi_video_thread] Video thread stopped.", obs_source_get_name(source));
		obs_source_output_video(source, nullptr); // If OBS has a large frame buffer (like 25 frames), this could prematurely cut off video.
	});
	bool enable_audio = obs_data_get_bool(settings, P_AUDIO);
	if (enable_audio) {
		audio_thread = std::thread([this](){
			blog(LOG_INFO, "[ndi_input_v5: '%s'] [ndi_input::ndi_audio_thread] Audio thread started.", obs_source_get_name(source));
			this->ndi_audio_thread();
			blog(LOG_INFO, "[ndi_input_v5: '%s'] [ndi_input::ndi_audio_thread] Audio thread stopped.", obs_source_get_name(source));
		});
	} else {
		do_log(LOG_DEBUG, "[ndi_input::update] Not starting audio thread because audio is disabled.");
	}

	tally.on_preview = obs_source_active(source);
	tally.on_program = obs_source_showing(source);
	ndiLib->NDIlib_recv_set_tally(ndi_recv, &tally);
}

void ndi_input::activate()
{
	if (running && ndi_recv) {
		tally.on_program = true;
		ndiLib->NDIlib_recv_set_tally(ndi_recv, &tally);
	}
}

void ndi_input::deactivate()
{
	if (running && ndi_recv) {
		tally.on_program = false;
		ndiLib->NDIlib_recv_set_tally(ndi_recv, &tally);
	}
}

void ndi_input::show()
{
	if (running && ndi_recv) {
		tally.on_preview = true;
		ndiLib->NDIlib_recv_set_tally(ndi_recv, &tally);
	}
}

void ndi_input::hide()
{
	if (running && ndi_recv) {
		tally.on_preview = false;
		ndiLib->NDIlib_recv_set_tally(ndi_recv, &tally);
	}
}

void ndi_input::stop_recv()
{
	running = false;
	if (video_thread.joinable())
		video_thread.join();
	if (audio_thread.joinable())
		audio_thread.join();

	if (ndi_recv) {
		ndiLib->NDIlib_recv_destroy(ndi_recv);
		ndi_recv = nullptr;
	}
}

void ndi_input::ndi_video_thread()
{
	// Perf tokens are a mysterious thing. Seems to affect the whole process, so I think it's safe to only request from the video thread.
	perf_token = os_request_high_performance("NDI 5 Receiver Video Thread");

	NDIlib_video_frame_v2_t ndi_video_frame;
	obs_source_frame obs_video_frame = {};

	bool last_frame = false;

	while (running) {
		if (ndiLib->recv_get_no_connections(ndi_recv) == 0) {
			if (last_frame) { // Output null frame if playback ends
				obs_source_output_video(source, nullptr);
				last_frame = false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		if (ndiLib->NDIlib_recv_capture_v3(ndi_recv, &ndi_video_frame, nullptr, nullptr, 200) != NDIlib_frame_type_video)
			continue;

		last_frame = true;

		obs_video_frame.format = ndi_video_format_to_obs(ndi_video_frame.FourCC);
		obs_video_frame.timestamp = (uint64_t)(ndi_video_frame.timestamp * 100);
		obs_video_frame.width = ndi_video_frame.xres;
		obs_video_frame.height = ndi_video_frame.yres;
		obs_video_frame.linesize[0] = ndi_video_frame.line_stride_in_bytes;
		obs_video_frame.data[0] = ndi_video_frame.p_data;

		video_colorspace colorspace = resolution_to_obs_colorspace(ndi_video_frame.xres, ndi_video_frame.yres);
		video_format_get_parameters(colorspace, yuv_range, obs_video_frame.color_matrix, obs_video_frame.color_range_min, obs_video_frame.color_range_max);

		obs_source_output_video(source, &obs_video_frame);

		ndiLib->NDIlib_recv_free_video_v2(ndi_recv, &ndi_video_frame);
	}

	os_end_high_performance(perf_token);
	perf_token = nullptr;
}

void ndi_input::ndi_audio_thread()
{
	NDIlib_audio_frame_v3_t ndi_audio_frame;
	obs_source_audio obs_audio_frame = {};

	while (running) {
		if (ndiLib->recv_get_no_connections(ndi_recv) == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		if (ndiLib->NDIlib_recv_capture_v3(ndi_recv, nullptr, &ndi_audio_frame, nullptr, 200) != NDIlib_frame_type_audio)
			continue;

		// Reports seem to suggest that NDI can provide audio without timestamps.
		if (!ndi_audio_frame.timestamp)
			do_log(LOG_WARNING, "[ndi_input::ndi_audio_thread] Missing timestamp from NDI!");

		size_t channel_count = std::min(8, ndi_audio_frame.no_channels);

		obs_audio_frame.speakers = ndi_audio_layout_to_obs(channel_count);
		obs_audio_frame.timestamp = (uint64_t)(ndi_audio_frame.timestamp * 100);
		obs_audio_frame.samples_per_sec = ndi_audio_frame.sample_rate;
		obs_audio_frame.format = AUDIO_FORMAT_FLOAT_PLANAR;
		obs_audio_frame.frames = ndi_audio_frame.no_samples;

		for (size_t i = 0; i < channel_count; ++i)
			obs_audio_frame.data[i] = (uint8_t*)(&ndi_audio_frame.p_data[i * ndi_audio_frame.no_samples]);

		obs_source_output_audio(source, &obs_audio_frame);

		ndiLib->NDIlib_recv_free_audio_v3(ndi_recv, &ndi_audio_frame);
	}
}

void register_ndi_source_info()
{
	struct obs_source_info info = {};
	info.id				= "ndi_input_v5";
	info.type			= OBS_SOURCE_TYPE_INPUT;
	info.output_flags	= OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name		= [](void *) { return obs_module_text("Input.Name"); };
	info.create			= [](obs_data_t *settings, obs_source_t *source) -> void * { return new ndi_input(settings, source); };
	info.destroy		= [](void *data) { delete static_cast<ndi_input *>(data); };
	info.get_defaults	= ndi_input::defaults;
	info.get_properties	= [](void *data) -> obs_properties_t * { return static_cast<ndi_input *>(data)->properties(); };
	info.update			= [](void *data, obs_data_t *settings) { static_cast<ndi_input *>(data)->update(settings); };
	info.activate		= [](void *data) { static_cast<ndi_input *>(data)->activate(); };
	info.deactivate		= [](void *data) { static_cast<ndi_input *>(data)->deactivate(); };
	info.show			= [](void *data) { static_cast<ndi_input *>(data)->show(); };
	info.hide			= [](void *data) { static_cast<ndi_input *>(data)->hide(); };

	obs_register_source(&info);
}
