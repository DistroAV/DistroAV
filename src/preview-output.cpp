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

#include "preview-output.h"

#include "plugin-main.h"

#include <util/platform.h>
#include <media-io/video-frame.h>

struct preview_output {
	bool is_running;
	QString ndi_name;
	QString ndi_groups;

	obs_source_t *current_source;
	obs_output_t *output;

	video_t *video_queue;
	audio_t *dummy_audio_queue; // unused for now
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	uint8_t *video_data;
	uint32_t video_linesize;

	obs_video_info ovi;
};

static struct preview_output context = {0};

void on_preview_scene_changed(enum obs_frontend_event event, void *param);
void render_preview_source(void *param, uint32_t cx, uint32_t cy);

void on_preview_output_started(void *data, calldata_t *)
{
	obs_log(LOG_INFO, "+on_preview_output_started()");
	Config::Current()->PreviewOutputEnabled = true;
	obs_log(LOG_INFO, "-on_preview_output_started()");
}

void on_preview_output_stopped(void *data, calldata_t *)
{
	obs_log(LOG_INFO, "+on_preview_output_stopped()");
	Config::Current()->PreviewOutputEnabled = false;
	obs_log(LOG_INFO, "-on_preview_output_stopped()");
}

void preview_output_stop()
{
	obs_log(LOG_INFO, "+preview_output_stop()");
	auto output_name = context.ndi_name.toUtf8().constData();
	if (context.is_running) {
		obs_log(LOG_INFO,
			"preview_output_stop: stopping NDI preview output '%s'",
			output_name);
		obs_output_stop(context.output);

		video_output_stop(context.video_queue);

		obs_remove_main_render_callback(render_preview_source,
						&context);
		obs_frontend_remove_event_callback(on_preview_scene_changed,
						   &context);

		obs_source_release(context.current_source);

		obs_enter_graphics();
		gs_stagesurface_destroy(context.stagesurface);
		gs_texrender_destroy(context.texrender);
		obs_leave_graphics();

		video_output_close(context.video_queue);
		audio_output_close(context.dummy_audio_queue);

		context.is_running = false;
		obs_log(LOG_INFO,
			"preview_output_stop: successfully stopped NDI preview output '%s'",
			output_name);
	} else {
		obs_log(LOG_ERROR,
			"preview_output_stop: NDI preview output '%s' is not running",
			output_name);
	}
	obs_log(LOG_INFO, "-preview_output_stop()");
}

void preview_output_start()
{
	obs_log(LOG_INFO, "+preview_output_start()");
	auto output_name = context.ndi_name.toUtf8().constData();
	if (context.output) {
		if (context.is_running) {
			preview_output_stop();
		}

		obs_log(LOG_INFO,
			"preview_output_start: starting NDI preview output '%s'",
			output_name);

		obs_get_video_info(&context.ovi);

		uint32_t width = context.ovi.base_width;
		uint32_t height = context.ovi.base_height;

		obs_enter_graphics();
		context.texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
		context.stagesurface =
			gs_stagesurface_create(width, height, GS_BGRA);
		obs_leave_graphics();

		const video_output_info *mainVOI =
			video_output_get_info(obs_get_video());
		const audio_output_info *mainAOI =
			audio_output_get_info(obs_get_audio());

		video_output_info voi = {0};
		voi.name = output_name;
		voi.format = VIDEO_FORMAT_BGRA;
		voi.width = width;
		voi.height = height;
		voi.fps_den = context.ovi.fps_den;
		voi.fps_num = context.ovi.fps_num;
		voi.cache_size = 16;
		voi.colorspace = mainVOI->colorspace;
		voi.range = mainVOI->range;

		video_output_open(&context.video_queue, &voi);

		audio_output_info aoi = {0};
		aoi.name = output_name;
		aoi.format = mainAOI->format;
		aoi.samples_per_sec = mainAOI->samples_per_sec;
		aoi.speakers = mainAOI->speakers;
		aoi.input_callback = [](void *, uint64_t, uint64_t, uint64_t *,
					uint32_t, struct audio_output_data *) {
			return false;
		};
		aoi.input_param = nullptr;

		audio_output_open(&context.dummy_audio_queue, &aoi);

		obs_frontend_add_event_callback(on_preview_scene_changed,
						&context);
		if (obs_frontend_preview_program_mode_active()) {
			context.current_source =
				obs_frontend_get_current_preview_scene();
		} else {
			context.current_source =
				obs_frontend_get_current_scene();
		}
		obs_add_main_render_callback(render_preview_source, &context);

		obs_data_t *settings = obs_output_get_settings(context.output);
		obs_data_set_string(settings, "ndi_name", output_name);
		obs_data_set_string(settings, "ndi_groups",
				    context.ndi_groups.toUtf8().constData());
		obs_output_update(context.output, settings);
		obs_data_release(settings);

		obs_output_set_media(context.output, context.video_queue,
				     context.dummy_audio_queue);

		context.is_running = obs_output_start(context.output);
		if (context.is_running) {
			obs_log(LOG_INFO,
				"preview_output_start: successfully started NDI preview output '%s'",
				output_name);
		} else {
			auto error = obs_output_get_last_error(context.output);
			obs_log(LOG_ERROR,
				"preview_output_start: failed to start NDI preview output '%s'; error='%s'",
				output_name, error);
		}

		obs_log(LOG_INFO,
			"preview_output_start: started NDI preview output");
	} else {
		obs_log(LOG_ERROR,
			"obs_logpreview_output_start: NDI preview output '%s' is not initialized",
			output_name);
	}
	obs_log(LOG_INFO, "-preview_output_start()");
}

