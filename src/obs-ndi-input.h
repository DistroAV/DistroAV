#pragma once

#include <atomic>
#include <thread>
#include <string>
#include <util/platform.h>

#define P_SOURCE_NAME      "source_name"
#define P_REFRESH_SOURCES  "refresh_sources"
#define P_BANDWIDTH        "bandwidth"
#define P_INPUT_UNBUFFERED "input_unbuffered"
#define P_HARDWARE_ACCEL   "hardware_accel"
#define P_AUDIO            "audio"
#define P_COLOR_RANGE      "color_range"

#define T_SOURCE_NAME          T_("Input.Properties.SourceName")
#define T_REFRESH_SOURCES      T_("Input.Properties.RefreshSources")
#define T_BANDWIDTH            T_("Input.Properties.Bandwidth")
#define T_BANDWIDTH_HIGHEST    T_("Input.Properties.Bandwidth.Highest")
#define T_BANDWIDTH_LOWEST     T_("Input.Properties.Bandwidth.Lowest")
#define T_BANDWIDTH_AUDIO_ONLY T_("Input.Properties.Bandwidth.AudioOnly")
#define T_INPUT_UNBUFFERED     T_("Input.Properties.InputUnbuffered")
#define T_HARDWARE_ACCEL       T_("Input.Properties.EnableHardwareAccel")
#define T_AUDIO                T_("Input.Properties.EnableAudio")
#define T_COLOR_RANGE          T_("Input.Properties.ColorRange")
#define T_COLOR_RANGE_PARTIAL  T_("Input.Properties.ColorRange.Partial")
#define T_COLOR_RANGE_FULL     T_("Input.Properties.ColorRange.Full")

enum ndi_input_bandwidth {
	OBS_NDI_BANDWIDTH_HIGHEST = 0,
	OBS_NDI_BANDWIDTH_LOWEST = 1,
	OBS_NDI_BANDWIDTH_AUDIO_ONLY = 2,
	OBS_NDI_BANDWIDTH_METADATA_ONLY = 3,
};

enum ndi_input_color_range {
	OBS_NDI_COLOR_RANGE_PARTIAL = 0,
	OBS_NDI_COLOR_RANGE_FULL = 1,
};

struct ndi_input
{
	obs_source_t* source;

	NDIlib_recv_instance_t ndi_recv;

	std::thread video_thread;
	std::thread audio_thread;
	std::atomic<bool> running;
	os_performance_token_t* perf_token;

	NDIlib_tally_t tally;

	video_range_type yuv_range;

	ndi_input(obs_data_t *settings, obs_source_t *source);
	~ndi_input();

	static void defaults(obs_data_t *settings);
	obs_properties_t *properties();
	void update(obs_data_t *settings);

	void activate();
	void deactivate();
	void show();
	void hide();

	void stop_recv();
	void ndi_video_thread();
	void ndi_audio_thread();
};

enum NDIlib_recv_bandwidth_e input_bandwidth_to_ndi(enum ndi_input_bandwidth in);
enum video_range_type input_color_range_to_obs(enum ndi_input_color_range in);

enum video_format ndi_video_format_to_obs(enum NDIlib_FourCC_video_type_e in);
enum video_colorspace resolution_to_obs_colorspace(size_t x, size_t y);
enum speaker_layout ndi_audio_layout_to_obs(size_t channel_count);

void register_ndi_source_info();
