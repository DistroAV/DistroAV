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

#define PROP_SOURCE "ndi_source_name"
#define PROP_BANDWIDTH "ndi_bw_mode"
#define PROP_HW_ACCEL "ndi_recv_hw_accel"
#define PROP_SYNC "ndi_sync"
#define PROP_FIX_ALPHA "ndi_fix_alpha_blending"

#define PROP_BW_HIGHEST 0
#define PROP_BW_LOWEST 1
#define PROP_BW_AUDIO_ONLY 2

#define PROP_SYNC_INTERNAL 0
#define PROP_SYNC_NDI_TIMESTAMP 1

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
    int sync_mode;
    pthread_t video_thread;
    pthread_t audio_thread;
    bool running;
    NDIlib_tally_t tally;
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

    obs_property_t* source_list = obs_properties_add_list(props, PROP_SOURCE,
        obs_module_text("NDIPlugin.SourceProps.SourceName"),
        OBS_COMBO_TYPE_LIST,
        OBS_COMBO_FORMAT_STRING);

    uint32_t nbSources = 0;
    const NDIlib_source_t* sources = ndiLib->NDIlib_find_get_current_sources(ndi_finder,
        &nbSources);

    for (uint32_t i = 0; i < nbSources; i++) {
        obs_property_list_add_string(source_list,
            sources[i].p_ndi_name, sources[i].p_ndi_name);
    }

    obs_property_t* bwModes = obs_properties_add_list(props, PROP_BANDWIDTH,
        obs_module_text("NDIPlugin.SourceProps.Bandwidth"),
        OBS_COMBO_TYPE_LIST,
        OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(bwModes,
        obs_module_text("NDIPlugin.BWMode.Highest"), PROP_BW_HIGHEST);
    obs_property_list_add_int(bwModes,
        obs_module_text("NDIPlugin.BWMode.Lowest"), PROP_BW_LOWEST);
    obs_property_list_add_int(bwModes,
        obs_module_text("NDIPlugin.BWMode.AudioOnly"), PROP_BW_AUDIO_ONLY);

    obs_property_t* syncModes = obs_properties_add_list(props, PROP_SYNC,
        obs_module_text("NDIPlugin.SourceProps.Sync"),
        OBS_COMBO_TYPE_LIST,
        OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(syncModes,
        obs_module_text("NDIPlugin.SyncMode.Internal"), PROP_SYNC_INTERNAL);
    obs_property_list_add_int(syncModes,
        obs_module_text("NDIPlugin.SyncMode.NDITimestamp"), PROP_SYNC_NDI_TIMESTAMP);

    obs_properties_add_bool(props, PROP_HW_ACCEL,
        obs_module_text("NDIPlugin.SourceProps.HWAccel"));

    obs_properties_add_bool(props, PROP_FIX_ALPHA,
        obs_module_text("NDIPlugin.SourceProps.AlphaBlendingFix"));

    obs_properties_add_button(props, "ndi_website", "NDI.NewTek.com", [](
        obs_properties_t *pps,
        obs_property_t *prop,
        void* private_data)
    {
        #if defined(_WIN32)
            ShellExecute(NULL, "open", "http://ndi.newtek.com", NULL, NULL, SW_SHOWNORMAL);
        #elif defined(__linux__) || defined(__APPLE__)
            system("open http://ndi.newtek.com");
        #endif

        return true;
    });

    return props;
}

void* ndi_source_poll_audio(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    blog(LOG_INFO, "audio thread for '%s' started",
                        obs_source_get_name(s->source));

    NDIlib_audio_frame_v2_t audio_frame;
    obs_source_audio obs_audio_frame = {0};

    NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;
    while (s->running) {
        frame_received = ndiLib->NDIlib_recv_capture_v2(
            s->ndi_receiver, nullptr, &audio_frame, nullptr, 1);

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

            switch (s->sync_mode) {
                case PROP_SYNC_INTERNAL:
                default:
                    obs_audio_frame.timestamp = os_gettime_ns();
                    obs_audio_frame.timestamp +=
                        (audio_frame.no_samples * 1000000000ULL /
                            audio_frame.sample_rate);
                    break;

                case PROP_SYNC_NDI_TIMESTAMP:
                    obs_audio_frame.timestamp =
                        (uint64_t)(audio_frame.timestamp * 100.0);
                    break;
            }

            obs_audio_frame.samples_per_sec = audio_frame.sample_rate;
            obs_audio_frame.format = AUDIO_FORMAT_FLOAT_PLANAR;
            obs_audio_frame.frames = audio_frame.no_samples;

            for (int i = 0; i < audio_frame.no_channels; i++) {
                obs_audio_frame.data[i] =
                    (uint8_t*)(&audio_frame.p_data[i * audio_frame.no_samples]);
            }

            obs_source_output_audio(s->source, &obs_audio_frame);
            ndiLib->NDIlib_recv_free_audio_v2(s->ndi_receiver, &audio_frame);
        }
    }

    blog(LOG_INFO, "audio thread for '%s' completed",
                obs_source_get_name(s->source));
    return nullptr;
}

