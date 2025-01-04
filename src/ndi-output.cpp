/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#include "plugin-main.h"

static FORCE_INLINE uint32_t min_uint32(uint32_t a, uint32_t b)
{
	return a < b ? a : b;
}

typedef void (*uyvy_conv_function)(uint8_t *input[], uint32_t in_linesize[],
				   uint32_t start_y, uint32_t end_y,
				   uint8_t *output, uint32_t out_linesize);

static void convert_i444_to_uyvy(uint8_t *input[], uint32_t in_linesize[],
				 uint32_t start_y, uint32_t end_y,
				 uint8_t *output, uint32_t out_linesize)
{
	uint8_t *_Y;
	uint8_t *_U;
	uint8_t *_V;
	uint8_t *_out;
	uint32_t width = min_uint32(in_linesize[0], out_linesize);
	for (uint32_t y = start_y; y < end_y; ++y) {
		_Y = input[0] + ((size_t)y * (size_t)in_linesize[0]);
		_U = input[1] + ((size_t)y * (size_t)in_linesize[1]);
		_V = input[2] + ((size_t)y * (size_t)in_linesize[2]);

		_out = output + ((size_t)y * (size_t)out_linesize);

		for (uint32_t x = 0; x < width; x += 2) {
			// Quality loss here. Some chroma samples are ignored.
			*(_out++) = *(_U++);
			_U++;
			*(_out++) = *(_Y++);
			*(_out++) = *(_V++);
			_V++;
			*(_out++) = *(_Y++);
		}
	}
}

typedef struct {
	obs_output_t *output;
	const char *ndi_name;
	const char *ndi_groups;
	bool uses_video;
	bool uses_audio;

	bool started;

	NDIlib_send_instance_t ndi_sender;

	uint32_t frame_width;
	uint32_t frame_height;
	NDIlib_FourCC_video_type_e frame_fourcc;
	double video_framerate;

	size_t audio_channels;
	uint32_t audio_samplerate;

	uint8_t *conv_buffer;
	uint32_t conv_linesize;
	uyvy_conv_function conv_function;

	uint8_t *audio_conv_buffer;
	size_t audio_conv_buffer_size;
} ndi_output_t;

const char *ndi_output_getname(void *)
{
	return obs_module_text("NDIPlugin.OutputName");
}

obs_properties_t *ndi_output_getproperties(void *)
{
	obs_log(LOG_INFO, "+ndi_output_getproperties()");

	obs_properties_t *props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_properties_add_text(
		props, "ndi_name",
		obs_module_text("NDIPlugin.OutputProps.NDIName"),
		OBS_TEXT_DEFAULT);
	obs_properties_add_text(
		props, "ndi_groups",
		obs_module_text("NDIPlugin.OutputProps.NDIGroups"),
		OBS_TEXT_DEFAULT);

	obs_log(LOG_INFO, "-ndi_output_getproperties()");

	return props;
}

void ndi_output_getdefaults(obs_data_t *settings)
{
	obs_log(LOG_INFO, "+ndi_output_getdefaults()");
	obs_data_set_default_string(settings, "ndi_name",
				    "DistroAV output (changeme)");
	obs_data_set_default_string(settings, "ndi_groups",
				    "DistroAV output (changeme)");
	obs_data_set_default_bool(settings, "uses_video", true);
	obs_data_set_default_bool(settings, "uses_audio", true);
	obs_log(LOG_INFO, "-ndi_output_getdefaults()");
}

void ndi_output_update(void *data, obs_data_t *settings);

void *ndi_output_create(obs_data_t *settings, obs_output_t *output)
{
	auto name = obs_data_get_string(settings, "ndi_name");
	auto groups = obs_data_get_string(settings, "ndi_groups");
	obs_log(LOG_INFO, "+ndi_output_create(name='%s', groups='%s', ...)",
		name, groups);
	auto o = (ndi_output_t *)bzalloc(sizeof(ndi_output_t));
	o->output = output;
	ndi_output_update(o, settings);

	obs_log(LOG_INFO, "-ndi_output_create(name='%s', groups='%s', ...)",
		name, groups);
	return o;
}

