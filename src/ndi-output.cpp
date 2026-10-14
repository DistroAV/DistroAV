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
#include "sync-debug.h"
#include <util/threading.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// #include "plugin-support.h"

using uyvy_conv_function = void (*)(uint8_t *const input[], const uint32_t in_linesize[], uint32_t start_y,
				    uint32_t end_y, uint8_t *output, uint32_t out_linesize);

static void convert_i444_to_uyvy(uint8_t *const input[], const uint32_t in_linesize[], uint32_t start_y, uint32_t end_y,
				 uint8_t *output, uint32_t out_linesize)
{
	// UYVY is 2 bytes per pixel; using it for the width avoids reading/writing into row padding.
	const uint32_t pairs = out_linesize / 4;
	for (uint32_t y = start_y; y < end_y; ++y) {
		const uint8_t *__restrict Y = input[0] + ((size_t)y * (size_t)in_linesize[0]);
		const uint8_t *__restrict U = input[1] + ((size_t)y * (size_t)in_linesize[1]);
		const uint8_t *__restrict V = input[2] + ((size_t)y * (size_t)in_linesize[2]);
		uint8_t *__restrict out = output + ((size_t)y * (size_t)out_linesize);

		// Index form with no pointer carried between iterations so the compiler can vectorize it.
		// Quality loss here: every other chroma sample is dropped.
		for (uint32_t i = 0; i < pairs; ++i) {
			out[4 * i + 0] = U[2 * i];
			out[4 * i + 1] = Y[2 * i];
			out[4 * i + 2] = V[2 * i];
			out[4 * i + 3] = Y[2 * i + 1];
		}
	}
}

struct ndi_output_t {
	obs_output_t *output = nullptr;
	std::string ndi_name;
	std::string ndi_groups;
	bool uses_video = false;
	bool uses_audio = false;

	bool started = false;

	NDIlib_send_instance_t ndi_sender = nullptr;
	std::mutex ndi_sender_mutex;

	uint32_t frame_width = 0;
	uint32_t frame_height = 0;
	NDIlib_FourCC_video_type_e frame_fourcc = NDIlib_FourCC_video_type_UYVY;
	int frame_rate_n = 0;
	int frame_rate_d = 1;

	size_t audio_channels = 0;
	uint32_t audio_samplerate = 0;

	std::vector<uint8_t> conv_buffer;
	uint32_t conv_linesize = 0;
	uyvy_conv_function conv_function = nullptr;

	std::vector<uint8_t> audio_conv_buffer;
	int8_t audio_path = -1;
	ndi_connection_monitor connections;
};

const char *ndi_output_getname(void *)
{
	return obs_module_text("NDIPlugin.OutputName");
}

obs_properties_t *ndi_output_getproperties(void *)
{
	obs_log(LOG_DEBUG, "+ndi_output_getproperties()");

	obs_properties_t *props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_properties_add_text(props, NDI_OUTPUT_KEY_NAME, obs_module_text("NDIPlugin.OutputProps.NDIName"),
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, NDI_OUTPUT_KEY_GROUPS, obs_module_text("NDIPlugin.OutputProps.NDIGroups"),
				OBS_TEXT_DEFAULT);

	obs_log(LOG_DEBUG, "-ndi_output_getproperties()");

	return props;
}

void ndi_output_getdefaults(obs_data_t *settings)
{
	obs_log(LOG_DEBUG, "+ndi_output_getdefaults()");
	obs_data_set_default_string(settings, NDI_OUTPUT_KEY_NAME, NDI_OUTPUT_DEFAULT_NAME);
	obs_data_set_default_string(settings, NDI_OUTPUT_KEY_GROUPS, NDI_OUTPUT_DEFAULT_NAME);
	obs_data_set_default_bool(settings, NDI_OUTPUT_KEY_USES_VIDEO, true);
	obs_data_set_default_bool(settings, NDI_OUTPUT_KEY_USES_AUDIO, true);
	obs_log(LOG_DEBUG, "-ndi_output_getdefaults()");
}

void ndi_output_update(void *data, obs_data_t *settings);

