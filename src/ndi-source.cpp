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

#include <util/platform.h>
#include <util/threading.h>

#include <QDesktopServices>
#include <QUrl>

#include <thread>

#define PROP_SOURCE "ndi_source_name"
#define PROP_BANDWIDTH "ndi_bw_mode"
#define PROP_BEHAVIOR "ndi_behavior"
#define PROP_BEHAVIOR_LASTFRAME "ndi_behavior_lastframe"
#define PROP_SYNC "ndi_sync"
#define PROP_FRAMESYNC "ndi_framesync"
#define PROP_HW_ACCEL "ndi_recv_hw_accel"
#define PROP_FIX_ALPHA "ndi_fix_alpha_blending"
#define PROP_YUV_RANGE "yuv_range"
#define PROP_YUV_COLORSPACE "yuv_colorspace"
#define PROP_LATENCY "latency"
#define PROP_AUDIO "ndi_audio"
#define PROP_PTZ "ndi_ptz"
#define PROP_PAN "ndi_pan"
#define PROP_TILT "ndi_tilt"
#define PROP_ZOOM "ndi_zoom"

#define PROP_BW_UNDEFINED -1
#define PROP_BW_HIGHEST 0
#define PROP_BW_LOWEST 1
#define PROP_BW_AUDIO_ONLY 2

#define PROP_BEHAVIOR_DISCONNECT "disconnect"
#define PROP_BEHAVIOR_KEEP "keep"

// sync mode "Internal" got removed
#define PROP_SYNC_INTERNAL 0
#define PROP_SYNC_NDI_TIMESTAMP 1
#define PROP_SYNC_NDI_SOURCE_TIMECODE 2

#define PROP_YUV_RANGE_PARTIAL 1
#define PROP_YUV_RANGE_FULL 2

#define PROP_YUV_SPACE_BT601 1
#define PROP_YUV_SPACE_BT709 2

#define PROP_LATENCY_UNDEFINED -1
#define PROP_LATENCY_NORMAL 0
#define PROP_LATENCY_LOW 1
#define PROP_LATENCY_LOWEST 2

enum behavior_type {
	BEHAVIOR_DISCONNECT,
	BEHAVIOR_KEEP,
};

extern NDIlib_find_instance_t ndi_finder;

typedef struct ptz_t {
	bool enabled;
	float pan;
	float tilt;
	float zoom;

	ptz_t(bool enabled_ = false, float pan_ = 0.0f, float tilt_ = 0.0f,
	      float zoom_ = 0.0f)
		: enabled(enabled_),
		  pan(pan_),
		  tilt(tilt_),
		  zoom(zoom_)
	{
	}
} ptz_t;

typedef struct ndi_source_config_t {
	char *ndi_receiver_name;
	const char *ndi_source_name;
	int bandwidth;
	enum behavior_type behavior;
	bool remember_last_frame;
	int sync_mode;
	bool framesync_enabled;
	bool hw_accel_enabled;
	video_range_type yuv_range;
	video_colorspace yuv_colorspace;
	int latency;
	bool audio_enabled;
	ptz_t ptz;
	NDIlib_tally_t tally;

	ndi_source_config_t()
	{
		memset(this, 0, sizeof(*this));
		bandwidth = PROP_BW_UNDEFINED;
		latency = PROP_LATENCY_UNDEFINED;
	}
} ndi_source_config_t;

typedef struct ndi_source_t {
	obs_source_t *obs_source;
	ndi_source_config_t config;

	bool running;
	pthread_t av_thread;

	ndi_source_t()
		: obs_source(nullptr),
		  config(),
		  running(false),
		  av_thread()
	{
	}
} ndi_source_t;

