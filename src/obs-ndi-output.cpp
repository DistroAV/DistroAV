#include <chrono>
#include <obs-module.h>

#include "obs-ndi.h"
#include "obs-ndi-output.h"

static void copy_video_data(struct video_data *src, struct video_data *dst, size_t size)
{
	if (!src->data[0])
		return;

	dst->data[0] = (uint8_t *)bmemdup(src->data[0], size);
}

static void free_video_data(struct video_data *frame)
{
	if (frame->data[0]) {
		bfree(frame->data[0]);
		frame->data[0] = NULL;
	}

	memset(frame, 0, sizeof(*frame));
}

static void copy_audio_data(struct audio_data *src, struct audio_data *dst, size_t size)
{
	if (!src->data[0])
		return;

	dst->data[0] = (uint8_t *)bmemdup(src->data[0], size);
}

static void free_audio_data(struct audio_data *frames)
{
	if (frames->data[0]) {
		bfree(frames->data[0]);
		frames->data[0] = NULL;
	}
	memset(frames, 0, sizeof(*frames));
}

ndi_output::ndi_output(obs_data_t *settings, obs_output_t *output)
	: output{output},
	  ndi_send{nullptr},
	  running{false},
	  video_frame_width{0},
	  video_frame_height{0},
	  video_framerate_num{0},
	  video_framerate_den{0},
	  audio_channels{0},
	  audio_samplerate{0}
{
	video_queue = std::make_unique<video_queue_t>();
	audio_queue = std::make_unique<audio_queue_t>();

	update(settings);

	blog(LOG_INFO, "NDI 5 Output created!");
}

ndi_output::~ndi_output()
{
	if (video_queue) {
		clear_video_queue();
		video_queue.reset();
	}

	if (audio_queue) {
		clear_audio_queue();
		audio_queue.reset();
	}
}

// Todo
bool ndi_output::start()
{
	if (!enable_video && !enable_audio)
		return false;

	uint32_t flags = 0;
	video_t *video = obs_output_video(output);
	audio_t *audio = obs_output_audio(output);

	if (!video && !audio)
		return false;

	if (video && enable_video) {
		video_format format = video_output_get_format(video);
		switch (format) {
		case VIDEO_FORMAT_NV12:
			video_frame_fourcc = NDIlib_FourCC_video_type_NV12;
			break;
		case VIDEO_FORMAT_I420:
			video_frame_fourcc = NDIlib_FourCC_video_type_I420;
			break;
		case VIDEO_FORMAT_RGBA:
			video_frame_fourcc = NDIlib_FourCC_video_type_RGBA;
			break;
		case VIDEO_FORMAT_BGRA:
			video_frame_fourcc = NDIlib_FourCC_video_type_BGRA;
			break;
		case VIDEO_FORMAT_BGRX:
			video_frame_fourcc = NDIlib_FourCC_video_type_BGRX;
			break;
		default:
			blog(LOG_WARNING, "Unsupported pixel format: %d", format);
			return false;
		}

		video_frame_width = video_output_get_width(video);
		video_frame_height = video_output_get_height(video);

		auto info = video_output_get_info(video);
		if (!info)
			return false;

		video_framerate_num = info->fps_num;
		video_framerate_den = info->fps_den;

		flags |= OBS_OUTPUT_VIDEO;
	}

	if (audio && enable_audio) {
		audio_channels = audio_output_get_channels(audio);
		audio_samplerate = audio_output_get_sample_rate(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}

	NDIlib_send_create_t desc;
	desc.p_ndi_name = send_name.c_str();
	desc.p_groups = nullptr;
	desc.clock_video = true; // Tell NDI to block the send call until a new frame should be delivered
	desc.clock_audio = false;

	ndi_send = ndiLib->send_create(&desc);
	if (!ndi_send)
		return false;

	running = obs_output_begin_data_capture(output, flags);
	if (!running)
		return false;

	ndi_send_thread = std::thread(&ndi_output::ndi_send_thread_work, this);

	return running;
}

// Todo
void ndi_output::stop(uint64_t)
{
	running = false;

	obs_output_end_data_capture(output);

	if (ndi_send_thread.joinable())
		ndi_send_thread.join();

	if (ndi_send) {
		ndiLib->send_destroy(ndi_send);
		ndi_send = nullptr;
	}
}

void ndi_output::raw_video(struct video_data *frame)
{
	std::lock_guard<std::mutex> lock(video_mutex);

	struct video_data new_frame = *frame;

	if (video_queue->size() > VIDEO_QUEUE_MAX_SIZE) {
		auto &f = video_queue->front();
		free_video_data(&f);
		video_queue->pop_front();
	}

	copy_video_data(frame, &new_frame, (size_t)frame->linesize[0] * (size_t)video_frame_height);

	video_queue->push_back(new_frame);
}

void ndi_output::raw_audio(struct audio_data *frames)
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	struct audio_data new_frames = *frames;

	if (audio_queue->size() > AUDIO_QUEUE_MAX_SIZE) {
		auto &f = audio_queue->front();
		free_audio_data(&f);
		audio_queue->pop_front();
	}

	copy_audio_data(frames, &new_frames, frames->frames * audio_channels * sizeof(float));

	audio_queue->push_back(new_frames);
}