void *ndi_output_create(obs_data_t *settings, obs_output_t *output)
{
	auto name = obs_data_get_string(settings, NDI_OUTPUT_KEY_NAME);
	auto groups = obs_data_get_string(settings, NDI_OUTPUT_KEY_GROUPS);
	obs_log(LOG_DEBUG, "+ndi_output_create(name='%s', groups='%s', ...)", name, groups);
	auto o = new ndi_output_t();
	o->output = output;
	ndi_output_update(o, settings);

	obs_log(LOG_DEBUG, "-ndi_output_create(name='%s', groups='%s', ...)", name, groups);
	return o;
}

static const std::map<video_format, std::string> video_to_color_format_map = {{VIDEO_FORMAT_P010, "P010"},
									      {VIDEO_FORMAT_I010, "I010"},
									      {VIDEO_FORMAT_P216, "P216"},
									      {VIDEO_FORMAT_P416, "P416"}};

bool ndi_output_start(void *data)
{
	auto o = static_cast<ndi_output_t *>(data);
	const char *name = o->ndi_name.c_str();
	const char *groups = o->ndi_groups.c_str();
	obs_log(LOG_DEBUG, "+ndi_output_start(name='%s', groups='%s', ...)", name, groups);
	if (o->started) {
		obs_log(LOG_INFO, "NDI Output already started: '%s'", name);
		obs_log(LOG_DEBUG, "-ndi_output_start(name='%s', groups='%s', ...)", name, groups);
		return false;
	}

	uint32_t flags = 0;
	video_t *video = obs_output_video(o->output);
	audio_t *audio = obs_output_audio(o->output);
	obs_output_set_last_error(o->output, "");

	if (!video && !audio) {
		obs_log(LOG_WARNING, "WARN-413 - NDI Output could not start. No Audio/Video data available. ('%s')",
			name);
		obs_log(LOG_DEBUG, "'%s'('%s') ndi_output_start: no video nor audio available", name, groups);
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
			o->conv_buffer.assign((size_t)height * (size_t)o->conv_linesize, 0);
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
			obs_log(LOG_ERROR, "ERR-410 - NDI Output cannot start : Unsupported pixel format %d. ('%s')",
				format, name);
			obs_log(LOG_DEBUG, "-ndi_output_start(name='%s', groups='%s', ...)", name, groups);
			auto it = video_to_color_format_map.find(format);
			auto error_string =
				std::string(obs_module_text("NDIPlugin.OutputSettings.LastError")) +
				(it != video_to_color_format_map.end() ? it->second : std::to_string(format));
			obs_output_set_last_error(o->output, error_string.c_str());
			return false;
		}

		o->frame_width = width;
		o->frame_height = height;
		const video_output_info *voi = video_output_get_info(video);
		o->frame_rate_n = (int)voi->fps_num;
		o->frame_rate_d = (int)voi->fps_den;
		flags |= OBS_OUTPUT_VIDEO;
	}

	if (o->uses_audio && audio) {
		o->audio_samplerate = audio_output_get_sample_rate(audio);
		o->audio_channels = audio_output_get_channels(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}

	NDIlib_send_create_t send_desc{};
	send_desc.p_ndi_name = name;
	send_desc.p_groups = groups[0] ? groups : nullptr;
	send_desc.clock_video = false;
	send_desc.clock_audio = false;

	std::lock_guard<std::mutex> lock(o->ndi_sender_mutex);
	o->ndi_sender = ndiLib->send_create(&send_desc);

	if (o->ndi_sender) {
		o->started = obs_output_begin_data_capture(o->output, flags);
		if (o->started) {
			obs_log(LOG_INFO, "NDI Output started successfully. '%s'", name);
			obs_log(LOG_DEBUG, "'%s' ndi_output_start: ndi output started", name);
		} else {
			obs_log(LOG_WARNING, "WARN-415 - NDI Sender data capture failed. '%s'", name);
			obs_log(LOG_DEBUG, "'%s' ndi_output_start: data capture start failed", name);
		}
	} else {
		obs_log(LOG_WARNING, "WARN-416 - NDI Sender initialisation failed. '%s'", name);
		obs_log(LOG_DEBUG, "'%s' ndi_output_start: ndi sender init failed", name);
	}

	obs_log(LOG_DEBUG, "-ndi_output_start(name='%s', groups='%s'...)", name, groups);

	return o->started;
}