static obs_source_t *find_filter_by_id(obs_source_t *context, const char *id)
{
	if (!context)
		return nullptr;

	typedef struct {
		const char *query;
		obs_source_t *result;
	} search_context_t;

	search_context_t filter_search = {};
	filter_search.query = id;
	filter_search.result = nullptr;

	obs_source_enum_filters(
		context,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			search_context_t *filter_search_ =
				static_cast<search_context_t *>(param);
			const char *obs_source_id = obs_source_get_id(filter);
			if (strcmp(obs_source_id, filter_search_->query) == 0) {
				obs_source_get_ref(filter);
				filter_search_->result = filter;
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

const char *ndi_source_getname(void *)
{
	return obs_module_text("NDIPlugin.NDISourceName");
}

obs_properties_t *ndi_source_getproperties(void *)
{
	obs_log(LOG_INFO, "+ndi_source_getproperties()");

	obs_properties_t *props = obs_properties_create();

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

	obs_property_t *p = obs_properties_add_list(
		props, PROP_BEHAVIOR,
		obs_module_text("NDIPlugin.SourceProps.Behavior"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(
		p, obs_module_text("NDIPlugin.SourceProps.Behavior.Keep"),
		PROP_BEHAVIOR_KEEP);
	obs_property_list_add_string(
		p, obs_module_text("NDIPlugin.SourceProps.Behavior.Disconnect"),
		PROP_BEHAVIOR_DISCONNECT);

	obs_properties_add_bool(
		props, PROP_BEHAVIOR_LASTFRAME,
		obs_module_text("NDIPlugin.SourceProps.BehaviorLastFrame"));

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
		bw_modes, [](obs_properties_t *props_, obs_property_t *,
			     obs_data_t *settings_) {
			bool is_audio_only =
				(obs_data_get_int(settings_, PROP_BANDWIDTH) ==
				 PROP_BW_AUDIO_ONLY);

			obs_property_t *yuv_range =
				obs_properties_get(props_, PROP_YUV_RANGE);
			obs_property_t *yuv_colorspace =
				obs_properties_get(props_, PROP_YUV_COLORSPACE);

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

	obs_properties_add_bool(props, PROP_FRAMESYNC,
				obs_module_text("NDIPlugin.NDIFrameSync"));

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

	obs_properties_t *group_ptz = obs_properties_create();
	obs_properties_add_float_slider(
		group_ptz, PROP_PAN,
		obs_module_text("NDIPlugin.SourceProps.Pan"), -1.0, 1.0, 0.001);
	obs_properties_add_float_slider(
		group_ptz, PROP_TILT,
		obs_module_text("NDIPlugin.SourceProps.Tilt"), -1.0, 1.0,
		0.001);
	obs_properties_add_float_slider(
		group_ptz, PROP_ZOOM,
		obs_module_text("NDIPlugin.SourceProps.Zoom"), 0.0, 1.0, 0.001);
	obs_properties_add_group(props, PROP_PTZ,
				 obs_module_text("NDIPlugin.SourceProps.PTZ"),
				 OBS_GROUP_CHECKABLE, group_ptz);

	auto group_ndi = obs_properties_create();
	obs_properties_add_button(
		group_ndi, "ndi_website", NDI_OFFICIAL_WEB_URL,
		[](obs_properties_t *, obs_property_t *, void *) {
			QDesktopServices::openUrl(
				QUrl(rehostUrl(PLUGIN_REDIRECT_NDI_WEB_URL)));
			return false;
		});
	obs_properties_add_group(props, "ndi", "NDI®", OBS_GROUP_NORMAL,
				 group_ndi);

	obs_log(LOG_INFO, "-ndi_source_getproperties()");

	return props;
}

void ndi_source_getdefaults(obs_data_t *settings)
{
	obs_log(LOG_INFO, "+ndi_source_getdefaults(…)");
	obs_data_set_default_int(settings, PROP_BANDWIDTH, PROP_BW_HIGHEST);
	obs_data_set_default_string(settings, PROP_BEHAVIOR,
				    PROP_BEHAVIOR_KEEP);
	obs_data_set_default_bool(settings, PROP_BEHAVIOR_LASTFRAME, true);
	obs_data_set_default_int(settings, PROP_SYNC,
				 PROP_SYNC_NDI_SOURCE_TIMECODE);
	obs_data_set_default_int(settings, PROP_YUV_RANGE,
				 PROP_YUV_RANGE_PARTIAL);
	obs_data_set_default_int(settings, PROP_YUV_COLORSPACE,
				 PROP_YUV_SPACE_BT709);
	obs_data_set_default_int(settings, PROP_LATENCY, PROP_LATENCY_NORMAL);
	obs_data_set_default_bool(settings, PROP_AUDIO, true);
	obs_log(LOG_INFO, "-ndi_source_getdefaults(…)");
}

void deactivate_source_output_video_texture(obs_source_t *obs_source)
{
	// Per https://docs.obsproject.com/reference-sources#c.obs_source_output_video
	// ```
	// void obs_source_output_video(obs_source_t *source, const struct obs_source_frame *frame)
	// Outputs asynchronous video data. Set to NULL to deactivate the texture.
	// ```
	obs_source_output_video(obs_source, NULL);
}

void ndi_source_thread_process_audio2(ndi_source_config_t *config,
				      NDIlib_audio_frame_v2_t *ndi_audio_frame2,
				      obs_source_t *obs_source,
				      obs_source_audio *obs_audio_frame);

void ndi_source_thread_process_audio3(ndi_source_config_t *config,
				      NDIlib_audio_frame_v3_t *ndi_audio_frame3,
				      obs_source_t *obs_source,
				      obs_source_audio *obs_audio_frame);

void ndi_source_thread_process_video2(ndi_source_config_t *config,
				      NDIlib_video_frame_v2_t *ndi_video_frame2,
				      obs_source *obs_source,
				      obs_source_frame *obs_video_frame);

void *ndi_source_thread(void *data)
{
	auto s = (ndi_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_INFO, "'%s' +ndi_source_thread(…)", obs_source_name);

	ndi_source_config_t config_most_recent;
	ndi_source_config_t config_last_used;

	obs_source_audio obs_audio_frame = {};
	obs_source_frame obs_video_frame = {};

	NDIlib_recv_create_v3_t recv_desc;
	recv_desc.allow_video_fields = true;

	NDIlib_recv_instance_t ndi_receiver = nullptr;
	NDIlib_video_frame_v2_t video_frame2;

	NDIlib_metadata_frame_t metadata_frame;
	NDIlib_framesync_instance_t ndi_frame_sync = nullptr;
	NDIlib_audio_frame_v2_t audio_frame2;
	int64_t timestamp_audio = 0;
	int64_t timestamp_video = 0;

	NDIlib_audio_frame_v3_t audio_frame3;
	NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;

	bool reset_ndi_receiver = true;

	//
	// Main NDI receiver loop: BEGIN
	//
	while (s->running) {

		// semi-atomic not *TOO* heavy bit copy "snapshot"
		config_most_recent = s->config;

		//
		// Check for changes that require resetting ndi_receiver: BEGIN
		//

		// Fast pointer comparison is fine here; no need for slow content comparison
		if (config_most_recent.ndi_receiver_name !=
		    config_last_used.ndi_receiver_name) {
			config_last_used.ndi_receiver_name =
				config_most_recent.ndi_receiver_name;

			// If config.ndi_receiver_name changed, then so did obs_source_name
			obs_source_name = obs_source_get_name(s->obs_source);

			reset_ndi_receiver = true;

			recv_desc.p_ndi_recv_name =
				config_most_recent.ndi_receiver_name;
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: ndi_receiver_name changed; Setting recv_desc.p_ndi_recv_name='%s'",
				obs_source_name, //
				recv_desc.p_ndi_recv_name);
		}

		// Fast pointer comparison is fine here; no need for slow content comparison
		if (config_most_recent.ndi_source_name !=
		    config_last_used.ndi_source_name) {
			config_last_used.ndi_source_name =
				config_most_recent.ndi_source_name;

			reset_ndi_receiver = true;

			recv_desc.source_to_connect_to.p_ndi_name =
				config_most_recent.ndi_source_name;
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: ndi_source_name changed; Setting recv_desc.source_to_connect_to.p_ndi_name='%s'",
				obs_source_name, //
				recv_desc.source_to_connect_to.p_ndi_name);
		}

		if (config_most_recent.bandwidth !=
		    config_last_used.bandwidth) {
			config_last_used.bandwidth =
				config_most_recent.bandwidth;

			reset_ndi_receiver = true;

			switch (config_most_recent.bandwidth) {
			case PROP_BW_HIGHEST:
			default:
				recv_desc.bandwidth =
					NDIlib_recv_bandwidth_highest;
				break;
			case PROP_BW_LOWEST:
				recv_desc.bandwidth =
					NDIlib_recv_bandwidth_lowest;
				break;
			case PROP_BW_AUDIO_ONLY:
				recv_desc.bandwidth =
					NDIlib_recv_bandwidth_audio_only;
				break;
			}
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: bandwidth changed; Setting recv_desc.bandwidth='%d'",
				obs_source_name, //
				recv_desc.bandwidth);
		}

		if (config_most_recent.latency != config_last_used.latency) {
			config_last_used.latency = config_most_recent.latency;

			reset_ndi_receiver = true;

			if (config_most_recent.latency == PROP_LATENCY_NORMAL)
				recv_desc.color_format =
					NDIlib_recv_color_format_UYVY_BGRA;
			else
				recv_desc.color_format =
					NDIlib_recv_color_format_fastest;
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: latency changed; Setting recv_desc.color_format='%d'",
				obs_source_name, //
				recv_desc.color_format);
		}

		if (config_most_recent.framesync_enabled !=
		    config_last_used.framesync_enabled) {
			config_last_used.framesync_enabled =
				config_most_recent.framesync_enabled;

			reset_ndi_receiver = true;

			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: framesync changed to %s",
				obs_source_name, //
				config_most_recent.framesync_enabled
					? "enabled"
					: "disabled");
		}
		//
		// Check for changes that require resetting ndi_receiver: END
		//

		//
		// Conditionally reset NDI receiver: BEGIN
		//
		if (reset_ndi_receiver) {
			reset_ndi_receiver = false;

			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: reset_ndi_receiver: Resetting NDI receiver…",
				obs_source_name);

			if (ndi_frame_sync) {
				ndiLib->framesync_destroy(ndi_frame_sync);
				ndi_frame_sync = nullptr;
			}

			if (ndi_receiver) {
#if 1
				obs_log(LOG_INFO,
					"'%s' ndi_source_thread: reset_ndi_receiver: ndiLib->recv_destroy(ndi_receiver)",
					obs_source_name);
#endif
				ndiLib->recv_destroy(ndi_receiver);
				ndi_receiver = nullptr;
			}
#if 1
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: reset_ndi_receiver: recv_desc = { p_ndi_recv_name='%s', source_to_connect_to.p_ndi_name='%s' }",
				obs_source_name, //
				recv_desc.p_ndi_recv_name,
				recv_desc.source_to_connect_to.p_ndi_name);
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: reset_ndi_receiver: +ndi_receiver = ndiLib->recv_create_v3(&recv_desc)",
				obs_source_name);
#endif
			ndi_receiver = ndiLib->recv_create_v3(&recv_desc);
#if 1
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: reset_ndi_receiver: -ndi_receiver = ndiLib->recv_create_v3(&recv_desc)",
				obs_source_name);
