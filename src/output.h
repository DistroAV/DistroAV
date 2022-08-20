#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <deque>
#include <thread>
#include <atomic>

extern "C" {
#include <media-io/video-frame.h>
}
#include <util/platform.h>

#include "obs-ndi.h"

#define P_OUTPUT_SOURCE_NAME "source_name"
#define P_ENABLE_VIDEO "enable_video"
#define P_ENABLE_AUDIO "enable_audio"

#define T_OUTPUT_SOURCE_NAME T_("Output.Properties.SourceName")
#define T_OUTPUT_SOURCE_NAME_DEFAULT T_("Output.Properties.SourceName.Default")

struct ndi_output {
	obs_output_t *output;

	NDIlib_send_instance_t ndi_send;

	std::atomic<bool> running;

	bool last_video_frame;
	enum video_format v_format;
	struct video_frame *obs_video_frame_a;
	std::unique_ptr<NDIlib_video_frame_v2_t> video_frame_a;
	struct video_frame *obs_video_frame_b;
	std::unique_ptr<NDIlib_video_frame_v2_t> video_frame_b;

	std::unique_ptr<NDIlib_audio_frame_v3_t> audio_frame;
	size_t audio_frame_buf_size;

	std::string send_name;
	bool enable_video;
	bool enable_audio;

	ndi_output(obs_data_t *settings, obs_output_t *output);
	~ndi_output();

	bool start();
	void stop(uint64_t ts);

	void raw_video(struct video_data *frame);
	void raw_audio(struct audio_data *frame);

	void update(obs_data_t *settings);
	static void defaults(obs_data_t *settings);
	obs_properties_t *properties();
};

void register_ndi_output_info();