void ndi_output_stop(void *data, uint64_t);

void replace_invalid_filename_chars(QString *s);

void ndi_output_update(void *data, obs_data_t *settings)
{
	auto o = static_cast<ndi_output_t *>(data);
	auto name = obs_data_get_string(settings, NDI_OUTPUT_KEY_NAME);
	// Replace invalid filename characters in the provided name and persist back to settings.
	if (name) {
		QString qname = QString::fromUtf8(name);
		replace_invalid_filename_chars(&qname);
		obs_data_set_string(settings, NDI_OUTPUT_KEY_NAME, QT_TO_UTF8(qname));
		name = obs_data_get_string(settings, NDI_OUTPUT_KEY_NAME);
	}
	auto groups = obs_data_get_string(settings, NDI_OUTPUT_KEY_GROUPS);
	obs_log(LOG_DEBUG, "ndi_output_update(name='%s', groups='%s', ...)", name, groups);

	// If the output is running, stop it so it can be recreated with the new settings.
	bool ndi_output_running = o->started;

	if (ndi_output_running) {
		obs_log(LOG_DEBUG, "ndi_output_update: ndi_name/ndi_groups changed, stopping output to apply changes");
		// Stop the output; timestamp argument not used here so pass0.
		ndi_output_stop(o, 0);
	}

	o->ndi_name = name ? name : "";
	o->ndi_groups = groups ? groups : "";
	o->uses_video = obs_data_get_bool(settings, NDI_OUTPUT_KEY_USES_VIDEO);
	o->uses_audio = obs_data_get_bool(settings, NDI_OUTPUT_KEY_USES_AUDIO);

	auto outputName = obs_output_get_name(o->output);
	if (outputName && std::strcmp(outputName, NDI_MAIN_OUTPUT_NAME) == 0) {
		auto config = Config::Current();
		config->OutputName = o->ndi_name.c_str();
		config->OutputGroups = o->ndi_groups.c_str();
		config->Save();
	} else if (outputName && std::strcmp(outputName, NDI_PREVIEW_OUTPUT_NAME) == 0) {
		auto config = Config::Current();
		config->PreviewOutputName = o->ndi_name.c_str();
		config->PreviewOutputGroups = o->ndi_groups.c_str();
		config->Save();
	}

	if (ndi_output_running && !o->started)
		ndi_output_start(o);

	obs_log(LOG_INFO, "NDI Output Updated. '%s'", name);
	obs_log(LOG_DEBUG, "ndi_output_update(name='%s', groups='%s', uses_video='%s', uses_audio='%s')", name, groups,
		o->uses_video ? "true" : "false", o->uses_audio ? "true" : "false");
}

void ndi_output_stop(void *data, uint64_t)
{
	auto o = static_cast<ndi_output_t *>(data);
	const char *name = o->ndi_name.c_str();
	const char *groups = o->ndi_groups.c_str();
	obs_log(LOG_DEBUG, "+ndi_output_stop(name='%s', groups='%s', ...)", name, groups);
	if (o->started) {
		o->started = false;

		obs_output_end_data_capture(o->output);

		{
			std::lock_guard<std::mutex> lock(o->ndi_sender_mutex);
			if (o->ndi_sender) {
				obs_log(LOG_DEBUG, "ndi_output_stop: +ndiLib->send_destroy(o->ndi_sender)");
				ndiLib->send_destroy(o->ndi_sender);
				obs_log(LOG_DEBUG, "ndi_output_stop: -ndiLib->send_destroy(o->ndi_sender)");
				o->ndi_sender = nullptr;
			}
		}

		o->conv_buffer.clear();
		o->conv_function = nullptr;

		o->frame_width = 0;
		o->frame_height = 0;
		o->frame_rate_n = 0;
		o->frame_rate_d = 1;
		o->audio_channels = 0;
		o->audio_samplerate = 0;

		obs_log(LOG_INFO, "NDI Output Stopped. '%s'", name);
	}

	obs_log(LOG_DEBUG, "-ndi_output_stop(name='%s', groups='%s', ...)", name, groups);
}