#endif
			if (!ndi_receiver) {
				obs_log(LOG_ERROR,
					"'%s' ndi_source_thread: reset_ndi_receiver: Cannot create ndi_receiver for NDI source '%s'",
					obs_source_name, //
					recv_desc.source_to_connect_to
						.p_ndi_name);
				break;
			}

			// Deactivate the source output video texture when using Audio only
			if (recv_desc.bandwidth ==
			    NDIlib_recv_bandwidth_audio_only) {
				obs_log(LOG_INFO,
					"'%s' ndi_source_thread: reset_ndi_receiver: Audio Only: Deactivate source output video texture",
					obs_source_name);
				deactivate_source_output_video_texture(
					s->obs_source);
			}

			// Apply Framesync Settings
			if (config_most_recent.framesync_enabled) {
				timestamp_audio = 0;
				timestamp_video = 0;

#if 1
				obs_log(LOG_INFO,
					"'%s' ndi_source_thread: +ndi_frame_sync = ndiLib->framesync_create(ndi_receiver)",
					obs_source_name);
#endif
				ndi_frame_sync =
					ndiLib->framesync_create(ndi_receiver);
#if 1
				obs_log(LOG_INFO,
					"'%s' ndi_source_thread: -ndi_frame_sync = ndiLib->framesync_create(ndi_receiver); ndi_frame_sync=%p",
					obs_source_name, //
					ndi_frame_sync);
#endif
				if (!ndi_frame_sync) {
					obs_log(LOG_ERROR,
						"'%s' ndi_source_thread: Cannot create ndi_frame_sync for NDI source '%s'",
						obs_source_name, //
						recv_desc.source_to_connect_to
							.p_ndi_name);
					break;
				}
			}
		}
		//
		// Conditionally reset NDI receiver: END
		//

		//
		// Now that we have a stable usable ndi_receiver,
		// check if there are any connections.
		// If not then micro-pause and restart the loop.
		//
		if (ndiLib->recv_get_no_connections(ndi_receiver) == 0) {
#if 0
			obs_log(LOG_INFO,
			     "'%s' ndi_source_thread: No connection; sleep and restart loop",
			     obs_source_name);
#endif
			//blog(LOG_INFO, "s");//leep");
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
			continue;
		}

		//
		// Change hardware acceleration
		//
		if (config_most_recent.hw_accel_enabled !=
		    config_last_used.hw_accel_enabled) {
			config_last_used.hw_accel_enabled =
				config_most_recent.hw_accel_enabled;

			NDIlib_metadata_frame_t hwAccelMetadata;
			hwAccelMetadata.p_data =
				config_most_recent.hw_accel_enabled
					? (char *)"<ndi_hwaccel enabled=\"true\"/>"
					: (char *)"<ndi_hwaccel enabled=\"false\"/>";
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: hw_accel_enabled changed; Sending NDI metadata '%s'",
				obs_source_name, //
				hwAccelMetadata.p_data);
			ndiLib->recv_send_metadata(ndi_receiver,
						   &hwAccelMetadata);
		}

		//
		// Change PTZ
		//
		if (config_most_recent.ptz.enabled) {
			const static float tollerance = 0.001f;
			if (fabs(config_most_recent.ptz.pan -
				 config_last_used.ptz.pan) > tollerance ||
			    fabs(config_most_recent.ptz.tilt -
				 config_last_used.ptz.tilt) > tollerance ||
			    fabs(config_most_recent.ptz.zoom -
				 config_last_used.ptz.zoom) > tollerance) {
				if (ndiLib->recv_ptz_is_supported(
					    ndi_receiver)) {
					config_last_used.ptz =
						config_most_recent.ptz;

					obs_log(LOG_INFO,
						"'%s' ndi_source_thread: ptz changed; Sending PTZ pan=%f, tilt=%f, zoom=%f",
						obs_source_name, //
						config_most_recent.ptz.pan,
						config_most_recent.ptz.tilt,
						config_most_recent.ptz.zoom);
					ndiLib->recv_ptz_pan_tilt(
						ndi_receiver,
						config_most_recent.ptz.pan,
						config_most_recent.ptz.tilt);
					ndiLib->recv_ptz_zoom(
						ndi_receiver,
						config_most_recent.ptz.zoom);
				}
			}
		}

		//
		// Change Tally
		//
		if (config_most_recent.tally.on_preview !=
			    config_last_used.tally.on_preview ||
		    config_most_recent.tally.on_program !=
			    config_last_used.tally.on_program) {
			config_last_used.tally = config_most_recent.tally;

			obs_log(LOG_INFO,
				"'%s' ndi_source_thread: tally changed; Sending tally on_preview=%d, on_program=%d",
				obs_source_name, //
				config_most_recent.tally.on_preview,
				config_most_recent.tally.on_program);
			ndiLib->recv_set_tally(ndi_receiver,
					       &config_most_recent.tally);
		}

		if (ndi_frame_sync) {
			//
			// ndi_frame_sync
			//

			//
			// AUDIO
			//
			audio_frame2 = {};
			ndiLib->framesync_capture_audio(
				ndi_frame_sync, &audio_frame2,
				0, // "Your desired sample rate. 0 for “use source”."
				0, // "Your desired channel count. 0 for “use source”."
				1024);
			if (audio_frame2.p_data &&
			    (audio_frame2.timestamp > timestamp_audio)) {
				//blog(LOG_INFO, "a");//udio_frame");
				timestamp_audio = audio_frame2.timestamp;
				ndi_source_thread_process_audio2(
					&config_most_recent, &audio_frame2,
					s->obs_source, &obs_audio_frame);
			}
			ndiLib->framesync_free_audio(ndi_frame_sync,
						     &audio_frame2);

			//
			// VIDEO
			//
			video_frame2 = {};
			ndiLib->framesync_capture_video(
				ndi_frame_sync, &video_frame2,
				NDIlib_frame_format_type_progressive);
			if (video_frame2.p_data &&
			    (video_frame2.timestamp > timestamp_video)) {
				//blog(LOG_INFO, "v");//ideo_frame");
				timestamp_video = video_frame2.timestamp;
				ndi_source_thread_process_video2(
					&config_most_recent, &video_frame2,
					s->obs_source, &obs_video_frame);
			}
			ndiLib->framesync_free_video(ndi_frame_sync,
						     &video_frame2);

			// TODO: More accurate sleep that subtracts the duration of this loop iteration?
			std::this_thread::sleep_for(
				std::chrono::milliseconds(5));
		} else {
			//
			// !ndi_frame_sync
			//
			frame_received = ndiLib->recv_capture_v3(ndi_receiver,
								 &video_frame2,
								 &audio_frame3,
								 nullptr, 100);

			if (frame_received == NDIlib_frame_type_audio) {
				//
				// AUDIO
				//
				//blog(LOG_INFO, "a");//udio_frame");
				ndi_source_thread_process_audio3(
					&config_most_recent, &audio_frame3,
					s->obs_source, &obs_audio_frame);

				ndiLib->recv_free_audio_v3(ndi_receiver,
							   &audio_frame3);
				continue;
			}

			if (frame_received == NDIlib_frame_type_video) {
				//
				// VIDEO
				//
				//blog(LOG_INFO, "v");//ideo_frame");
				ndi_source_thread_process_video2(
					&config_most_recent, &video_frame2,
					s->obs_source, &obs_video_frame);

				ndiLib->recv_free_video_v2(ndi_receiver,
							   &video_frame2);
				continue;
			}
		}
	}
	//
	// Main NDI receiver loop: END
	//

	if (ndi_frame_sync) {
		ndiLib->framesync_destroy(ndi_frame_sync);
		ndi_frame_sync = nullptr;
	}

	if (ndi_receiver) {
		ndiLib->recv_destroy(ndi_receiver);
		ndi_receiver = nullptr;
	}

	obs_log(LOG_INFO, "'%s' -ndi_source_thread(…)", obs_source_name);

	return nullptr;
}

