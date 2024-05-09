/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#ifdef _WIN32
#include <Windows.h>
#endif

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <util/threading.h>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include <media-io/audio-resampler.h>
#include <QString>

#include "plugin-main.h"

#define TEXFORMAT GS_BGRA
#define FLT_PROP_NAME "ndi_filter_ndiname"

typedef struct {
	obs_source_t *context;

	NDIlib_send_instance_t ndi_sender;

	pthread_mutex_t ndi_sender_video_mutex;
	pthread_mutex_t ndi_sender_audio_mutex;

	obs_video_info ovi;
	obs_audio_info oai;

	uint32_t known_width;
	uint32_t known_height;

	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	uint8_t *video_data;
	uint32_t video_linesize;

	video_t *video_output;
	bool is_audioonly;

	uint8_t *audio_conv_buffer;
	size_t audio_conv_buffer_size;
} ndi_filter_t;

const char *ndi_filter_getname(void *)
{
	return obs_module_text("NDIPlugin.FilterName");
}

const char *ndi_audiofilter_getname(void *)
{
	return obs_module_text("NDIPlugin.AudioFilterName");
}

void ndi_filter_update(void *data, obs_data_t *settings);

obs_properties_t *ndi_filter_getproperties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_properties_add_text(
		props, FLT_PROP_NAME,
		obs_module_text("NDIPlugin.FilterProps.NDIName"),
		OBS_TEXT_DEFAULT);

	obs_properties_add_button(
		props, "ndi_apply",
		obs_module_text("NDIPlugin.FilterProps.ApplySettings"),
		[](obs_properties_t *, obs_property_t *, void *private_data) {
			auto s = (ndi_filter_t *)private_data;
			auto settings = obs_source_get_settings(s->context);
			ndi_filter_update(s, settings);
			obs_data_release(settings);
			return true;
		});

	auto ndi_website_button = obs_properties_add_button(
		props, "ndi_website", NDI_WEB_URL, nullptr);
	obs_property_button_set_type(ndi_website_button, OBS_BUTTON_URL);
	obs_property_button_set_url(ndi_website_button,
				    const_cast<char *>(NDI_WEB_URL));

	return props;
}

void ndi_filter_getdefaults(obs_data_t *defaults)
{
	obs_data_set_default_string(
		defaults, FLT_PROP_NAME,
		obs_module_text("NDIPlugin.FilterProps.NDIName.Default"));
}

void ndi_filter_raw_video(void *data, video_data *frame)
{
	auto f = (ndi_filter_t *)data;

	if (!frame || !frame->data[0])
		return;

	NDIlib_video_frame_v2_t video_frame = {0};
	video_frame.xres = f->known_width;
	video_frame.yres = f->known_height;
	video_frame.FourCC = NDIlib_FourCC_type_BGRA;
	video_frame.frame_rate_N = f->ovi.fps_num;
	video_frame.frame_rate_D = f->ovi.fps_den;
	video_frame.picture_aspect_ratio = 0; // square pixels
	video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
	video_frame.timecode = (frame->timestamp / 100);
	video_frame.p_data = frame->data[0];
	video_frame.line_stride_in_bytes = frame->linesize[0];

	pthread_mutex_lock(&f->ndi_sender_video_mutex);
	ndiLib->send_send_video_v2(f->ndi_sender, &video_frame);
	pthread_mutex_unlock(&f->ndi_sender_video_mutex);
}

void ndi_filter_offscreen_render(void *data, uint32_t, uint32_t)
{
	auto f = (ndi_filter_t *)data;

	obs_source_t *target = obs_filter_get_parent(f->context);
	if (!target) {
		return;
	}

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);

	gs_texrender_reset(f->texrender);

	if (gs_texrender_begin(f->texrender, width, height)) {
		vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f,
			 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(target);

		gs_blend_state_pop();
		gs_texrender_end(f->texrender);

		if (f->known_width != width || f->known_height != height) {

			gs_stagesurface_destroy(f->stagesurface);
			f->stagesurface = gs_stagesurface_create(width, height,
								 TEXFORMAT);

			video_output_info vi = {0};
			vi.format = VIDEO_FORMAT_BGRA;
			vi.width = width;
			vi.height = height;
			vi.fps_den = f->ovi.fps_den;
			vi.fps_num = f->ovi.fps_num;
			vi.cache_size = 16;
			vi.colorspace = VIDEO_CS_DEFAULT;
			vi.range = VIDEO_RANGE_DEFAULT;
			vi.name = obs_source_get_name(f->context);

			video_output_close(f->video_output);
			video_output_open(&f->video_output, &vi);
			video_output_connect(f->video_output, nullptr,
					     ndi_filter_raw_video, f);

			f->known_width = width;
			f->known_height = height;
		}

		video_frame output_frame;
		if (video_output_lock_frame(f->video_output, &output_frame, 1,
					    os_gettime_ns())) {
			if (f->video_data) {
				gs_stagesurface_unmap(f->stagesurface);
				f->video_data = nullptr;
			}

			gs_stage_texture(
				f->stagesurface,
				gs_texrender_get_texture(f->texrender));
			gs_stagesurface_map(f->stagesurface, &f->video_data,
					    &f->video_linesize);

			uint32_t linesize = output_frame.linesize[0];
			for (uint32_t i = 0; i < f->known_height; ++i) {
				uint32_t dst_offset = linesize * i;
				uint32_t src_offset = f->video_linesize * i;
				memcpy(output_frame.data[0] + dst_offset,
				       f->video_data + src_offset, linesize);
			}

			video_output_unlock_frame(f->video_output);
		}
	}
}

