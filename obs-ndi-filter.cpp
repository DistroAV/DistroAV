/*
	obs-ndi (NDI I/O in OBS Studio)
	Copyright (C) 2016 Stéphane Lepin <stephane.lepin@gmail.com>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <util/threading.h>
#include <media-io/video-frame.h>

#include "obs-ndi.h"

#define TEXFORMAT GS_BGRA

struct ndi_filter {
	obs_source_t *context;
	NDIlib_send_instance_t ndi_sender;
	struct obs_video_info ovi;
	obs_display_t *renderer;
	gs_stagesurf_t *stagesurface;
	gs_texrender_t *texrender;
	uint32_t last_width;
	uint32_t last_height;

	uint8_t *video_data;
	uint32_t video_linesize;

	bool audio_initialized;
};

const char* ndi_filter_getname(void *data) {
	UNUSED_PARAMETER(data);
	return obs_module_text("NDIFilter");
}

obs_properties_t* ndi_filter_getproperties(void *data) {
	UNUSED_PARAMETER(data);
	obs_properties_t* props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	return props;
}

void ndi_filter_update(void *data, obs_data_t *settings) {
	UNUSED_PARAMETER(settings);
	struct ndi_filter *s = static_cast<ndi_filter *>(data);

	NDIlib_send_create_t send_desc;
	send_desc.p_ndi_name = obs_source_get_name(s->context);
	send_desc.p_groups = NULL;
	send_desc.clock_video = false;
	send_desc.clock_audio = false;

	ndiLib->NDIlib_send_destroy(s->ndi_sender);
	s->ndi_sender = ndiLib->NDIlib_send_create(&send_desc);
}

void ndi_filter_offscreen_render(void *data, uint32_t cx, uint32_t cy) {
	struct ndi_filter *s = static_cast<ndi_filter *>(data);
	
	obs_source_t *target = obs_filter_get_target(s->context);

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);

	if (s->last_width != width || s->last_height != height) {
		gs_stagesurface_unmap(s->stagesurface);
		gs_stagesurface_destroy(s->stagesurface);

		s->stagesurface = gs_stagesurface_create(width, height, TEXFORMAT);
		gs_stagesurface_map(s->stagesurface, &s->video_data, &s->video_linesize);

		s->last_width = width;
		s->last_height = height;
	}

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(s->texrender, width, height)) {
		struct vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		obs_source_video_render(target);

		gs_texrender_end(s->texrender);
		gs_stage_texture(s->stagesurface, gs_texrender_get_texture(s->texrender));

		NDIlib_video_frame_t video_frame = { 0 };
		video_frame.xres = s->last_width;
		video_frame.yres = s->last_height;
		video_frame.FourCC = NDIlib_FourCC_type_BGRA;
		video_frame.frame_rate_N = s->ovi.fps_num;
		video_frame.frame_rate_D = s->ovi.fps_den;
		video_frame.picture_aspect_ratio = (float)video_frame.xres / (float)video_frame.yres;
		video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
		video_frame.timecode = NDIlib_send_timecode_synthesize;
		video_frame.p_data = s->video_data;
		video_frame.line_stride_in_bytes = s->video_linesize;

		ndiLib->NDIlib_send_send_video_async(s->ndi_sender, &video_frame);
	}

	gs_blend_state_pop();
	
	//obs_source_release(target);
}

void* ndi_filter_create(obs_data_t *settings, obs_source_t *source) {
	struct ndi_filter *s = static_cast<ndi_filter *>(bzalloc(sizeof(struct ndi_filter)));
	s->context = source;

	s->texrender = gs_texrender_create(TEXFORMAT, GS_ZS_NONE);

	s->last_width = 0;
	s->last_height = 0;
	s->audio_initialized = false;

	gs_init_data display_desc = {};
	display_desc.adapter = 0;
	display_desc.format = GS_BGRA;
	display_desc.zsformat = GS_ZS_NONE;
	display_desc.cx = 0;
	display_desc.cy = 0;
	
	#ifdef _WIN32
	display_desc.window.hwnd = obs_frontend_get_main_window_handle();
	#elif __APPLE__
	display_desc.window.view = obs_frontend_get_main_window_handle();
	#endif
	
	s->renderer = obs_display_create(&display_desc);
	obs_display_add_draw_callback(s->renderer, ndi_filter_offscreen_render, s);

	obs_get_video_info(&s->ovi);
	ndi_filter_update(s, settings);

	return s;
}

void ndi_filter_destroy(void *data) {
	struct ndi_filter *s = static_cast<ndi_filter *>(data);

	obs_display_destroy(s->renderer);
	ndiLib->NDIlib_send_destroy(s->ndi_sender);

	gs_stagesurface_unmap(s->stagesurface);
	gs_stagesurface_destroy(s->stagesurface);
	gs_texrender_destroy(s->texrender);
}

void ndi_filter_tick(void *data, float seconds) {
	struct ndi_filter *s = static_cast<ndi_filter *>(data);

	obs_get_video_info(&s->ovi);
	gs_texrender_reset(s->texrender);
}

void ndi_filter_videorender(void *data, gs_effect_t *effect) {
	UNUSED_PARAMETER(effect);
	struct ndi_filter *s = static_cast<ndi_filter *>(data);

	obs_source_skip_video_filter(s->context);
}

struct obs_audio_data* ndi_filter_audiofilter(void *data, struct obs_audio_data *audio_data) {
	struct ndi_filter *s = static_cast<ndi_filter *>(data);
	const audio_t *obs_audio = obs_get_audio();

	NDIlib_audio_frame_t audio_frame = { 0 };
	audio_frame.sample_rate = audio_output_get_sample_rate(obs_audio);
	audio_frame.no_channels = audio_output_get_channels(obs_audio);
	audio_frame.timecode = audio_data->timestamp;
	audio_frame.no_samples = audio_data->frames;
	audio_frame.p_data = (float*)(audio_data->data[0]);

	ndiLib->NDIlib_send_send_audio(s->ndi_sender, &audio_frame);

	return audio_data;
}

struct obs_source_info create_ndi_filter_info() {
	struct obs_source_info ndi_filter_info = {};
	ndi_filter_info.id				= "ndi_filter";
	ndi_filter_info.type			= OBS_SOURCE_TYPE_FILTER;
	ndi_filter_info.output_flags	= OBS_SOURCE_VIDEO;
	ndi_filter_info.get_name		= ndi_filter_getname;
	ndi_filter_info.get_properties	= ndi_filter_getproperties;
	ndi_filter_info.create			= ndi_filter_create;
	ndi_filter_info.destroy			= ndi_filter_destroy;
	ndi_filter_info.update			= ndi_filter_update;
	ndi_filter_info.video_tick		= ndi_filter_tick;
	ndi_filter_info.video_render	= ndi_filter_videorender;

	// Audio is available only with async sources
	ndi_filter_info.filter_audio	= ndi_filter_audiofilter; 

	return ndi_filter_info;
}