void ndi_source_thread_process_audio2(ndi_source_config_t *config,
				      NDIlib_audio_frame_v2_t *ndi_audio_frame2,
				      obs_source_t *obs_source,
				      obs_source_audio *obs_audio_frame)
{
	if (!config->audio_enabled) {
		return;
	}

	const int channelCount = ndi_audio_frame2->no_channels > 8
					 ? 8
					 : ndi_audio_frame2->no_channels;

	obs_audio_frame->speakers = channel_count_to_layout(channelCount);

	switch (config->sync_mode) {
	case PROP_SYNC_NDI_TIMESTAMP:
		obs_audio_frame->timestamp =
			(uint64_t)(ndi_audio_frame2->timestamp * 100);
		break;

	case PROP_SYNC_NDI_SOURCE_TIMECODE:
		obs_audio_frame->timestamp =
			(uint64_t)(ndi_audio_frame2->timecode * 100);
		break;
	}

	obs_audio_frame->samples_per_sec = ndi_audio_frame2->sample_rate;
	obs_audio_frame->format = AUDIO_FORMAT_FLOAT_PLANAR;
	obs_audio_frame->frames = ndi_audio_frame2->no_samples;
	for (int i = 0; i < channelCount; ++i) {
		obs_audio_frame->data[i] =
			(uint8_t *)ndi_audio_frame2->p_data +
			(i * ndi_audio_frame2->channel_stride_in_bytes);
	}

	obs_source_output_audio(obs_source, obs_audio_frame);
}