void ndi_output_destroy(void *data)
{
	auto o = static_cast<ndi_output_t *>(data);
	obs_log(LOG_DEBUG, "+ndi_output_destroy(name='%s', groups='%s', ...)", o->ndi_name.c_str(),
		o->ndi_groups.c_str());
	obs_log(LOG_DEBUG, "-ndi_output_destroy(name='%s', groups='%s', ...)", o->ndi_name.c_str(),
		o->ndi_groups.c_str());
	delete o;
}

void ndi_output_rawvideo(void *data, video_data *frame)
{
	auto o = static_cast<ndi_output_t *>(data);
	if (!o->started || !o->frame_width || !o->frame_height)
		return;

	// Held until the send completes so ndi_output_stop cannot destroy the sender mid-send.
	std::lock_guard<std::mutex> lock(o->ndi_sender_mutex);
	if (!o->ndi_sender)
		return;

	o->connections.check(o->ndi_sender, "video");

	uint32_t height = o->frame_height;

	NDIlib_video_frame_v2_t video_frame = {0};
	video_frame.xres = o->frame_width;
	video_frame.yres = height;
	video_frame.frame_rate_N = o->frame_rate_n;
	video_frame.frame_rate_D = o->frame_rate_d;
	video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
#ifdef SYNC_DEBUG
	video_frame.timestamp = frame->timestamp / 100;
#endif
	video_frame.timecode = NDIlib_send_timecode_synthesize;
	video_frame.FourCC = o->frame_fourcc;

	if (video_frame.FourCC == NDIlib_FourCC_type_UYVY) {
		o->conv_function(frame->data, frame->linesize, 0, height, o->conv_buffer.data(), o->conv_linesize);
		video_frame.p_data = o->conv_buffer.data();
		video_frame.line_stride_in_bytes = o->conv_linesize;
	} else {
		video_frame.p_data = frame->data[0];
		video_frame.line_stride_in_bytes = frame->linesize[0];
	}
	SYNC_DEBUG_LOG_VIDEO_TIME("NDI <- ndi_output", o->ndi_name.c_str(), video_frame.timestamp * 100,
				  (uint8_t *)video_frame.p_data);
	ndiLib->send_send_video_async_v2(o->ndi_sender, &video_frame);
}

void ndi_output_rawaudio(void *data, audio_data *frame)
{
	// NOTE: The logic in this function should be similar to
	// ndi-filter.cpp/ndi_filter_asyncaudio(...)
	auto o = static_cast<ndi_output_t *>(data);
	if (!o->started || !o->audio_samplerate || !o->audio_channels)
		return;

	std::lock_guard<std::mutex> lock(o->ndi_sender_mutex);
	if (!o->ndi_sender)
		return;

	o->connections.check(o->ndi_sender, "audio");

	NDIlib_audio_frame_v3_t audio_frame = {0};
	audio_frame.sample_rate = o->audio_samplerate;
	audio_frame.no_channels = (int)o->audio_channels;
#ifdef SYNC_DEBUG
	audio_frame.timestamp = frame->timestamp / 100;
#endif
	audio_frame.timecode = NDIlib_send_timecode_synthesize;
	audio_frame.no_samples = frame->frames;
	audio_frame.channel_stride_in_bytes = frame->frames * sizeof(float);
	audio_frame.FourCC = NDIlib_FourCC_audio_type_FLTP;

	const size_t channels = (size_t)audio_frame.no_channels;
	audio_frame.p_data =
		const_cast<uint8_t *>(ndi_pack_planar_audio(frame->data, channels, audio_frame.channel_stride_in_bytes,
							    o->audio_conv_buffer, o->audio_path, "NDI Output"));
	SYNC_DEBUG_LOG_AUDIO_TIME("NDI <- ndi_output", o->ndi_name.c_str(), audio_frame.timestamp * 100,
				  (float *)audio_frame.p_data, audio_frame.no_samples, audio_frame.sample_rate);
	ndiLib->send_send_audio_v3(o->ndi_sender, &audio_frame);
}

obs_output_info create_ndi_output_info()
{
	obs_output_info ndi_output_info = {};
	ndi_output_info.id = NDI_OUTPUT_ID;
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