void ndi_output::update(obs_data_t *settings)
{
	send_name = obs_data_get_string(settings, P_OUTPUT_SOURCE_NAME);
	//enable_video = obs_data_get_bool(settings, P_ENABLE_VIDEO);
	enable_video = true;
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

void ndi_output::clear_video_queue()
{
	std::lock_guard<std::mutex> lock(video_mutex);
	while (video_queue->size()) {
		auto &f = video_queue->front();
		free_video_data(&f);
		video_queue->pop_front();
	}
}

void ndi_output::clear_audio_queue()
{
	std::lock_guard<std::mutex> lock(audio_mutex);
	while (audio_queue->size()) {
		auto &f = audio_queue->front();
		free_audio_data(&f);
		audio_queue->pop_front();
	}
}

// Todo
void ndi_output::ndi_send_thread_work()
{
	NDIlib_video_frame_v2_t ndi_video_frame;
	ndi_video_frame.FourCC = video_frame_fourcc;
	ndi_video_frame.xres = video_frame_width;
	ndi_video_frame.yres = video_frame_height;
	ndi_video_frame.frame_rate_N = video_framerate_num;
	ndi_video_frame.frame_rate_D = video_framerate_den;
	ndi_video_frame.frame_format_type = NDIlib_frame_format_type_progressive;

	NDIlib_audio_frame_v3_t ndi_audio_frame;
	ndi_audio_frame.no_channels = audio_channels;
	ndi_audio_frame.sample_rate = audio_samplerate;
	ndi_audio_frame.FourCC = NDIlib_FourCC_audio_type_FLTP;

	while (running) {
		std::lock_guard<std::mutex> lock(video_mutex);
		if (video_queue->size() > 1)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	while (running) {
		{
			std::lock_guard<std::mutex> lock(video_mutex);
			if (video_queue->size()) {
				if (ndi_video_frame.p_data) {
					bfree(ndi_video_frame.p_data);
					ndi_video_frame.p_data = NULL;
				}

				auto &f = video_queue->front();

				ndi_video_frame.p_data = f.data[0];
				ndi_video_frame.line_stride_in_bytes = f.linesize[0];

				video_queue->pop_front();
			}
		}

		ndiLib->send_send_video_v2(ndi_send, &ndi_video_frame);
	}

	if (ndi_video_frame.p_data)
		bfree(ndi_video_frame.p_data);

	if (ndi_audio_frame.p_data)
		bfree(ndi_audio_frame.p_data);
}

void register_ndi_output_info()
{
	struct obs_output_info info = {};
	info.id = "ndi_output_v5";
	info.flags = OBS_OUTPUT_AV;
	info.get_name = [](void *) { return obs_module_text("Output.Name"); };
	info.create = [](obs_data_t *settings, obs_output_t *output) -> void * { return new ndi_output(settings, output); };
	info.destroy = [](void *data) { delete static_cast<ndi_output *>(data); };
	info.start = [](void *data) -> bool { return static_cast<ndi_output *>(data)->start(); };
	info.stop = [](void *data, uint64_t ts) { static_cast<ndi_output *>(data)->stop(ts); };
	info.raw_video = [](void *data, struct video_data *frame) { static_cast<ndi_output *>(data)->raw_video(frame); };
	info.raw_audio = [](void *data, struct audio_data *frames) { static_cast<ndi_output *>(data)->raw_audio(frames); };
	info.update = [](void *data, obs_data_t *settings) { static_cast<ndi_output *>(data)->update(settings); };
	info.get_defaults = ndi_output::defaults;
	info.get_properties = [](void *data) -> obs_properties_t * { return static_cast<ndi_output *>(data)->properties(); };

	obs_register_output(&info);
}