void ndi_source_thread_process_audio3(ndi_source_config_t *config,
				      NDIlib_audio_frame_v3_t *ndi_audio_frame3,
				      obs_source_t *obs_source,
				      obs_source_audio *obs_audio_frame)
{
	if (!config->audio_enabled) {
		return;
	}

	const int channelCount = ndi_audio_frame3->no_channels > 8
					 ? 8
					 : ndi_audio_frame3->no_channels;

	obs_audio_frame->speakers = channel_count_to_layout(channelCount);

	switch (config->sync_mode) {
	case PROP_SYNC_NDI_TIMESTAMP:
		obs_audio_frame->timestamp =
			(uint64_t)(ndi_audio_frame3->timestamp * 100);
		break;

	case PROP_SYNC_NDI_SOURCE_TIMECODE:
		obs_audio_frame->timestamp =
			(uint64_t)(ndi_audio_frame3->timecode * 100);
		break;
	}

	obs_audio_frame->samples_per_sec = ndi_audio_frame3->sample_rate;
	obs_audio_frame->format = AUDIO_FORMAT_FLOAT_PLANAR;
	obs_audio_frame->frames = ndi_audio_frame3->no_samples;
	for (int i = 0; i < channelCount; ++i) {
		obs_audio_frame->data[i] =
			(uint8_t *)ndi_audio_frame3->p_data +
			(i * ndi_audio_frame3->channel_stride_in_bytes);
	}

	obs_source_output_audio(obs_source, obs_audio_frame);
}

