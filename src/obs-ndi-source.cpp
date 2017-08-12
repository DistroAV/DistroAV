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

#ifdef _WIN32
#include <Windows.h>
#endif

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

#include "obs-ndi.h"

obs_source_t* find_filter_by_id(obs_source_t* context, const char* id) {
    if (!context)
        return nullptr;

    struct search_context {
        const char* query;
        obs_source_t* result;
    };

    struct search_context filter_search;
    filter_search.query = id;
    filter_search.result = nullptr;

    obs_source_enum_filters(context,
        [](obs_source_t*, obs_source_t* filter, void* param) {
            struct search_context* filter_search =
                static_cast<struct search_context*>(param);

            const char* id = obs_source_get_id(filter);
            if (strcmp(id, filter_search->query) == 0) {
                obs_source_addref(filter);
                filter_search->result = filter;
            }
        },
    &filter_search);

    return filter_search.result;
}

extern NDIlib_find_instance_t ndi_finder;

struct ndi_source {
    obs_source_t* source;
    NDIlib_recv_instance_t ndi_receiver;
    pthread_t video_thread;
    pthread_t audio_thread;
    bool running;
    NDIlib_tally_t tally;
    uint32_t no_sources;
    const NDIlib_source_t* ndi_sources;
    bool alpha_filter_enabled;
};

const char* ndi_source_getname(void* data) {
    UNUSED_PARAMETER(data);
    return obs_module_text("NDIPlugin.NDISourceName");
}

obs_properties_t* ndi_source_getproperties(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    obs_properties_t* props = obs_properties_create();
    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

    obs_property_t* source_list = obs_properties_add_list(props, "ndi_source",
        obs_module_text("NDIPlugin.SourceProps.SourceName"),
        OBS_COMBO_TYPE_LIST,
        OBS_COMBO_FORMAT_INT);

    obs_property_set_modified_callback(source_list, [](
        obs_properties_t* pps,
        obs_property_t* p,
        obs_data_t* settings) {
        size_t selected_item = obs_data_get_int(settings, "ndi_source");

        obs_data_set_string(settings, "ndi_source_name",
            obs_property_list_item_name(p, selected_item));
        obs_data_set_string(settings, "ndi_source_ip",
            obs_property_list_item_string(p, selected_item));

        return true;
    });

    obs_properties_add_bool(props, "ndi_low_bandwidth",
        obs_module_text("NDIPlugin.SourceProps.LowBandwidth"));

    obs_properties_add_bool(props, "ndi_fix_alpha_blending",
        obs_module_text("NDIPlugin.SourceProps.AlphaBlendingFix"));

    obs_properties_add_button(props, "ndi_website", "NDI.NewTek.com", [](
        obs_properties_t *pps,
        obs_property_t *prop,
        void* private_data) {
        #if defined(_WIN32)
        ShellExecute(NULL, "open", "http://ndi.newtek.com", NULL, NULL, SW_SHOWNORMAL);
        #elif defined(__linux__) || defined(__APPLE__)
        system("open http://ndi.newtek.com");
        #endif

        return true;
    });

    s->no_sources = 0;
    s->ndi_sources = ndiLib->NDIlib_find_get_current_sources(ndi_finder,
        &s->no_sources);

    for (uint32_t i = 0; i < s->no_sources; i++)
        obs_property_list_add_int(source_list, s->ndi_sources[i].p_ndi_name, i);

    return props;
}

