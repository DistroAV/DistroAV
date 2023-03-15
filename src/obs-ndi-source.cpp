/*
obs-ndi
Copyright (C) 2016-2023 Stéphane Lepin <stephane.lepin@gmail.com>

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
#include <util/platform.h>
#include <util/threading.h>
#include <chrono>
#include <thread>
#include <algorithm>

#include "obs-ndi.h"
#include "Config.h"

#define PROP_SOURCE "ndi_source_name"
#define PROP_BANDWIDTH "ndi_bw_mode"
#define PROP_HW_ACCEL "ndi_recv_hw_accel"
#define PROP_SYNC "ndi_sync"
#define PROP_FIX_ALPHA "ndi_fix_alpha_blending"
#define PROP_YUV_RANGE "yuv_range"
#define PROP_YUV_COLORSPACE "yuv_colorspace"
#define PROP_LATENCY "latency"
#define PROP_AUDIO "ndi_audio"

#define PROP_BW_HIGHEST 0
#define PROP_BW_LOWEST 1
#define PROP_BW_AUDIO_ONLY 2

// sync mode "Internal" got removed
#define PROP_SYNC_INTERNAL 0
#define PROP_SYNC_NDI_TIMESTAMP 1
#define PROP_SYNC_NDI_SOURCE_TIMECODE 2

#define PROP_YUV_RANGE_PARTIAL 1
#define PROP_YUV_RANGE_FULL 2

#define PROP_YUV_SPACE_BT601 1
#define PROP_YUV_SPACE_BT709 2

#define PROP_LATENCY_NORMAL 0
#define PROP_LATENCY_LOW 1
#define PROP_LATENCY_LOWEST 2

extern NDIlib_find_instance_t ndi_finder;


struct ndi_source {
	obs_source_t *source;
	NDIlib_recv_instance_t ndi_receiver;
	int sync_mode;
	video_range_type yuv_range;
	video_colorspace yuv_colorspace;
	pthread_t av_thread;
	bool running;
	NDIlib_tally_t tally;
	bool alpha_filter_enabled;
	bool audio_enabled;
	os_performance_token_t *perf_token;
};

static obs_source_t *find_filter_by_id(obs_source_t *context, const char *id)
{
	if (!context)
		return nullptr;

	struct search_context {
		const char *query;
		obs_source_t *result;
	};

	struct search_context filter_search = {};
	filter_search.query = id;
	filter_search.result = nullptr;

	obs_source_enum_filters(
		context,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			struct search_context *filter_search =
				(struct search_context *)param;

			const char *id = obs_source_get_id(filter);
			if (strcmp(id, filter_search->query) == 0) {
				obs_source_get_ref(filter);
				filter_search->result = filter;
			}
		},
		&filter_search);

	return filter_search.result;
}

static speaker_layout channel_count_to_layout(int channels)
{
	switch (channels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(21, 0, 0)
		return SPEAKERS_4POINT0;
#else
		return SPEAKERS_QUAD;
#endif
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

static video_colorspace prop_to_colorspace(int index)
{
	switch (index) {
	case PROP_YUV_SPACE_BT601:
		return VIDEO_CS_601;
	default:
	case PROP_YUV_SPACE_BT709:
		return VIDEO_CS_709;
	}
}

static video_range_type prop_to_range_type(int index)
{
	switch (index) {
	case PROP_YUV_RANGE_FULL:
		return VIDEO_RANGE_FULL;
	default:
	case PROP_YUV_RANGE_PARTIAL:
		return VIDEO_RANGE_PARTIAL;
	}
}

static obs_source_frame *blank_video_frame()
{
	obs_source_frame *frame =
		obs_source_frame_create(VIDEO_FORMAT_NONE, 0, 0);
	frame->timestamp = os_gettime_ns();
	return frame;
}

const char *ndi_source_getname(void *data)
{
	UNUSED_PARAMETER(data);
	return obs_module_text("NDIPlugin.NDISourceName");
}

obs_properties_t *ndi_source_getproperties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_property_t *source_list = obs_properties_add_list(
		props, PROP_SOURCE,
		obs_module_text("NDIPlugin.SourceProps.SourceName"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);

	uint32_t nbSources = 0;
	const NDIlib_source_t *sources =
		ndiLib->find_get_current_sources(ndi_finder, &nbSources);

	for (uint32_t i = 0; i < nbSources; ++i) {
		obs_property_list_add_string(source_list, sources[i].p_ndi_name,
					     sources[i].p_ndi_name);
	}

	obs_property_t *bw_modes = obs_properties_add_list(
		props, PROP_BANDWIDTH,
		obs_module_text("NDIPlugin.SourceProps.Bandwidth"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(bw_modes,
				  obs_module_text("NDIPlugin.BWMode.Highest"),
				  PROP_BW_HIGHEST);
	obs_property_list_add_int(bw_modes,
				  obs_module_text("NDIPlugin.BWMode.Lowest"),
				  PROP_BW_LOWEST);
	obs_property_list_add_int(bw_modes,
				  obs_module_text("NDIPlugin.BWMode.AudioOnly"),
				  PROP_BW_AUDIO_ONLY);

	obs_property_set_modified_callback(
		bw_modes, [](obs_properties_t *props, obs_property_t *property,
			     obs_data_t *settings) {
			UNUSED_PARAMETER(property);
			bool is_audio_only =
				(obs_data_get_int(settings, PROP_BANDWIDTH) ==
				 PROP_BW_AUDIO_ONLY);

			obs_property_t *yuv_range =
				obs_properties_get(props, PROP_YUV_RANGE);
			obs_property_t *yuv_colorspace =
				obs_properties_get(props, PROP_YUV_COLORSPACE);

			obs_property_set_visible(yuv_range, !is_audio_only);
			obs_property_set_visible(yuv_colorspace,
						 !is_audio_only);

			return true;
		});

	obs_property_t *sync_modes = obs_properties_add_list(
		props, PROP_SYNC, obs_module_text("NDIPlugin.SourceProps.Sync"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(
		sync_modes, obs_module_text("NDIPlugin.SyncMode.NDITimestamp"),
		PROP_SYNC_NDI_TIMESTAMP);
	obs_property_list_add_int(
		sync_modes,
		obs_module_text("NDIPlugin.SyncMode.NDISourceTimecode"),
		PROP_SYNC_NDI_SOURCE_TIMECODE);

	obs_properties_add_bool(
		props, PROP_HW_ACCEL,
		obs_module_text("NDIPlugin.SourceProps.HWAccel"));

	obs_properties_add_bool(
		props, PROP_FIX_ALPHA,
		obs_module_text("NDIPlugin.SourceProps.AlphaBlendingFix"));

	obs_property_t *yuv_ranges = obs_properties_add_list(
		props, PROP_YUV_RANGE,
		obs_module_text("NDIPlugin.SourceProps.ColorRange"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(
		yuv_ranges,
		obs_module_text("NDIPlugin.SourceProps.ColorRange.Partial"),
		PROP_YUV_RANGE_PARTIAL);
	obs_property_list_add_int(
		yuv_ranges,
		obs_module_text("NDIPlugin.SourceProps.ColorRange.Full"),
		PROP_YUV_RANGE_FULL);

	obs_property_t *yuv_spaces = obs_properties_add_list(
		props, PROP_YUV_COLORSPACE,
		obs_module_text("NDIPlugin.SourceProps.ColorSpace"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(yuv_spaces, "BT.709", PROP_YUV_SPACE_BT709);
	obs_property_list_add_int(yuv_spaces, "BT.601", PROP_YUV_SPACE_BT601);

	obs_property_t *latency_modes = obs_properties_add_list(
		props, PROP_LATENCY,
		obs_module_text("NDIPlugin.SourceProps.Latency"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(
		latency_modes,
		obs_module_text("NDIPlugin.SourceProps.Latency.Normal"),
		PROP_LATENCY_NORMAL);
	obs_property_list_add_int(
		latency_modes,
		obs_module_text("NDIPlugin.SourceProps.Latency.Low"),
		PROP_LATENCY_LOW);
	obs_property_list_add_int(
		latency_modes,
		obs_module_text("NDIPlugin.SourceProps.Latency.Lowest"),
		PROP_LATENCY_LOWEST);

	obs_properties_add_bool(props, PROP_AUDIO,
				obs_module_text("NDIPlugin.SourceProps.Audio"));

	obs_properties_add_button(props, "ndi_website", "NDI.NewTek.com",
				  [](obs_properties_t *pps,
				     obs_property_t *prop, void *private_data) {
					  UNUSED_PARAMETER(pps);
					  UNUSED_PARAMETER(prop);
					  UNUSED_PARAMETER(private_data);
#if defined(_WIN32)
					  ShellExecute(NULL, L"open",
						       L"http://ndi.newtek.com",
						       NULL, NULL,
						       SW_SHOWNORMAL);
#elif defined(__linux__) || defined(__APPLE__)
            (void)!system("open http://ndi.newtek.com");
#endif
					  return true;
				  });

	return props;
}

void ndi_source_getdefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, PROP_BANDWIDTH, PROP_BW_HIGHEST);
	obs_data_set_default_int(settings, PROP_SYNC,
				 PROP_SYNC_NDI_SOURCE_TIMECODE);
	obs_data_set_default_int(settings, PROP_YUV_RANGE,
				 PROP_YUV_RANGE_PARTIAL);
	obs_data_set_default_int(settings, PROP_YUV_COLORSPACE,
				 PROP_YUV_SPACE_BT709);
	obs_data_set_default_int(settings, PROP_LATENCY, PROP_LATENCY_NORMAL);
	obs_data_set_default_bool(settings, PROP_AUDIO, true);
}

void *ndi_source_poll_audio_video(void *data)
{
	auto s = (struct ndi_source *)data;

	blog(LOG_INFO, "A/V thread for '%s' started",
	     obs_source_get_name(s->source));

	NDIlib_audio_frame_v3_t audio_frame;
	obs_source_audio obs_audio_frame = {};
	NDIlib_video_frame_v2_t video_frame;
	obs_source_frame obs_video_frame = {};

	if (s->perf_token) {
		os_end_high_performance(s->perf_token);
	}
	s->perf_token = os_request_high_performance("NDI Receiver Thread");

	NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;
	while (s->running) {
		if (ndiLib->recv_get_no_connections(s->ndi_receiver) == 0) {
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
			continue;
		}

		frame_received =
			ndiLib->recv_capture_v3(s->ndi_receiver, &video_frame,
						&audio_frame, nullptr, 100);

		if (frame_received == NDIlib_frame_type_audio) {
			if (s->audio_enabled) {
				const int channelCount =
					audio_frame.no_channels > 8
						? 8
						: audio_frame.no_channels;

				obs_audio_frame.speakers =
					channel_count_to_layout(channelCount);

				switch (s->sync_mode) {
				case PROP_SYNC_NDI_TIMESTAMP:
					obs_audio_frame.timestamp = (uint64_t)(
						audio_frame.timestamp * 100);
					break;

				case PROP_SYNC_NDI_SOURCE_TIMECODE:
					obs_audio_frame.timestamp = (uint64_t)(
						audio_frame.timecode * 100);
					break;
				}

				obs_audio_frame.samples_per_sec =
					audio_frame.sample_rate;
				obs_audio_frame.format =
					AUDIO_FORMAT_FLOAT_PLANAR;
				obs_audio_frame.frames = audio_frame.no_samples;

				for (int i = 0; i < channelCount; ++i) {
					obs_audio_frame.data[i] =
						(uint8_t *)audio_frame.p_data +
						i * audio_frame.channel_stride_in_bytes;
				}

				obs_source_output_audio(s->source,
							&obs_audio_frame);
			}
			ndiLib->recv_free_audio_v3(s->ndi_receiver,
						   &audio_frame);
		}

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

			case NDIlib_FourCC_type_I420:
				obs_video_frame.format = VIDEO_FORMAT_I420;
				break;

			case NDIlib_FourCC_type_NV12:
				obs_video_frame.format = VIDEO_FORMAT_NV12;
				break;

			default:
				blog(LOG_INFO,
				     "warning: unsupported video pixel format: %d",
				     video_frame.FourCC);
				break;
			}

			switch (s->sync_mode) {
			case PROP_SYNC_NDI_TIMESTAMP:
				obs_video_frame.timestamp =
					(uint64_t)(video_frame.timestamp * 100);
				break;

			case PROP_SYNC_NDI_SOURCE_TIMECODE:
				obs_video_frame.timestamp =
					(uint64_t)(video_frame.timecode * 100);
				break;
			}

			obs_video_frame.width = video_frame.xres;
			obs_video_frame.height = video_frame.yres;
			obs_video_frame.linesize[0] =
				video_frame.line_stride_in_bytes;
			obs_video_frame.data[0] = video_frame.p_data;

			video_format_get_parameters(
				s->yuv_colorspace, s->yuv_range,
				obs_video_frame.color_matrix,
				obs_video_frame.color_range_min,
				obs_video_frame.color_range_max);

			obs_source_output_video(s->source, &obs_video_frame);
			ndiLib->recv_free_video_v2(s->ndi_receiver,
						   &video_frame);
		}
	}

	os_end_high_performance(s->perf_token);
	s->perf_token = NULL;

	blog(LOG_INFO, "audio thread for '%s' completed",
	     obs_source_get_name(s->source));
	return nullptr;
}

void ndi_source_update(void *data, obs_data_t *settings)
{
	auto s = (struct ndi_source *)data;

	if (s->running) {
		s->running = false;
		pthread_join(s->av_thread, NULL);
	}
	s->running = false;
	ndiLib->recv_destroy(s->ndi_receiver);

	bool hwAccelEnabled = obs_data_get_bool(settings, PROP_HW_ACCEL);

	s->alpha_filter_enabled = obs_data_get_bool(settings, PROP_FIX_ALPHA);
	// Don't persist this value in settings
	obs_data_set_bool(settings, PROP_FIX_ALPHA, false);

	if (s->alpha_filter_enabled) {
		obs_source_t *existing_filter =
			find_filter_by_id(s->source, OBS_NDI_ALPHA_FILTER_ID);

		if (!existing_filter) {
			obs_source_t *new_filter = obs_source_create(
				OBS_NDI_ALPHA_FILTER_ID,
				obs_module_text(
					"NDIPlugin.PremultipliedAlphaFilterName"),
				nullptr, nullptr);
			obs_source_filter_add(s->source, new_filter);
			obs_source_release(new_filter);
		}
	}

	NDIlib_recv_create_v3_t recv_desc;
	recv_desc.source_to_connect_to.p_ndi_name =
		obs_data_get_string(settings, PROP_SOURCE);
	recv_desc.allow_video_fields = true;
	if (obs_data_get_int(settings, PROP_LATENCY) == PROP_LATENCY_NORMAL)
		recv_desc.color_format = NDIlib_recv_color_format_UYVY_BGRA;
	else
		recv_desc.color_format = NDIlib_recv_color_format_fastest;

	switch (obs_data_get_int(settings, PROP_BANDWIDTH)) {
	case PROP_BW_HIGHEST:
	default:
		recv_desc.bandwidth = NDIlib_recv_bandwidth_highest;
		break;
	case PROP_BW_LOWEST:
		recv_desc.bandwidth = NDIlib_recv_bandwidth_lowest;
		break;
	case PROP_BW_AUDIO_ONLY:
		recv_desc.bandwidth = NDIlib_recv_bandwidth_audio_only;
		obs_source_output_video(s->source, blank_video_frame());
		break;
	}

	s->sync_mode = (int)obs_data_get_int(settings, PROP_SYNC);
	// if sync mode is set to the unsupported "Internal" mode, set it
	// to "Source Timing" mode and apply that change to the settings data
	if (s->sync_mode == PROP_SYNC_INTERNAL) {
		s->sync_mode = PROP_SYNC_NDI_SOURCE_TIMECODE;
		obs_data_set_int(settings, PROP_SYNC,
				 PROP_SYNC_NDI_SOURCE_TIMECODE);
	}

	s->yuv_range = prop_to_range_type(
		(int)obs_data_get_int(settings, PROP_YUV_RANGE));
	s->yuv_colorspace = prop_to_colorspace(
		(int)obs_data_get_int(settings, PROP_YUV_COLORSPACE));

	// disable OBS buffering only for "Lowest" latency mode
	const bool is_unbuffered = (obs_data_get_int(settings, PROP_LATENCY) ==
				    PROP_LATENCY_LOWEST);
	obs_source_set_async_unbuffered(s->source, is_unbuffered);

	s->audio_enabled = obs_data_get_bool(settings, PROP_AUDIO);

	s->ndi_receiver = ndiLib->recv_create_v3(&recv_desc);
	if (s->ndi_receiver) {
		if (hwAccelEnabled) {
			NDIlib_metadata_frame_t hwAccelMetadata;
			hwAccelMetadata.p_data =
				(char *)"<ndi_hwaccel enabled=\"true\"/>";
			ndiLib->recv_send_metadata(s->ndi_receiver,
						   &hwAccelMetadata);
		}

		s->running = true;
		pthread_create(&s->av_thread, nullptr,
			       ndi_source_poll_audio_video, data);

		blog(LOG_INFO, "started A/V threads for source '%s'",
		     recv_desc.source_to_connect_to.p_ndi_name);

		// Update tally status
		Config *conf = Config::Current();
		s->tally.on_preview = conf->TallyPreviewEnabled &&
				      obs_source_showing(s->source);
		s->tally.on_program = conf->TallyProgramEnabled &&
				      obs_source_active(s->source);
		ndiLib->recv_set_tally(s->ndi_receiver, &s->tally);
	} else {
		blog(LOG_ERROR, "can't create a receiver for NDI source '%s'",
		     recv_desc.source_to_connect_to.p_ndi_name);
	}
}

void ndi_source_shown(void *data)
{
	auto s = (struct ndi_source *)data;

	if (s->ndi_receiver) {
		s->tally.on_preview = (Config::Current())->TallyPreviewEnabled;
		ndiLib->recv_set_tally(s->ndi_receiver, &s->tally);
	}
}

void ndi_source_hidden(void *data)
{
	auto s = (struct ndi_source *)data;

	if (s->ndi_receiver) {
		s->tally.on_preview = false;
		ndiLib->recv_set_tally(s->ndi_receiver, &s->tally);
	}
}

void ndi_source_activated(void *data)
{
	auto s = (struct ndi_source *)data;

	if (s->ndi_receiver) {
		s->tally.on_program = (Config::Current())->TallyProgramEnabled;
		ndiLib->recv_set_tally(s->ndi_receiver, &s->tally);
	}
}

void ndi_source_deactivated(void *data)
{
	auto s = (struct ndi_source *)data;

	if (s->ndi_receiver) {
		s->tally.on_program = false;
		ndiLib->recv_set_tally(s->ndi_receiver, &s->tally);
	}
}

void *ndi_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto s = (struct ndi_source *)bzalloc(sizeof(struct ndi_source));
	s->source = source;
	s->running = false;
	s->perf_token = NULL;
	ndi_source_update(s, settings);
	return s;
}

void ndi_source_destroy(void *data)
{
	auto s = (struct ndi_source *)data;
	s->running = false;
	pthread_join(s->av_thread, NULL);
	ndiLib->recv_destroy(s->ndi_receiver);
	bfree(s);
}

struct obs_source_info create_ndi_source_info()
{
	struct obs_source_info ndi_source_info = {};
	ndi_source_info.id = "ndi_source";
	ndi_source_info.type = OBS_SOURCE_TYPE_INPUT;
	ndi_source_info.output_flags = OBS_SOURCE_ASYNC_VIDEO |
				       OBS_SOURCE_AUDIO |
				       OBS_SOURCE_DO_NOT_DUPLICATE;
	ndi_source_info.get_name = ndi_source_getname;
	ndi_source_info.get_properties = ndi_source_getproperties;
	ndi_source_info.get_defaults = ndi_source_getdefaults;
	ndi_source_info.update = ndi_source_update;
	ndi_source_info.show = ndi_source_shown;
	ndi_source_info.hide = ndi_source_hidden;
	ndi_source_info.activate = ndi_source_activated;
	ndi_source_info.deactivate = ndi_source_deactivated;
	ndi_source_info.create = ndi_source_create;
	ndi_source_info.destroy = ndi_source_destroy;

	return ndi_source_info;
}