void ndi_filter_update(void *data, obs_data_t *settings)
{
	auto f = (ndi_filter_t *)data;

	obs_remove_main_render_callback(ndi_filter_offscreen_render, f);

	NDIlib_send_create_t send_desc;
	send_desc.p_ndi_name = obs_data_get_string(settings, FLT_PROP_NAME);
	send_desc.p_groups = nullptr;
	send_desc.clock_video = false;
	send_desc.clock_audio = false;

	if (!f->is_audioonly) {
		pthread_mutex_lock(&f->ndi_sender_video_mutex);
	}
	pthread_mutex_lock(&f->ndi_sender_audio_mutex);
	ndiLib->send_destroy(f->ndi_sender);
	f->ndi_sender = ndiLib->send_create(&send_desc);
	pthread_mutex_unlock(&f->ndi_sender_audio_mutex);
	if (!f->is_audioonly) {
		pthread_mutex_unlock(&f->ndi_sender_video_mutex);
		obs_add_main_render_callback(ndi_filter_offscreen_render, f);
	}
}

void *ndi_filter_create(obs_data_t *settings, obs_source_t *source)
{
	auto name = obs_data_get_string(settings, FLT_PROP_NAME);
	blog(LOG_INFO, "[obs-ndi] +ndi_filter_create(name=\"%s\")", name);

	auto f = (ndi_filter_t *)bzalloc(sizeof(ndi_filter_t));
	f->context = source;
	f->texrender = gs_texrender_create(TEXFORMAT, GS_ZS_NONE);
	pthread_mutex_init(&f->ndi_sender_video_mutex, NULL);
	pthread_mutex_init(&f->ndi_sender_audio_mutex, NULL);
	obs_get_video_info(&f->ovi);
	obs_get_audio_info(&f->oai);

	ndi_filter_update(f, settings);

	blog(LOG_INFO, "[obs-ndi] -ndi_filter_create(...)");

	return f;
}

void *ndi_filter_create_audioonly(obs_data_t *settings, obs_source_t *source)
{
	auto name = obs_data_get_string(settings, FLT_PROP_NAME);
	blog(LOG_INFO, "[obs-ndi] +ndi_filter_create_audioonly(name=\"%s\")",
	     name);

	auto f = (ndi_filter_t *)bzalloc(sizeof(ndi_filter_t));
	f->is_audioonly = true;
	f->context = source;
	pthread_mutex_init(&f->ndi_sender_audio_mutex, NULL);
	obs_get_audio_info(&f->oai);

	ndi_filter_update(f, settings);

	blog(LOG_INFO, "[obs-ndi] -ndi_filter_create_audioonly(...)");

	return f;
}

void ndi_filter_destroy(void *data)
{
	blog(LOG_INFO, "[obs-ndi] +ndi_filter_destroy(...)");

	auto f = (ndi_filter_t *)data;

	obs_remove_main_render_callback(ndi_filter_offscreen_render, f);
	video_output_close(f->video_output);

	pthread_mutex_lock(&f->ndi_sender_video_mutex);
	pthread_mutex_lock(&f->ndi_sender_audio_mutex);
	ndiLib->send_destroy(f->ndi_sender);
	pthread_mutex_unlock(&f->ndi_sender_audio_mutex);
	pthread_mutex_unlock(&f->ndi_sender_video_mutex);

	gs_stagesurface_unmap(f->stagesurface);
	gs_stagesurface_destroy(f->stagesurface);
	gs_texrender_destroy(f->texrender);

	if (f->audio_conv_buffer) {
		blog(LOG_INFO,
		     "[obs-ndi] ndi_filter_destroy: freeing %zu bytes",
		     f->audio_conv_buffer_size);
		bfree(f->audio_conv_buffer);
		f->audio_conv_buffer = nullptr;
	}

	bfree(f);

	blog(LOG_INFO, "[obs-ndi] -ndi_filter_destroy(...)");
}