void* ndi_source_poll_audio(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    NDIlib_audio_frame_t audio_frame;
    obs_source_audio obs_audio_frame = {0};

    NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;
    while (s->running) {
        frame_received = ndiLib->NDIlib_recv_capture(s->ndi_receiver,
            NULL, &audio_frame, NULL, 1000);

        if (frame_received == NDIlib_frame_type_audio) {
            switch (audio_frame.no_channels) {
                case 1:
                    obs_audio_frame.speakers = SPEAKERS_MONO;
                    break;
                case 2:
                    obs_audio_frame.speakers = SPEAKERS_STEREO;
                    break;
                case 3:
                    obs_audio_frame.speakers = SPEAKERS_2POINT1;
                    break;
                case 4:
                    obs_audio_frame.speakers = SPEAKERS_QUAD;
                    break;
                case 5:
                    obs_audio_frame.speakers = SPEAKERS_4POINT1;
                    break;
                case 6:
                    obs_audio_frame.speakers = SPEAKERS_5POINT1;
                    break;
                case 8:
                    obs_audio_frame.speakers = SPEAKERS_7POINT1;
                    break;
                default:
                    obs_audio_frame.speakers = SPEAKERS_UNKNOWN;
            }

            obs_audio_frame.timestamp =
                os_gettime_ns() - (audio_frame.sample_rate * audio_frame.no_samples);
            obs_audio_frame.samples_per_sec = audio_frame.sample_rate;
            obs_audio_frame.format = AUDIO_FORMAT_FLOAT_PLANAR;
            obs_audio_frame.frames = audio_frame.no_samples;

            for (int i = 0; i < audio_frame.no_channels; i++) {
                obs_audio_frame.data[i] =
                    (uint8_t*)(&audio_frame.p_data[i * audio_frame.no_samples]);
            }

            obs_source_output_audio(s->source, &obs_audio_frame);
            ndiLib->NDIlib_recv_free_audio(s->ndi_receiver, &audio_frame);
        }
    }

    return NULL;
}

void* ndi_source_poll_video(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    NDIlib_video_frame_t video_frame;
    obs_source_frame obs_video_frame = {0};

    NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;
    while (s->running) {
        frame_received = ndiLib->NDIlib_recv_capture(s->ndi_receiver,
            &video_frame, NULL, NULL, 1000);

        if (frame_received == NDIlib_frame_type_video) {
            switch (video_frame.FourCC) {
                case NDIlib_FourCC_type_BGRA:
                    obs_video_frame.format = VIDEO_FORMAT_BGRA;
                    break;

                case NDIlib_FourCC_type_BGRX:
                    obs_video_frame.format = VIDEO_FORMAT_BGRX;
                    break;

                case NDIlib_FourCC_type_RGBA:
                case NDIlib_FourCC_type_RGBX:
                    obs_video_frame.format = VIDEO_FORMAT_RGBA;
                    break;

                case NDIlib_FourCC_type_UYVY:
                case NDIlib_FourCC_type_UYVA:
                    obs_video_frame.format = VIDEO_FORMAT_UYVY;
                    break;
            }

            obs_video_frame.timestamp = video_frame.timecode;
            obs_video_frame.width = video_frame.xres;
            obs_video_frame.height = video_frame.yres;
            obs_video_frame.linesize[0] = video_frame.line_stride_in_bytes;
            obs_video_frame.data[0] = video_frame.p_data;

            video_format_get_parameters(VIDEO_CS_DEFAULT, VIDEO_RANGE_DEFAULT,
                obs_video_frame.color_matrix, obs_video_frame.color_range_min,
                obs_video_frame.color_range_max);

            obs_source_output_video(s->source, &obs_video_frame);

            ndiLib->NDIlib_recv_free_video(s->ndi_receiver, &video_frame);
        }
    }

    return NULL;
}

