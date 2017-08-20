/*
obs-ndi (NDI I/O in OBS Studio)
Copyright (C) 2016-2017 Stéphane Lepin <stephane.lepin@gmail.com>

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
#include <media-io/audio-resampler.h>

#include "obs-ndi.h"

#define NUM_BUFFERS 4
#define TEXFORMAT GS_BGRA

struct ndi_filter {
    obs_source_t* context;
    NDIlib_send_instance_t ndi_sender;
    struct obs_video_info ovi;
    struct obs_audio_info oai;
    obs_display_t* renderer;

    uint32_t known_width;
    uint32_t known_height;

    uint8_t write_tex;
    uint8_t read_tex;
    gs_texrender_t* tex[NUM_BUFFERS];

    gs_stagesurf_t* stagesurface;
    uint8_t* video_data;
    uint32_t video_linesize;

    bool audio_initialized;
    
    bool send_running;
    pthread_t send_thread;
    os_sem_t* frame_semaphore;
};

const char* ndi_filter_getname(void* data) {
    UNUSED_PARAMETER(data);
    return obs_module_text("NDIPlugin.FilterName");
}

obs_properties_t* ndi_filter_getproperties(void* data) {
    UNUSED_PARAMETER(data);
    obs_properties_t* props = obs_properties_create();
    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
    return props;
}

void ndi_filter_offscreen_render(void* data, uint32_t cx, uint32_t cy) {
    struct ndi_filter* s = static_cast<ndi_filter*>(data);

    if (!s->send_running) {
        return;
    }

    obs_source_t* target = obs_filter_get_target(s->context);
    uint32_t width = obs_source_get_base_width(target);
    uint32_t height = obs_source_get_base_height(target);

    gs_texrender_t* current_texture = s->tex[s->write_tex];
    s->write_tex = (s->write_tex >= (NUM_BUFFERS - 1) ? 0 : s->write_tex + 1);

    gs_texrender_reset(current_texture);

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

    if (gs_texrender_begin(current_texture, width, height)) {
        struct vec4 background;
        vec4_zero(&background);

        gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
        gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

        obs_source_video_render(target);

        gs_texrender_end(current_texture);
        os_sem_post(s->frame_semaphore);
    }

    gs_blend_state_pop();
}

void* ndi_filter_send_thread(void* data) {
    struct ndi_filter* s = static_cast<ndi_filter*>(data);

    blog(LOG_INFO, "filter '%s': send thread started",
        obs_source_get_name(s->context));

    obs_source_t* target = obs_filter_get_target(s->context);
    while (s->send_running) {
        os_sem_wait(s->frame_semaphore);
        if (s->read_tex == s->write_tex) {
            continue;
        }

        uint32_t width = obs_source_get_base_width(target);
        uint32_t height = obs_source_get_base_height(target);

        obs_enter_graphics();
        if (s->known_width != width || s->known_height != height) {
            gs_stagesurface_unmap(s->stagesurface);
            gs_stagesurface_destroy(s->stagesurface);

            s->stagesurface = gs_stagesurface_create(width, height, TEXFORMAT);
            gs_stagesurface_map(s->stagesurface,
                &s->video_data, &s->video_linesize);

            s->known_width = width;
            s->known_height = height;
        }

        gs_stage_texture(s->stagesurface,
            gs_texrender_get_texture(s->tex[s->read_tex]));
        obs_leave_graphics();

        s->read_tex = (s->read_tex >= (NUM_BUFFERS - 1) ? 0 : s->read_tex + 1);

        NDIlib_video_frame_v2_t video_frame = { 0 };
        video_frame.xres = s->known_width;
        video_frame.yres = s->known_height;
        video_frame.FourCC = NDIlib_FourCC_type_BGRA;
        video_frame.frame_rate_N = s->ovi.fps_num;
        video_frame.frame_rate_D = s->ovi.fps_den;
        video_frame.picture_aspect_ratio =
                (float)video_frame.xres / (float)video_frame.yres;
        video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
        video_frame.timecode = NDIlib_send_timecode_synthesize;
        video_frame.p_data = s->video_data;
        video_frame.line_stride_in_bytes = s->video_linesize;

        ndiLib->NDIlib_send_send_video_async_v2(s->ndi_sender, &video_frame);
    }

    blog(LOG_INFO, "filter '%s': send thread stopped",
        obs_source_get_name(s->context));
    return nullptr;
}

void ndi_filter_update(void* data, obs_data_t* settings) {
    UNUSED_PARAMETER(settings);
    struct ndi_filter* s = static_cast<ndi_filter*>(data);

    if (s->send_running) {
        s->send_running = false;
        os_sem_post(s->frame_semaphore);
        pthread_join(s->send_thread, NULL);
    }
    s->send_running = false;

    NDIlib_send_create_t send_desc;
    send_desc.p_ndi_name = obs_source_get_name(s->context);
    send_desc.p_groups = NULL;
    send_desc.clock_video = false;
    send_desc.clock_audio = false;

    ndiLib->NDIlib_send_destroy(s->ndi_sender);
    s->ndi_sender = ndiLib->NDIlib_send_create(&send_desc);

    s->send_running = true;
    pthread_create(&s->send_thread, nullptr, ndi_filter_send_thread, data);
}

void* ndi_filter_create(obs_data_t* settings, obs_source_t* source) {
    struct ndi_filter* s =
        static_cast<ndi_filter*>(bzalloc(sizeof(struct ndi_filter)));
    s->context = source;

    s->write_tex = 0;
    s->read_tex = 0;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        s->tex[i] = gs_texrender_create(TEXFORMAT, GS_ZS_NONE);
    }

    s->known_width = 0;
    s->known_height = 0;
    s->audio_initialized = false;

    s->send_running = false;
    os_sem_init(&s->frame_semaphore, 0);

    gs_init_data display_desc = {};
    display_desc.adapter = 0;
    display_desc.format = GS_BGRA;
    display_desc.zsformat = GS_ZS_NONE;
    display_desc.cx = 0;
    display_desc.cy = 0;

    #ifdef _WIN32
    display_desc.window.hwnd = obs_frontend_get_main_window_handle();
    #elif __APPLE__
    display_desc.window.view = (id) obs_frontend_get_main_window_handle();
    #endif

    obs_get_video_info(&s->ovi);
    obs_get_audio_info(&s->oai);

    s->renderer = obs_display_create(&display_desc);
    obs_display_add_draw_callback(s->renderer, ndi_filter_offscreen_render, s);

    ndi_filter_update(s, settings);

    return s;
}

void ndi_filter_destroy(void* data) {
    struct ndi_filter* s = static_cast<ndi_filter*>(data);

    if (s->send_running) {
        s->send_running = false;
        os_sem_post(s->frame_semaphore);
        pthread_join(s->send_thread, NULL);
    }

    obs_display_destroy(s->renderer);
    ndiLib->NDIlib_send_destroy(s->ndi_sender);

    gs_stagesurface_unmap(s->stagesurface);
    gs_stagesurface_destroy(s->stagesurface);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        gs_texrender_destroy(s->tex[i]);
    }

    os_sem_destroy(s->frame_semaphore);
}

void ndi_filter_tick(void* data, float seconds) {
    struct ndi_filter* s = static_cast<ndi_filter*>(data);

    obs_get_video_info(&s->ovi);
    obs_get_audio_info(&s->oai);
}

void ndi_filter_videorender(void* data, gs_effect_t* effect) {
    UNUSED_PARAMETER(effect);
    struct ndi_filter* s = static_cast<ndi_filter*>(data);
    obs_source_skip_video_filter(s->context);
}

struct obs_audio_data* ndi_filter_audiofilter(void *data,
        struct obs_audio_data *audio_data) {
    struct ndi_filter *s = static_cast<ndi_filter *>(data);

    NDIlib_audio_frame_v2_t audio_frame = { 0 };
    audio_frame.sample_rate = s->oai.samples_per_sec;
    audio_frame.no_channels = s->oai.speakers;
    audio_frame.timecode = audio_data->timestamp;
    audio_frame.no_samples = audio_data->frames;
    audio_frame.channel_stride_in_bytes = audio_frame.no_samples * 4;

    size_t data_size =
        audio_frame.no_channels * audio_frame.channel_stride_in_bytes;
    uint8_t* ndi_data = (uint8_t*)bmalloc(data_size);

    for (int i = 0; i < audio_frame.no_channels; i++) {
        memcpy(&ndi_data[i * audio_frame.channel_stride_in_bytes],
            audio_data->data[i],
            audio_frame.channel_stride_in_bytes);
    }

    audio_frame.p_data = (float*)ndi_data;

    ndiLib->NDIlib_send_send_audio_v2(s->ndi_sender, &audio_frame);
    bfree(ndi_data);

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