void ndi_filter_destroy_audioonly(void *data)
{
	blog(LOG_INFO, "[obs-ndi] +ndi_filter_destroy_audioonly(...)");

	auto f = (ndi_filter_t *)data;

	pthread_mutex_lock(&f->ndi_sender_audio_mutex);
	ndiLib->send_destroy(f->ndi_sender);
	pthread_mutex_unlock(&f->ndi_sender_audio_mutex);

	if (f->audio_conv_buffer) {
		bfree(f->audio_conv_buffer);
		f->audio_conv_buffer = nullptr;
	}

	bfree(f);

	blog(LOG_INFO, "[obs-ndi] -ndi_filter_destroy_audioonly(...)");
}

void ndi_filter_tick(void *data, float)
{
	auto f = (ndi_filter_t *)data;
	obs_get_video_info(&f->ovi);
}

void ndi_filter_videorender(void *data, gs_effect_t *)
{
	auto f = (ndi_filter_t *)data;
	obs_source_skip_video_filter(f->context);
}

obs_audio_data *ndi_filter_asyncaudio(void *data, obs_audio_data *audio_data)
{
	// NOTE: The logic in this function should be similar to
	// obs-ndi-output::ndi_output_raw_audio
	auto f = (ndi_filter_t *)data;

	obs_get_audio_info(&f->oai);

	NDIlib_audio_frame_v2_t audio_frame = {0};
	audio_frame.sample_rate = f->oai.samples_per_sec;
	audio_frame.no_channels = f->oai.speakers;
	audio_frame.timecode = audio_data->timestamp / 100;
	audio_frame.no_samples = audio_data->frames;
	audio_frame.channel_stride_in_bytes = audio_frame.no_samples * 4;

	const size_t data_size =
		audio_frame.no_channels * audio_frame.channel_stride_in_bytes;

	if (data_size > f->audio_conv_buffer_size) {
		blog(LOG_INFO,
		     "[obs-ndi] ndi_filter_asyncaudio: growing audio_conv_buffer from %zu to %zu bytes",
		     f->audio_conv_buffer_size, data_size);
		if (f->audio_conv_buffer) {
			blog(LOG_INFO,
			     "[obs-ndi] ndi_filter_asyncaudio: freeing %zu bytes",
			     f->audio_conv_buffer_size);
			bfree(f->audio_conv_buffer);
		}
		blog(LOG_INFO,
		     "[obs-ndi] ndi_filter_asyncaudio: allocating %zu bytes",
		     data_size);
		f->audio_conv_buffer = (uint8_t *)bmalloc(data_size);
		f->audio_conv_buffer_size = data_size;
	}

	for (int i = 0; i < audio_frame.no_channels; ++i) {
		memcpy(f->audio_conv_buffer +
			       (i * audio_frame.channel_stride_in_bytes),
		       audio_data->data[i],
		       audio_frame.channel_stride_in_bytes);
	}

	audio_frame.p_data = (float *)f->audio_conv_buffer;

	pthread_mutex_lock(&f->ndi_sender_audio_mutex);
	ndiLib->send_send_audio_v2(f->ndi_sender, &audio_frame);
	pthread_mutex_unlock(&f->ndi_sender_audio_mutex);

	return audio_data;
}

obs_source_info create_ndi_filter_info()
{
	obs_source_info ndi_filter_info = {};
	ndi_filter_info.id = "ndi_filter";
	ndi_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	ndi_filter_info.output_flags = OBS_SOURCE_VIDEO;

	ndi_filter_info.get_name = ndi_filter_getname;
	ndi_filter_info.get_properties = ndi_filter_getproperties;
	ndi_filter_info.get_defaults = ndi_filter_getdefaults;

	ndi_filter_info.create = ndi_filter_create;
	ndi_filter_info.destroy = ndi_filter_destroy;
	ndi_filter_info.update = ndi_filter_update;

	ndi_filter_info.video_tick = ndi_filter_tick;
	ndi_filter_info.video_render = ndi_filter_videorender;

	// Audio is available only with async sources
	ndi_filter_info.filter_audio = ndi_filter_asyncaudio;

	return ndi_filter_info;
}

obs_source_info create_ndi_audiofilter_info()
{
	obs_source_info ndi_filter_info = {};
	ndi_filter_info.id = "ndi_audiofilter";
	ndi_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	ndi_filter_info.output_flags = OBS_SOURCE_AUDIO;

	ndi_filter_info.get_name = ndi_audiofilter_getname;
	ndi_filter_info.get_properties = ndi_filter_getproperties;
	ndi_filter_info.get_defaults = ndi_filter_getdefaults;

	ndi_filter_info.create = ndi_filter_create_audioonly;
	ndi_filter_info.update = ndi_filter_update;
	ndi_filter_info.destroy = ndi_filter_destroy_audioonly;

	ndi_filter_info.filter_audio = ndi_filter_asyncaudio;

	return ndi_filter_info;
}