void ndi_source_thread_process_video2(ndi_source_config_t *config,
				      NDIlib_video_frame_v2_t *ndi_video_frame,
				      obs_source *obs_source,
				      obs_source_frame *obs_video_frame)
{
	switch (ndi_video_frame->FourCC) {
	case NDIlib_FourCC_type_BGRA:
		obs_video_frame->format = VIDEO_FORMAT_BGRA;
		break;

	case NDIlib_FourCC_type_BGRX:
		obs_video_frame->format = VIDEO_FORMAT_BGRX;
		break;

	case NDIlib_FourCC_type_RGBA:
	case NDIlib_FourCC_type_RGBX:
		obs_video_frame->format = VIDEO_FORMAT_RGBA;
		break;

	case NDIlib_FourCC_type_UYVY:
	case NDIlib_FourCC_type_UYVA:
		obs_video_frame->format = VIDEO_FORMAT_UYVY;
		break;

	case NDIlib_FourCC_type_I420:
		obs_video_frame->format = VIDEO_FORMAT_I420;
		break;

	case NDIlib_FourCC_type_NV12:
		obs_video_frame->format = VIDEO_FORMAT_NV12;
		break;

	default:
		obs_log(LOG_WARNING,
			"warning: unsupported video pixel format: %d",
			ndi_video_frame->FourCC);
		break;
	}

	switch (config->sync_mode) {
	case PROP_SYNC_NDI_TIMESTAMP:
		obs_video_frame->timestamp =
			(uint64_t)(ndi_video_frame->timestamp * 100);
		break;

	case PROP_SYNC_NDI_SOURCE_TIMECODE:
		obs_video_frame->timestamp =
			(uint64_t)(ndi_video_frame->timecode * 100);
		break;
	}

	obs_video_frame->width = ndi_video_frame->xres;
	obs_video_frame->height = ndi_video_frame->yres;
	obs_video_frame->linesize[0] = ndi_video_frame->line_stride_in_bytes;
	obs_video_frame->data[0] = ndi_video_frame->p_data;

	video_format_get_parameters(config->yuv_colorspace, config->yuv_range,
				    obs_video_frame->color_matrix,
				    obs_video_frame->color_range_min,
				    obs_video_frame->color_range_max);

	obs_source_output_video(obs_source, obs_video_frame);
}

void ndi_source_thread_start(ndi_source_t *s)
{
	s->running = true;
	pthread_create(&s->av_thread, nullptr, ndi_source_thread, s);
	obs_log(LOG_INFO,
		"'%s' ndi_source_thread_start: Started A/V ndi_source_thread for NDI source '%s'",
		obs_source_get_name(s->obs_source), s->config.ndi_source_name);
}

void ndi_source_thread_stop(ndi_source_t *s)
{
	if (s->running) {
		s->running = false;
		pthread_join(s->av_thread, NULL);
		auto obs_source = s->obs_source;
		auto obs_source_name = obs_source_get_name(obs_source);
		obs_log(LOG_INFO,
			"'%s' ndi_source_thread_stop: Stopped A/V ndi_source_thread for NDI source '%s'",
			obs_source_name, s->config.ndi_source_name);

		if (!s->config.remember_last_frame) {
			obs_log(LOG_INFO,
				"'%s' ndi_source_thread_stop: Behavior Blank Frame: Deactivate source output video texture",
				obs_source_name);
			deactivate_source_output_video_texture(obs_source);
		}
	}
}

