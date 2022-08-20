#include <chrono>
#include <obs-module.h>

#include "obs-ndi.h"
#include "output.h"

ndi_output::ndi_output(obs_data_t *settings, obs_output_t *output)
	: output(output),
	  ndi_send(nullptr),
	  running(false),
	  last_video_frame(false),
	  obs_video_frame_a(nullptr),
	  obs_video_frame_b(nullptr),
	  audio_frame_buf_size(0),
	  send_name(""),
	  enable_video(false),
	  enable_audio(false)
{
	update(settings);

	blog(LOG_DEBUG, "[ndi_output::ndi_output] NDI 5 Output created.");
}

ndi_output::~ndi_output()
{
	blog(LOG_DEBUG, "[ndi_output::~ndi_output] NDI 5 output destroyed.");
}

bool ndi_output::start()
{
	if (!enable_video && !enable_audio)
		return false;

	uint32_t flags = 0;
	video_t *video = obs_output_video(output);
	audio_t *audio = obs_output_audio(output);

	if (video && enable_video) {
		NDIlib_video_frame_v2_t video_frame = {};

		// TODO: Add more supported video formats
		v_format = video_output_get_format(video);
		switch (v_format) {
			case VIDEO_FORMAT_NV12:
				video_frame.FourCC = NDIlib_FourCC_video_type_NV12;
				break;
			case VIDEO_FORMAT_I420:
				video_frame.FourCC = NDIlib_FourCC_video_type_I420;
				break;
			case VIDEO_FORMAT_RGBA:
				video_frame.FourCC = NDIlib_FourCC_video_type_RGBA;
				break;
			case VIDEO_FORMAT_BGRA:
				video_frame.FourCC = NDIlib_FourCC_video_type_BGRA;
				break;
			case VIDEO_FORMAT_BGRX:
				video_frame.FourCC = NDIlib_FourCC_video_type_BGRX;
				break;
			default:
				blog(LOG_WARNING, "Unsupported pixel format: %d", v_format);
				return false;
		}

		auto info = video_output_get_info(video);
		if (!info)
			return false;

		video_frame.xres = info->width;
		video_frame.yres = info->height;
		video_frame.frame_rate_N = info->fps_num;
		video_frame.frame_rate_D = info->fps_den;
		video_frame.frame_format_type = NDIlib_frame_format_type_progressive;

		video_frame_a = std::make_unique<NDIlib_video_frame_v2_t>(video_frame);
		video_frame_b = std::make_unique<NDIlib_video_frame_v2_t>(video_frame);

		// Create OBS video frames. These are the real buffer, the NDI frames are an overlay
		obs_video_frame_a = video_frame_create(v_format, info->width, info->height);
		obs_video_frame_b = video_frame_create(v_format, info->width, info->height);

		// Assign the OBS video frame buffers to the NDI frames
		video_frame_a->p_data = obs_video_frame_a->data[0];
		video_frame_a->line_stride_in_bytes = obs_video_frame_a->linesize[0];
		video_frame_b->p_data = obs_video_frame_b->data[0];
		video_frame_b->line_stride_in_bytes = obs_video_frame_b->linesize[0];

		flags |= OBS_OUTPUT_VIDEO;
	}

	if (audio && enable_audio) {
		audio_frame = std::make_unique<NDIlib_audio_frame_v3_t>();
		audio_frame->sample_rate = audio_output_get_sample_rate(audio);
		audio_frame->FourCC = NDIlib_FourCC_audio_type_FLTP;
		audio_frame->no_channels = (int)audio_output_get_channels(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}

	if (!flags) {
		blog(LOG_ERROR, "Failed to start output, no video or audio could be initialized.");
		return false;
	}

	NDIlib_send_create_t desc;
	desc.p_ndi_name = send_name.c_str();
	desc.p_groups = nullptr;
	desc.clock_video = false;
	desc.clock_audio = false;

	ndi_send = ndiLib->send_create(&desc);
	if (!ndi_send) {
		blog(LOG_ERROR, "Failed to create NDI sender.");
		return false;
	}

	running = obs_output_begin_data_capture(output, flags);
	if (!running) {
		blog(LOG_ERROR, "Failed to begin data capture.");
		return false;
	}

	return running;
}

// Todo
void ndi_output::stop(uint64_t)
{
	running = false;

	obs_output_end_data_capture(output);

	if (ndi_send) {
		ndiLib->send_destroy(ndi_send);
		ndi_send = nullptr;
	}

	video_frame_destroy(obs_video_frame_a);
	obs_video_frame_a = nullptr;
	video_frame_destroy(obs_video_frame_b);
	obs_video_frame_b = nullptr;

	if (audio_frame && audio_frame->p_data) {
		bfree(audio_frame->p_data);
		audio_frame->p_data = nullptr;
		audio_frame_buf_size = 0;
	}
}

void ndi_output::raw_video(struct video_data *frame)
{
	if (!running || !video_frame_a)
		return;

	// Get A or B buffer frame
	NDIlib_video_frame_v2_t *video_frame = (last_video_frame) ? video_frame_a.get() : video_frame_b.get();
	struct video_frame *obs_video_frame = (last_video_frame) ? obs_video_frame_a : obs_video_frame_b;

	video_frame->timecode = frame->timestamp / 100;

	// This one is true evil.
	struct video_frame *in_frame = reinterpret_cast<struct video_frame*>(frame);

	// Copy data to the OBS buffer that sits underneath the NDI frame
	video_frame_copy(obs_video_frame, in_frame, v_format, video_frame->yres);

	// Swap buffer for next frame
	last_video_frame = !last_video_frame;

	// Send async - offloads encode to internal NDI worker thread
	ndiLib->send_send_video_async_v2(ndi_send, video_frame);
}

void ndi_output::raw_audio(struct audio_data *frame)
{
	if (!running || !audio_frame)
		return;

	// Set frame metadata
	audio_frame->no_samples = frame->frames;
	audio_frame->timecode = frame->timestamp / 100;
	audio_frame->channel_stride_in_bytes = frame->frames * 4;

	// Ensure audio buffer capacity
	const size_t required_buf_size = audio_frame->no_channels * audio_frame->channel_stride_in_bytes;
	if (required_buf_size > audio_frame_buf_size) {
		if (audio_frame->p_data)
			bfree(audio_frame->p_data);
		audio_frame->p_data = (uint8_t *)bmalloc(required_buf_size);
		audio_frame_buf_size = required_buf_size;
	}

	// Copy audio data to buffer
	for (int i = 0; i < audio_frame->no_channels; ++i)
		memcpy(audio_frame->p_data + (i * audio_frame->channel_stride_in_bytes), frame->data[i], audio_frame->channel_stride_in_bytes);

	// Send audio synchronously, to avoid more buffer complication
	ndiLib->send_send_audio_v3(ndi_send, audio_frame.get());
}

void ndi_output::update(obs_data_t *settings)
{
	send_name = obs_data_get_string(settings, P_OUTPUT_SOURCE_NAME);
	enable_video = obs_data_get_bool(settings, P_ENABLE_VIDEO);
	enable_audio = obs_data_get_bool(settings, P_ENABLE_AUDIO);
}

void ndi_output::defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, P_OUTPUT_SOURCE_NAME, T_OUTPUT_SOURCE_NAME_DEFAULT);
	obs_data_set_default_bool(settings, P_ENABLE_VIDEO, true);
	obs_data_set_default_bool(settings, P_ENABLE_AUDIO, true);
}

