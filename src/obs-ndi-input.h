#pragma once

#include <atomic>
#include <string>
#include <util/platform.h>

struct ndi_input
{
	obs_source_t* source;

	NDIlib_recv_instance_t ndi_receiver;
	int sync_mode;
	video_range_type yuv_range;
	video_colorspace yuv_colorspace;
	pthread_t av_thread;
	std::atomic<bool> running;

	NDIlib_tally_t tally;
	bool alpha_filter_enabled;
	bool audio_enabled;
	os_performance_token_t* perf_token;

	ndi_input(obs_data_t *settings, obs_source_t *source);
	~ndi_input();

	static void defaults(obs_data_t *settings);
	obs_properties_t *properties();
	void update(obs_data_t *settings);

	void activate();
	void deactivate();
	void show();
	void hide();
};

void register_ndi_source_info();