void ndi_source_update(void *data, obs_data_t *settings)
{
	auto s = (ndi_source_t *)data;
	auto obs_source = s->obs_source;
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_INFO, "'%s' +ndi_source_update(…)", obs_source_name);

	s->config.ndi_source_name = obs_data_get_string(settings, PROP_SOURCE);
	s->config.bandwidth = (int)obs_data_get_int(settings, PROP_BANDWIDTH);

	const char *behavior = obs_data_get_string(settings, PROP_BEHAVIOR);
	if (strcmp(behavior, PROP_BEHAVIOR_DISCONNECT) == 0) {
		s->config.behavior = BEHAVIOR_DISCONNECT;
	} else {
		s->config.behavior = BEHAVIOR_KEEP;
	}

	s->config.remember_last_frame =
		obs_data_get_bool(settings, PROP_BEHAVIOR_LASTFRAME);

	s->config.sync_mode = (int)obs_data_get_int(settings, PROP_SYNC);
	// if sync mode is set to the unsupported "Internal" mode, set it
	// to "Source Timing" mode and apply that change to the settings data
	if (s->config.sync_mode == PROP_SYNC_INTERNAL) {
		s->config.sync_mode = PROP_SYNC_NDI_SOURCE_TIMECODE;
		obs_data_set_int(settings, PROP_SYNC,
				 PROP_SYNC_NDI_SOURCE_TIMECODE);
	}

	s->config.framesync_enabled =
		obs_data_get_bool(settings, PROP_FRAMESYNC);

	s->config.hw_accel_enabled = obs_data_get_bool(settings, PROP_HW_ACCEL);

	bool alpha_filter_enabled = obs_data_get_bool(settings, PROP_FIX_ALPHA);
	// Prevent duplicate filters by not persisting this value in settings
	obs_data_set_bool(settings, PROP_FIX_ALPHA, false);
	if (alpha_filter_enabled) {
		obs_source_t *existing_filter =
			find_filter_by_id(obs_source, OBS_NDI_ALPHA_FILTER_ID);
		if (!existing_filter) {
			obs_source_t *new_filter = obs_source_create(
				OBS_NDI_ALPHA_FILTER_ID,
				obs_module_text(
					"NDIPlugin.PremultipliedAlphaFilterName"),
				nullptr, nullptr);
			obs_source_filter_add(obs_source, new_filter);
			obs_source_release(new_filter);
		}
	}

	s->config.yuv_range = prop_to_range_type(
		(int)obs_data_get_int(settings, PROP_YUV_RANGE));
	s->config.yuv_colorspace = prop_to_colorspace(
		(int)obs_data_get_int(settings, PROP_YUV_COLORSPACE));

	s->config.latency = (int)obs_data_get_int(settings, PROP_LATENCY);
	// Disable OBS buffering only for "Lowest" latency mode
	const bool is_unbuffered = (s->config.latency == PROP_LATENCY_LOWEST);
	obs_source_set_async_unbuffered(obs_source, is_unbuffered);

	s->config.audio_enabled = obs_data_get_bool(settings, PROP_AUDIO);
	obs_source_set_audio_active(obs_source, s->config.audio_enabled);

	bool ptz_enabled = obs_data_get_bool(settings, PROP_PTZ);
	float pan = (float)obs_data_get_double(settings, PROP_PAN);
	float tilt = (float)obs_data_get_double(settings, PROP_TILT);
	float zoom = (float)obs_data_get_double(settings, PROP_ZOOM);
	s->config.ptz = ptz_t(ptz_enabled, pan, tilt, zoom);

	// Update tally status
	auto config = Config::Current();
	s->config.tally.on_preview = config->TallyPreviewEnabled &&
				     obs_source_showing(obs_source);
	s->config.tally.on_program = config->TallyProgramEnabled &&
				     obs_source_active(obs_source);

	if (strlen(s->config.ndi_source_name) == 0) {
		obs_log(LOG_INFO,
			"'%s' ndi_source_update: No NDI Source defined; Requesting Source Thread Stop.",
			obs_source_name);
		ndi_source_thread_stop(s);
	} else {
		obs_log(LOG_INFO, "'%s' ndi_source_update: NDI Source defined.",
			obs_source_name);

		if (!s->running) {
			//
			// Thread is not running; start it if either:
			// 1. the source is active
			//    -or-
			// 2. the behavior property is set to keep the NDI receiver running
			//
			if (obs_source_active(obs_source) ||
			    s->config.behavior == BEHAVIOR_KEEP) {

				if (s->config.bandwidth ==
				    NDIlib_recv_bandwidth_audio_only) {
					obs_log(LOG_INFO,
						"'%s' ndi_source_update: Audio Only: Deactivate source output video texture",
						obs_source_name);
					deactivate_source_output_video_texture(
						obs_source);
				}

				obs_log(LOG_INFO,
					"'%s' ndi_source_update: Requesting Source Thread Start.",
					obs_source_name);
				ndi_source_thread_start(s);
			}
		}
	}

	obs_log(LOG_INFO, "'%s' -ndi_source_update(…)", obs_source_name);
}

