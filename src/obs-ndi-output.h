#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <deque>
#include <thread>
#include <atomic>
#include <util/platform.h>

#include "obs-ndi.h"

#define P_OUTPUT_SOURCE_NAME      "source_name"
#define P_ENABLE_VIDEO            "enable_video"
#define P_ENABLE_AUDIO            "enable_audio"

#define T_OUTPUT_SOURCE_NAME          T_("Output.Properties.SourceName")
#define T_OUTPUT_SOURCE_NAME_DEFAULT  T_("Output.Properties.SourceName.Default")

#define VIDEO_QUEUE_MAX_SIZE 15
#define AUDIO_QUEUE_MAX_SIZE 96

typedef std::deque<struct video_data> video_queue_t;
typedef std::deque<struct audio_data> audio_queue_t;

struct ndi_output
{
	obs_output_t* output;

	NDIlib_send_instance_t ndi_send;
	std::thread ndi_send_thread;

	std::atomic<bool> running;

	std::mutex video_mutex;
	std::unique_ptr<video_queue_t> video_queue;
	std::mutex audio_mutex;
	std::unique_ptr<audio_queue_t> audio_queue;

	NDIlib_FourCC_video_type_e video_frame_fourcc;
	uint32_t video_frame_width;
	uint32_t video_frame_height;
	uint32_t video_framerate_num;
	uint32_t video_framerate_den;

	size_t audio_channels;
	uint32_t audio_samplerate;

	std::string send_name;
	bool enable_video;
	bool enable_audio;

	ndi_output(obs_data_t *settings, obs_output_t *output);
	~ndi_output();

	bool start();
	void stop(uint64_t ts);

	void raw_video(struct video_data *frame);
	void raw_audio(struct audio_data *frames);

	void update(obs_data_t *settings);
	static void defaults(obs_data_t *settings);
	obs_properties_t *properties();

	void clear_video_queue();
	void clear_audio_queue();

	void ndi_send_thread_work();
};

static void register_ndi_output_info();