bool ndi_output_start(void *data)
{
	auto o = (ndi_output_t *)data;
	auto name = o->ndi_name;
	auto groups = o->ndi_groups;
	obs_log(LOG_INFO, "+ndi_output_start(name='%s', groups='%s', ...)",
		name, groups);
	if (o->started) {
		obs_log(LOG_INFO,
			"-ndi_output_start(name='%s', groups='%s', ...)", name,
			groups);
		return false;
	}

	uint32_t flags = 0;
	video_t *video = obs_output_video(o->output);
	audio_t *audio = obs_output_audio(o->output);

	if (!video && !audio) {
		obs_log(LOG_ERROR,
			"'%s' ndi_output_start: no video and audio available",
			name);
		obs_log(LOG_INFO,
			"-ndi_output_start(name='%s', groups='%s', ...)", name,
			groups);
		return false;
	}

	if (o->uses_video && video) {
		video_format format = video_output_get_format(video);
		uint32_t width = video_output_get_width(video);
		uint32_t height = video_output_get_height(video);

		switch (format) {
		case VIDEO_FORMAT_I444:
			o->conv_function = convert_i444_to_uyvy;
			o->frame_fourcc = NDIlib_FourCC_video_type_UYVY;
			o->conv_linesize = width * 2;
			o->conv_buffer =
				new uint8_t[(size_t)height *
					    (size_t)o->conv_linesize * 2]();
			break;

		case VIDEO_FORMAT_NV12:
			o->frame_fourcc = NDIlib_FourCC_video_type_NV12;
			break;

		case VIDEO_FORMAT_I420:
			o->frame_fourcc = NDIlib_FourCC_video_type_I420;
			break;

		case VIDEO_FORMAT_RGBA:
			o->frame_fourcc = NDIlib_FourCC_video_type_RGBA;
			break;

		case VIDEO_FORMAT_BGRA:
			o->frame_fourcc = NDIlib_FourCC_video_type_BGRA;
			break;

		case VIDEO_FORMAT_BGRX:
			o->frame_fourcc = NDIlib_FourCC_video_type_BGRX;
			break;

		default:
			obs_log(LOG_WARNING,
				"warning: unsupported pixel format %d", format);
			obs_log(LOG_INFO,
				"-ndi_output_start(name='%s', groups='%s', ...)",
				name, groups);
			return false;
		}

		o->frame_width = width;
		o->frame_height = height;
		o->video_framerate = video_output_get_frame_rate(video);
		flags |= OBS_OUTPUT_VIDEO;
	}

	if (o->uses_audio && audio) {
		o->audio_samplerate = audio_output_get_sample_rate(audio);
		o->audio_channels = audio_output_get_channels(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}

	NDIlib_send_create_t send_desc;
	send_desc.p_ndi_name = name;
	if (groups && groups[0])
		send_desc.p_groups = groups;
	else
		send_desc.p_groups = nullptr;
	send_desc.clock_video = false;
	send_desc.clock_audio = false;

	o->ndi_sender = ndiLib->send_create(&send_desc);
	if (o->ndi_sender) {
		o->started = obs_output_begin_data_capture(o->output, flags);
		if (o->started) {
			obs_log(LOG_INFO,
				"'%s' ndi_output_start: ndi output started",
				name);
		} else {
			obs_log(LOG_ERROR,
				"'%s' ndi_output_start: data capture start failed",
				name);
		}
	} else {
		obs_log(LOG_ERROR,
			"'%s' ndi_output_start: ndi sender init failed", name);
	}

	obs_log(LOG_INFO, "-ndi_output_start(name='%s', groups='%s'...)", name,
		groups);

	return o->started;
}

void ndi_output_update(void *data, obs_data_t *settings)
{
	auto o = (ndi_output_t *)data;
	auto name = obs_data_get_string(settings, "ndi_name");
	auto groups = obs_data_get_string(settings, "ndi_groups");
	obs_log(LOG_INFO, "ndi_output_update(name='%s', groups='%s', ...)",
		name, groups);
	o->ndi_name = name;
	o->ndi_groups = groups;
	o->uses_video = obs_data_get_bool(settings, "uses_video");
	o->uses_audio = obs_data_get_bool(settings, "uses_audio");
}

void ndi_output_stop(void *data, uint64_t)
{
	auto o = (ndi_output_t *)data;
	auto name = o->ndi_name;
	auto groups = o->ndi_groups;
	obs_log(LOG_INFO, "+ndi_output_stop(name='%s', groups='%s', ...)", name,
		groups);
	if (o->started) {
		o->started = false;

		obs_output_end_data_capture(o->output);

		if (o->ndi_sender) {
			obs_log(LOG_DEBUG,
				"ndi_output_stop: +ndiLib->send_destroy(o->ndi_sender)");
			ndiLib->send_destroy(o->ndi_sender);
			obs_log(LOG_DEBUG,
				"ndi_output_stop: -ndiLib->send_destroy(o->ndi_sender)");
			o->ndi_sender = nullptr;
		}

		if (o->conv_buffer) {
			delete o->conv_buffer;
			o->conv_buffer = nullptr;
			o->conv_function = nullptr;
		}

		o->frame_width = 0;
		o->frame_height = 0;
		o->video_framerate = 0.0;

		o->audio_channels = 0;
		o->audio_samplerate = 0;
	}

	obs_log(LOG_INFO, "-ndi_output_stop(name='%s', groups='%s', ...)", name,
		groups);
}

void ndi_output_destroy(void *data)
{
	auto o = (ndi_output_t *)data;
	auto name = o->ndi_name;
	auto groups = o->ndi_groups;
	obs_log(LOG_INFO, "+ndi_output_destroy(name='%s', groups='%s', ...)",
		name, groups);

	if (o->audio_conv_buffer) {
		obs_log(LOG_INFO, "ndi_output_destroy: freeing %zu bytes",
			o->audio_conv_buffer_size);
		bfree(o->audio_conv_buffer);
		o->audio_conv_buffer = nullptr;
	}
	obs_log(LOG_INFO, "-ndi_output_destroy(name='%s', groups='%s', ...)",
		name, groups);
	bfree(o);
}

void ndi_output_rawvideo(void *data, video_data *frame)
{
	auto o = (ndi_output_t *)data;
	if (!o->started || !o->frame_width || !o->frame_height)
		return;

	uint32_t width = o->frame_width;
	uint32_t height = o->frame_height;

	NDIlib_video_frame_v2_t video_frame = {0};
	video_frame.xres = width;
	video_frame.yres = height;
	video_frame.frame_rate_N = (int)(o->video_framerate * 100);
	// TODO fixme: broken on fractional framerates
	video_frame.frame_rate_D = 100;
	video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
	video_frame.timecode = NDIlib_send_timecode_synthesize;
	video_frame.FourCC = o->frame_fourcc;

	if (video_frame.FourCC == NDIlib_FourCC_type_UYVY) {
		o->conv_function(frame->data, frame->linesize, 0, height,
				 o->conv_buffer, o->conv_linesize);
		video_frame.p_data = o->conv_buffer;
		video_frame.line_stride_in_bytes = o->conv_linesize;
	} else {
		video_frame.p_data = frame->data[0];
		video_frame.line_stride_in_bytes = frame->linesize[0];
	}

	ndiLib->send_send_video_async_v2(o->ndi_sender, &video_frame);
}

void ndi_output_rawaudio(void *data, audio_data *frame)
{
	// NOTE: The logic in this function should be similar to
	// ndi-filter.cpp/ndi_filter_asyncaudio(...)
	auto o = (ndi_output_t *)data;
	if (!o->started || !o->audio_samplerate || !o->audio_channels)
		return;

	NDIlib_audio_frame_v3_t audio_frame = {0};
	audio_frame.sample_rate = o->audio_samplerate;
	audio_frame.no_channels = (int)o->audio_channels;
	audio_frame.timecode = NDIlib_send_timecode_synthesize;
	audio_frame.no_samples = frame->frames;
	audio_frame.channel_stride_in_bytes = frame->frames * 4;
	audio_frame.FourCC = NDIlib_FourCC_audio_type_FLTP;

	const size_t data_size =
		audio_frame.no_channels * audio_frame.channel_stride_in_bytes;

	if (data_size > o->audio_conv_buffer_size) {
		obs_log(LOG_INFO,
			"ndi_output_rawaudio(`%s`): growing audio_conv_buffer from %zu to %zu bytes",
			o->ndi_name, o->audio_conv_buffer_size, data_size);
		if (o->audio_conv_buffer) {
			obs_log(LOG_INFO,
				"ndi_output_rawaudio(`%s`): freeing %zu bytes",
				o->ndi_name, o->audio_conv_buffer_size);
			bfree(o->audio_conv_buffer);
		}
		obs_log(LOG_INFO,
			"ndi_output_rawaudio(`%s`): allocating %zu bytes",
			o->ndi_name, data_size);
		o->audio_conv_buffer = (uint8_t *)bmalloc(data_size);
		o->audio_conv_buffer_size = data_size;
	}

	for (int i = 0; i < audio_frame.no_channels; ++i) {
		memcpy(o->audio_conv_buffer +
			       (i * audio_frame.channel_stride_in_bytes),
		       frame->data[i], audio_frame.channel_stride_in_bytes);
	}

	audio_frame.p_data = o->audio_conv_buffer;

	ndiLib->send_send_audio_v3(o->ndi_sender, &audio_frame);
}

obs_output_info create_ndi_output_info()
{
	obs_output_info ndi_output_info = {};
	ndi_output_info.id = "ndi_output";
	ndi_output_info.flags = OBS_OUTPUT_AV;

	ndi_output_info.get_name = ndi_output_getname;
	ndi_output_info.get_properties = ndi_output_getproperties;
	ndi_output_info.get_defaults = ndi_output_getdefaults;

	ndi_output_info.create = ndi_output_create;
	ndi_output_info.start = ndi_output_start;
	ndi_output_info.update = ndi_output_update;
	ndi_output_info.stop = ndi_output_stop;
	ndi_output_info.destroy = ndi_output_destroy;

	ndi_output_info.raw_video = ndi_output_rawvideo;
	ndi_output_info.raw_audio = ndi_output_rawaudio;

	return ndi_output_info;
}
