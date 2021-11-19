#pragma once

#include <string>
#include <atomic>
#include <util/platform.h>

#define P_OUTPUT_SOURCE_NAME      "source_name"
#define P_ENABLE_VIDEO            "enable_video"
#define P_ENABLE_AUDIO            "enable_audio"

#define T_OUTPUT_SOURCE_NAME          T_("Output.Properties.SourceName")
#define T_OUTPUT_SOURCE_NAME_DEFAULT  T_("Output.Properties.SourceName.Default")

struct ndi_output
{
	obs_output_t* output;

	NDIlib_send_instance_t ndi_send;
	std::atomic<bool> running;

	struct video_data *current_frame;

	uint32_t frame_width;
	uint32_t frame_height;
	NDIlib_FourCC_video_type_e frame_fourcc;
	double video_framerate;

	size_t audio_channels;
	uint32_t audio_samplerate;

	std::string source_name;
	std::atomic<bool> enable_video;
	std::atomic<bool> enable_audio;

	ndi_output(obs_data_t *settings, obs_output_t *output);
	~ndi_output();

	bool start();
	void stop(uint64_t ts);

	void raw_video(struct video_data *frame);
	void raw_audio(struct audio_data *frames);

	void update(obs_data_t *settings);
	static void defaults(obs_data_t *settings);
	obs_properties_t *properties();
};

void register_ndi_output_info();