void ndi_source_update(void* data, obs_data_t* settings) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    NDIlib_source_t selected_source;
    selected_source.p_ndi_name =
        obs_data_get_string(settings, "ndi_source_name");
    selected_source.p_ip_address =
        obs_data_get_string(settings, "ndi_source_ip");
    bool lowBandwidth =
        obs_data_get_bool(settings, "ndi_low_bandwidth");
    
    s->alpha_filter_enabled =
        obs_data_get_bool(settings, "ndi_fix_alpha_blending");
    // Don't persist this value in settings
    obs_data_set_bool(settings, "ndi_fix_alpha_blending", false);

    if (s->alpha_filter_enabled) {
        obs_source_t* existing_filter =
            find_filter_by_id(s->source, OBS_NDI_ALPHA_FILTER_ID);

        if (!existing_filter) {
            obs_source_t* new_filter =
                obs_source_create(OBS_NDI_ALPHA_FILTER_ID,
                  obs_module_text("NDIPlugin.PremultipliedAlphaFilterName"),
                  nullptr, nullptr);
            obs_source_filter_add(s->source, new_filter);
            obs_source_release(new_filter);
        }
    }

    NDIlib_recv_create_t recv_desc;
    recv_desc.source_to_connect_to = selected_source;
    recv_desc.bandwidth =
        (lowBandwidth ? NDIlib_recv_bandwidth_lowest : NDIlib_recv_bandwidth_highest);
    recv_desc.allow_video_fields = true;
    recv_desc.color_format = NDIlib_recv_color_format_e_UYVY_BGRA;

    s->running = false;
    pthread_cancel(s->video_thread);
    pthread_cancel(s->audio_thread);
    ndiLib->NDIlib_recv_destroy(s->ndi_receiver);

    s->ndi_receiver = ndiLib->NDIlib_recv_create2(&recv_desc);
    if (s->ndi_receiver) {
        s->running = true;
        pthread_create(&s->video_thread, NULL, ndi_source_poll_video, data);
        pthread_create(&s->audio_thread, NULL, ndi_source_poll_audio, data);
    } else {
        blog(LOG_ERROR,
            "can't create a receiver for NDI source '%s'",
            recv_desc.source_to_connect_to.p_ndi_name);
    }
}

void ndi_source_shown(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    if (s->ndi_receiver) {
        s->tally.on_preview = true;
        ndiLib->NDIlib_recv_set_tally(s->ndi_receiver, &s->tally);
    }
}

void ndi_source_hidden(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    if (s->ndi_receiver) {
        s->tally.on_preview = false;
        ndiLib->NDIlib_recv_set_tally(s->ndi_receiver, &s->tally);
    }
}

void ndi_source_activated(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    if (s->ndi_receiver) {
        s->tally.on_program = true;
        ndiLib->NDIlib_recv_set_tally(s->ndi_receiver, &s->tally);
    }
}

void ndi_source_deactivated(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    if (s->ndi_receiver) {
        s->tally.on_program = false;
        ndiLib->NDIlib_recv_set_tally(s->ndi_receiver, &s->tally);
    }
}

void* ndi_source_create(obs_data_t* settings, obs_source_t* source) {
    struct ndi_source* s =
        static_cast<ndi_source*>(bzalloc(sizeof(struct ndi_source)));
    s->source = source;
    s->running = true;

    ndi_source_update(s, settings);
    return s;
}

void ndi_source_destroy(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);
    s->running = false;
    pthread_cancel(s->video_thread);
    pthread_cancel(s->audio_thread);
    ndiLib->NDIlib_recv_destroy(s->ndi_receiver);
}

struct obs_source_info create_ndi_source_info() {
    struct obs_source_info ndi_source_info = {};
    ndi_source_info.id				= "ndi_source";
    ndi_source_info.type			= OBS_SOURCE_TYPE_INPUT;
    ndi_source_info.output_flags	= OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO |
                                      OBS_SOURCE_DO_NOT_DUPLICATE;
    ndi_source_info.get_name		= ndi_source_getname;
    ndi_source_info.get_properties	= ndi_source_getproperties;
    ndi_source_info.update			= ndi_source_update;
    ndi_source_info.show			= ndi_source_shown;
    ndi_source_info.hide			= ndi_source_hidden;
    ndi_source_info.activate		= ndi_source_activated;
    ndi_source_info.deactivate		= ndi_source_deactivated;
    ndi_source_info.create			= ndi_source_create;
    ndi_source_info.destroy			= ndi_source_destroy;

    return ndi_source_info;
}