obs_properties_t *ndi_output::properties()
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_properties_add_text(props, P_OUTPUT_SOURCE_NAME, T_OUTPUT_SOURCE_NAME, OBS_TEXT_DEFAULT);

	return props;
}

void register_ndi_output_info()
{
	struct obs_output_info info = {};
	info.id =				"ndi_output_v5";
	info.flags =			OBS_OUTPUT_AV;
	info.get_name =			[](void *) { return obs_module_text("Output.Name"); };
	info.create =			[](obs_data_t *settings, obs_output_t *output) -> void * { return new ndi_output(settings, output); };
	info.destroy =			[](void *data) { delete static_cast<ndi_output *>(data); };
	info.start =			[](void *data) -> bool { return static_cast<ndi_output *>(data)->start(); };
	info.stop =				[](void *data, uint64_t ts) { static_cast<ndi_output *>(data)->stop(ts); };
	info.raw_video =		[](void *data, struct video_data *frame) { static_cast<ndi_output *>(data)->raw_video(frame); };
	info.raw_audio =		[](void *data, struct audio_data *frames) { static_cast<ndi_output *>(data)->raw_audio(frames); };
	info.update =			[](void *data, obs_data_t *settings) { static_cast<ndi_output *>(data)->update(settings); };
	info.get_defaults =		ndi_output::defaults;
	info.get_properties =	[](void *data) -> obs_properties_t * { return static_cast<ndi_output *>(data)->properties(); };

	obs_register_output(&info);
}