void* ndi_source_poll_video(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    blog(LOG_INFO, "video thread for '%s' started",
                    obs_source_get_name(s->source));

    NDIlib_video_frame_v2_t video_frame;
    obs_source_frame obs_video_frame = {0};

    NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;
    while (s->running) {
        frame_received = ndiLib->NDIlib_recv_capture_v2(s->ndi_receiver,
            &video_frame, nullptr, nullptr, 1);

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

            switch (s->sync_mode) {
                case PROP_SYNC_INTERNAL:
                default:
                    obs_video_frame.timestamp = os_gettime_ns();
                    break;

                case PROP_SYNC_NDI_TIMESTAMP:
                    obs_video_frame.timestamp =
                        (uint64_t)(video_frame.timestamp * 100.0);
                    break;
            }

            obs_video_frame.width = video_frame.xres;
            obs_video_frame.height = video_frame.yres;
            obs_video_frame.linesize[0] = video_frame.line_stride_in_bytes;
            obs_video_frame.data[0] = video_frame.p_data;

            video_format_get_parameters(VIDEO_CS_DEFAULT, VIDEO_RANGE_DEFAULT,
                obs_video_frame.color_matrix, obs_video_frame.color_range_min,
                obs_video_frame.color_range_max);

            obs_source_output_video(s->source, &obs_video_frame);
            ndiLib->NDIlib_recv_free_video_v2(s->ndi_receiver, &video_frame);
        }
    }

    blog(LOG_INFO, "video thread for '%s' completed",
            obs_source_get_name(s->source));
    return nullptr;
}

void ndi_source_update(void* data, obs_data_t* settings) {
    struct ndi_source* s = static_cast<ndi_source*>(data);

    if(s->running) {
        s->running = false;
        pthread_join(s->video_thread, NULL);
        pthread_join(s->audio_thread, NULL);
    }
    s->running = false;
    ndiLib->NDIlib_recv_destroy(s->ndi_receiver);

    bool hwAccelEnabled = obs_data_get_bool(settings, PROP_HW_ACCEL);

    s->alpha_filter_enabled =
        obs_data_get_bool(settings, PROP_FIX_ALPHA);
    // Don't persist this value in settings
    obs_data_set_bool(settings, PROP_FIX_ALPHA, false);

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
    recv_desc.source_to_connect_to.p_ndi_name =
        obs_data_get_string(settings, PROP_SOURCE);
    recv_desc.allow_video_fields = true;
    recv_desc.color_format = NDIlib_recv_color_format_UYVY_BGRA;

    switch (obs_data_get_int(settings, PROP_BANDWIDTH)) {
        case PROP_BW_HIGHEST:
            recv_desc.bandwidth = NDIlib_recv_bandwidth_highest;
            break;
        case PROP_BW_LOWEST:
            recv_desc.bandwidth = NDIlib_recv_bandwidth_lowest;
            break;
        case PROP_BW_AUDIO_ONLY:
            recv_desc.bandwidth = NDIlib_recv_bandwidth_audio_only;
            break;
    }

    s->sync_mode = (int)obs_data_get_int(settings, PROP_SYNC);
    s->ndi_receiver = ndiLib->NDIlib_recv_create_v2(&recv_desc);
    if (s->ndi_receiver) {
        if (hwAccelEnabled) {
            NDIlib_metadata_frame_t hwAccelMetadata;
            hwAccelMetadata.p_data = "<ndi_hwaccel enabled=\"true\"/>";
            ndiLib->NDIlib_recv_send_metadata(
                s->ndi_receiver, &hwAccelMetadata);
        }

        // Important for low latency receiving
        obs_source_set_async_unbuffered(s->source, true);

        s->running = true;
        pthread_create(&s->video_thread, nullptr, ndi_source_poll_video, data);
        pthread_create(&s->audio_thread, nullptr, ndi_source_poll_audio, data);

        blog(LOG_INFO, "started A/V threads for source '%s'",
            recv_desc.source_to_connect_to.p_ndi_name);

        // Update tally status
        s->tally.on_preview = obs_source_showing(s->source);
        s->tally.on_program = obs_source_active(s->source);
        ndiLib->NDIlib_recv_set_tally(s->ndi_receiver, &s->tally);
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
    s->running = false;
    s->sync_mode = PROP_SYNC_INTERNAL;
    ndi_source_update(s, settings);
    return s;
}

void ndi_source_destroy(void* data) {
    struct ndi_source* s = static_cast<ndi_source*>(data);
    s->running = false;
    pthread_join(s->video_thread, NULL);
    pthread_join(s->audio_thread, NULL);
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