void preview_output_deinit()
{
	obs_log(LOG_INFO, "+preview_output_deinit()");
	if (context.output) {
		preview_output_stop();

		auto output_name = context.ndi_name.toUtf8().constData();
		obs_log(LOG_INFO,
			"preview_output_deinit: releasing NDI preview output '%s'",
			output_name);

		// Stop handling remote start/stop events from obs-websocket
		auto sh = obs_output_get_signal_handler(context.output);
		signal_handler_disconnect(sh, "start",
					  on_preview_output_started, nullptr);
		signal_handler_disconnect(sh, "stop", on_preview_output_stopped,
					  nullptr);

		obs_output_release(context.output);
		context.output = nullptr;
		context.ndi_name.clear();
		context.ndi_groups.clear();
		obs_log(LOG_INFO,
			"preview_output_deinit: successfully released NDI preview output '%s'",
			output_name);
	} else {
		obs_log(LOG_INFO,
			"preview_output_deinit: NDI preview output not created");
	}

	obs_log(LOG_INFO, "-preview_output_deinit()");
}

void preview_output_init()
{
	obs_log(LOG_INFO, "+preview_output_init()");

	auto config = Config::Current();
	auto output_name = config->PreviewOutputName;
	auto output_groups = config->PreviewOutputGroups;
	auto is_enabled = config->PreviewOutputEnabled;

	if (output_name.isEmpty() || //
	    (output_name != context.ndi_name) ||
	    output_groups != context.ndi_groups) {
		preview_output_deinit();

		if (!output_name.isEmpty()) {
			auto output_name_ = output_name.toUtf8().constData();
			obs_log(LOG_INFO,
				"preview_output_init: creating NDI preview output '%s'",
				output_name_);
			obs_data_t *output_settings = obs_data_create();
			obs_data_set_string(output_settings, "ndi_name",
					    output_name_);
			obs_data_set_string(output_settings, "ndi_groups",
					    output_groups.toUtf8().constData());
			obs_data_set_bool(output_settings, "uses_audio",
					  false); // Preview has no audio
			context.output = obs_output_create("ndi_output",
							   "NDI Preview Output",
							   output_settings,
							   nullptr);
			obs_data_release(output_settings);
			if (context.output) {
				obs_log(LOG_INFO,
					"preview_output_init: successfully created NDI preview output '%s'",
					output_name_);

				// Start handling remote start/stop events from obs-websocket
				auto sh = obs_output_get_signal_handler(
					context.output);
				signal_handler_connect(
					sh, "start", on_preview_output_started,
					nullptr);
				signal_handler_connect(
					sh, "stop", on_preview_output_stopped,
					nullptr);

				context.ndi_name = output_name;
				context.ndi_groups = output_groups;
			} else {
				obs_log(LOG_ERROR,
					"preview_output_init: failed to create NDI preview output '%s'",
					output_name_);
			}
		}
	}

	if (context.is_running != is_enabled) {
		if (is_enabled) {
			preview_output_start();
		} else {
			preview_output_stop();
		}
	}

	obs_log(LOG_INFO, "-preview_output_init()");
}

void on_preview_scene_changed(enum obs_frontend_event event, void *param)
{
	obs_log(LOG_INFO, "on_preview_scene_changed(%d)", event);
	auto ctx = (struct preview_output *)param;
	switch (event) {
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		obs_source_release(ctx->current_source);
		ctx->current_source = obs_frontend_get_current_preview_scene();
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		obs_source_release(ctx->current_source);
		ctx->current_source = obs_frontend_get_current_scene();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		if (!obs_frontend_preview_program_mode_active()) {
			obs_source_release(ctx->current_source);
			ctx->current_source = obs_frontend_get_current_scene();
		}
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		obs_source_release(ctx->current_source);
		ctx->current_source = nullptr;
		break;
	default:
		break;
	}
}

void render_preview_source(void *param, uint32_t, uint32_t)
{
	auto ctx = (struct preview_output *)param;
	if (!ctx->current_source)
		return;

	uint32_t width = obs_source_get_base_width(ctx->current_source);
	uint32_t height = obs_source_get_base_height(ctx->current_source);

	gs_texrender_reset(ctx->texrender);

	if (gs_texrender_begin(ctx->texrender, width, height)) {
		struct vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f,
			 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(ctx->current_source);

		gs_blend_state_pop();
		gs_texrender_end(ctx->texrender);

		struct video_frame output_frame;
		if (video_output_lock_frame(ctx->video_queue, &output_frame, 1,
					    os_gettime_ns())) {
			gs_stage_texture(
				ctx->stagesurface,
				gs_texrender_get_texture(ctx->texrender));

			if (gs_stagesurface_map(ctx->stagesurface,
						&ctx->video_data,
						&ctx->video_linesize)) {
				uint32_t linesize = output_frame.linesize[0];
				for (uint32_t i = 0; i < ctx->ovi.base_height;
				     i++) {
					uint32_t dst_offset = linesize * i;
					uint32_t src_offset =
						ctx->video_linesize * i;
					memcpy(output_frame.data[0] +
						       dst_offset,
					       ctx->video_data + src_offset,
					       linesize);
				}

				gs_stagesurface_unmap(ctx->stagesurface);
				ctx->video_data = nullptr;
			}

			video_output_unlock_frame(ctx->video_queue);
		}
	}
}