void ndi_source_shown(void *data)
{
	auto s = (ndi_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_INFO, "'%s' ndi_source_shown(…)", obs_source_name);
	s->config.tally.on_preview = (Config::Current())->TallyPreviewEnabled;
	if (!s->running) {
		obs_log(LOG_INFO,
			"'%s' ndi_source_shown: Requesting Source Thread Start.",
			obs_source_name);
		ndi_source_thread_start(s);
	}
}

void ndi_source_hidden(void *data)
{
	auto s = (ndi_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_INFO, "'%s' ndi_source_hidden(…)", obs_source_name);
	s->config.tally.on_preview = false;
	if (s->config.behavior == BEHAVIOR_DISCONNECT && s->running) {
		obs_log(LOG_INFO,
			"'%s' ndi_source_hidden: Requesting Source Thread Stop.",
			obs_source_name);
		ndi_source_thread_stop(s);
	}
}

void ndi_source_activated(void *data)
{
	auto s = (ndi_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_INFO, "'%s' ndi_source_activated(…)", obs_source_name);
	s->config.tally.on_program = (Config::Current())->TallyProgramEnabled;
	if (!s->running) {
		obs_log(LOG_INFO,
			"'%s' ndi_source_activated: Requesting Source Thread Start.",
			obs_source_name);
		ndi_source_thread_start(s);
	}
}

void ndi_source_deactivated(void *data)
{
	auto s = (ndi_source_t *)data;
	obs_log(LOG_INFO, "'%s' ndi_source_deactivated(…)",
		obs_source_get_name(s->obs_source));
	s->config.tally.on_program = false;
}

void new_ndi_receiver_name(const char *obs_source_name,
			   char **ndi_receiver_name)
{
	if (*ndi_receiver_name) {
		bfree(*ndi_receiver_name);
	}
	*ndi_receiver_name = bstrdup(QT_TO_UTF8(
		QString("%1 '%1'").arg(PLUGIN_DISPLAY_NAME, obs_source_name)));
	// obs_log(LOG_INFO,
	//      "'%s' new_ndi_receiver_name: ndi_receiver_name='%s'",
	//      obs_source_name, *ndi_receiver_name);
}

void ndi_source_renamed(void *data, calldata_t *);

void *ndi_source_create(obs_data_t *settings, obs_source_t *obs_source)
{
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_INFO, "'%s' +ndi_source_create(…)", obs_source_name);

	auto s = (ndi_source_t *)bzalloc(sizeof(ndi_source_t));
	s->obs_source = obs_source;
	new_ndi_receiver_name(obs_source_name, &(s->config.ndi_receiver_name));

	auto sh = obs_source_get_signal_handler(s->obs_source);
	signal_handler_connect(sh, "rename", ndi_source_renamed, s);

	ndi_source_update(s, settings);

	obs_log(LOG_INFO, "'%s' -ndi_source_create(…)", obs_source_name);

	return s;
}

void ndi_source_renamed(void *data, calldata_t *)
{
	auto s = (ndi_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	new_ndi_receiver_name(obs_source_name, &(s->config.ndi_receiver_name));
	obs_log(LOG_INFO, "'%s' ndi_source_renamed: ndi_receiver_name='%s'",
		obs_source_name, s->config.ndi_receiver_name);
}

void ndi_source_destroy(void *data)
{
	auto s = (ndi_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_INFO, "'%s' +ndi_source_destroy(…)", obs_source_name);

	signal_handler_disconnect(obs_source_get_signal_handler(s->obs_source),
				  "rename", ndi_source_renamed, s);

	ndi_source_thread_stop(s);

	if (s->config.ndi_receiver_name) {
		bfree(s->config.ndi_receiver_name);
		s->config.ndi_receiver_name = nullptr;
	}
	bfree(s);

	obs_log(LOG_INFO, "'%s' -ndi_source_destroy(…)", obs_source_name);
}

obs_source_info create_ndi_source_info()
{
	obs_source_info ndi_source_info = {};
	ndi_source_info.id = "ndi_source";
	ndi_source_info.type = OBS_SOURCE_TYPE_INPUT;
	ndi_source_info.output_flags = OBS_SOURCE_ASYNC_VIDEO |
				       OBS_SOURCE_AUDIO |
				       OBS_SOURCE_DO_NOT_DUPLICATE;

	ndi_source_info.get_name = ndi_source_getname;
	ndi_source_info.get_properties = ndi_source_getproperties;
	ndi_source_info.get_defaults = ndi_source_getdefaults;

	ndi_source_info.create = ndi_source_create;
	ndi_source_info.activate = ndi_source_activated;
	ndi_source_info.show = ndi_source_shown;
	ndi_source_info.update = ndi_source_update;
	ndi_source_info.hide = ndi_source_hidden;
	ndi_source_info.deactivate = ndi_source_deactivated;
	ndi_source_info.destroy = ndi_source_destroy;

	return ndi_source_info;
}
