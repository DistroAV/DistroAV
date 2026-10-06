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

#include "canvas-output.h"

#include "plugin-main.h"

#include <QMainWindow>

#include <algorithm>

struct canvas_output {
	QString ndi_name;
	QString ndi_groups;
	QString canvas_name;
	QString last_error;
	obs_output_t *output;
	// Strong ref held for as long as the output exists, so the canvas video mix
	// cannot be freed while the output (or its end_data_capture thread) uses it.
	obs_canvas_t *canvas;
};

static struct canvas_output context = {};

QString canvas_output_last_error()
{
	return context.last_error;
}

static void on_canvas_output_started(void *, calldata_t *)
{
	obs_log(LOG_DEBUG, "+on_canvas_output_started()");
	Config::Current()->CanvasOutputEnabled = true;
	obs_log(LOG_DEBUG, "-on_canvas_output_started()");
	obs_log(LOG_INFO, "NDI Canvas Output started");
}

static void on_canvas_output_stopped(void *, calldata_t *)
{
	obs_log(LOG_DEBUG, "+on_canvas_output_stopped()");
	Config::Current()->CanvasOutputEnabled = false;
	obs_log(LOG_DEBUG, "-on_canvas_output_stopped()");
	obs_log(LOG_INFO, "NDI Canvas Output stopped");
}

static void on_canvas_removed(void *, calldata_t *)
{
	obs_log(LOG_DEBUG, "+on_canvas_removed()");
	obs_log(LOG_WARNING, "WARN-429 - Canvas '%s' was removed; stopping NDI Canvas Output '%s'",
		QT_TO_UTF8(context.canvas_name), QT_TO_UTF8(context.ndi_name));
	auto canvas_name = context.canvas_name;
	// The output must stop before the canvas mix is freed. Do not fall back to another canvas.
	// The setting stays enabled so the output restarts if the canvas comes back.
	canvas_output_deinit();
	context.last_error = QString("Canvas '%1' was removed").arg(canvas_name);
	obs_log(LOG_DEBUG, "-on_canvas_removed()");
}

static void on_canvas_created(void *, calldata_t *)
{
	// Canvases are often created by other plugins after OBS has finished loading,
	// or when a scene collection is loaded. Retry on the UI thread if not running.
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(
		main_window,
		[] {
			if (!context.output) {
				obs_log(LOG_DEBUG, "on_canvas_created: retrying NDI Canvas Output initialization");
				canvas_output_init();
			}
		},
		Qt::QueuedConnection);
}

void canvas_output_register_signals()
{
	signal_handler_connect(obs_get_signal_handler(), "canvas_create", on_canvas_created, nullptr);
}

void canvas_output_unregister_signals()
{
	signal_handler_disconnect(obs_get_signal_handler(), "canvas_create", on_canvas_created, nullptr);
}

QList<QPair<QString, QString>> canvas_output_list_canvases()
{
	QList<QPair<QString, QString>> canvases;
	// Only collect name/uuid here: the callback runs while libobs holds the canvases mutex.
	obs_enum_canvases(
		[](void *param, obs_canvas_t *canvas) {
			auto list = static_cast<QList<QPair<QString, QString>> *>(param);
			if (obs_canvas_has_video(canvas)) {
				list->append({QString::fromUtf8(obs_canvas_get_name(canvas)),
					      QString::fromUtf8(obs_canvas_get_uuid(canvas))});
			}
			return true;
		},
		&canvases);
	return canvases;
}

// Returns a strong ref or nullptr
static obs_canvas_t *resolve_canvas(const QString &uuid, const QString &name)
{
	obs_canvas_t *canvas = nullptr;
	if (!uuid.isEmpty())
		canvas = obs_get_canvas_by_uuid(QT_TO_UTF8(uuid));
	if (!canvas && !name.isEmpty())
		canvas = obs_get_canvas_by_name(QT_TO_UTF8(name));
	if (canvas && (obs_canvas_removed(canvas) || !obs_canvas_has_video(canvas))) {
		obs_canvas_release(canvas);
		canvas = nullptr;
	}
	return canvas;
}

static bool canvas_output_is_supported(obs_canvas_t *canvas)
{
	obs_log(LOG_DEBUG, "+canvas_output_is_supported()");
	obs_data_t *output_settings = obs_data_create();
	auto output = obs_output_create("test_ndi_output", "Test NDI Canvas Output", output_settings, nullptr);
	obs_data_release(output_settings);
	bool is_supported = false;

	if (output != nullptr) {
		// Check the pixel format of the selected canvas, not the main canvas
		obs_output_set_media(output, obs_canvas_get_video(canvas), obs_get_audio());
		if (!obs_output_start(output)) {
			context.last_error = obs_output_get_last_error(output);
			obs_log(LOG_DEBUG, "canvas_output_is_supported: '%s'", QT_TO_UTF8(context.last_error));
		} else {
			is_supported = true;
			obs_log(LOG_DEBUG, "canvas_output_is_supported: NDI Canvas Output is supported");
		}
		obs_output_stop(output);
		obs_output_release(output);
	} else {
		obs_log(LOG_DEBUG, "canvas_output_is_supported: NDI Canvas Output could not be created");
	}
	obs_log(LOG_DEBUG, "-canvas_output_is_supported()");
	return is_supported;
}

static void canvas_output_start()
{
	obs_log(LOG_DEBUG, "+canvas_output_start()");
	obs_log(LOG_DEBUG, "canvas_output_start: starting NDI Canvas Output '%s'", QT_TO_UTF8(context.ndi_name));

	obs_output_start(context.output);
	if (obs_output_active(context.output)) {
		obs_log(LOG_DEBUG, "canvas_output_start: successfully started NDI Canvas Output '%s'",
			QT_TO_UTF8(context.ndi_name));
		context.last_error = QString("");
	} else {
		context.last_error = obs_output_get_last_error(context.output);
		obs_log(LOG_ERROR, "ERR-431 - Failed to start NDI Canvas Output '%s'; error='%s'",
			QT_TO_UTF8(context.ndi_name), QT_TO_UTF8(context.last_error));
		// Could not start Output, still trigger it to stop.
		obs_output_stop(context.output);
	}
	obs_log(LOG_DEBUG, "-canvas_output_start()");
}

void canvas_output_deinit()
{
	obs_log(LOG_DEBUG, "+canvas_output_deinit()");

	if (context.output) {
		obs_log(LOG_DEBUG, "canvas_output_deinit: releasing NDI Canvas Output '%s'",
			QT_TO_UTF8(context.ndi_name));

		// Stop handling "remote" start/stop events (ex: from obs-websocket)
		auto sh = obs_output_get_signal_handler(context.output);
		signal_handler_disconnect(sh, "start", on_canvas_output_started, nullptr);
		signal_handler_disconnect(sh, "stop", on_canvas_output_stopped, nullptr);

		obs_output_stop(context.output);
		// Releasing joins the output's end_data_capture thread, so the canvas video is no longer used after this.
		obs_output_release(context.output);
		context.output = nullptr;
	}

	if (context.canvas) {
		signal_handler_disconnect(obs_canvas_get_signal_handler(context.canvas), "remove", on_canvas_removed,
					  nullptr);
		obs_canvas_release(context.canvas);
		context.canvas = nullptr;
	}

	context.ndi_name.clear();
	context.ndi_groups.clear();
	context.canvas_name.clear();
	obs_log(LOG_DEBUG, "-canvas_output_deinit()");
}

void canvas_output_init()
{
	obs_log(LOG_DEBUG, "+canvas_output_init()");

	auto config = Config::Current();
	auto output_name = config->CanvasOutputName;
	auto output_groups = config->CanvasOutputGroups;
	auto canvas_uuid = config->CanvasOutputCanvasUuid;
	auto canvas_name = config->CanvasOutputCanvasName;
	auto audio_track = std::clamp(config->CanvasOutputAudioTrack, 1, MAX_AUDIO_MIXES);

	canvas_output_deinit();
	context.last_error = QString("");

	if (!config->CanvasOutputEnabled || output_name.isEmpty()) {
		obs_log(LOG_DEBUG, "-canvas_output_init(): disabled");
		return;
	}

	auto canvas = resolve_canvas(canvas_uuid, canvas_name);
	if (!canvas) {
		context.last_error = canvas_name.isEmpty() ? QString("No canvas selected")
							   : QString("Canvas '%1' not found").arg(canvas_name);
		obs_log(LOG_WARNING, "WARN-427 - NDI Canvas Output '%s' not started, canvas '%s' not found",
			QT_TO_UTF8(output_name), QT_TO_UTF8(canvas_name));
		obs_log(LOG_DEBUG, "-canvas_output_init()");
		return;
	}

	if (!canvas_output_is_supported(canvas)) {
		obs_log(LOG_WARNING, "WARN-428 - NDI Canvas Output disabled, canvas '%s' format not supported",
			obs_canvas_get_name(canvas));
		obs_canvas_release(canvas);
		obs_log(LOG_DEBUG, "-canvas_output_init()");
		return;
	}

	obs_log(LOG_DEBUG, "canvas_output_init: creating NDI Canvas Output '%s' for canvas '%s', audio track %d",
		QT_TO_UTF8(output_name), obs_canvas_get_name(canvas), audio_track);
	obs_data_t *output_settings = obs_data_create();
	obs_data_set_string(output_settings, "ndi_name", QT_TO_UTF8(output_name));
	obs_data_set_string(output_settings, "ndi_groups", QT_TO_UTF8(output_groups));

	context.output = obs_output_create("ndi_output", "NDI Canvas Output", output_settings, nullptr);
	obs_data_release(output_settings);
	if (!context.output) {
		obs_log(LOG_ERROR, "ERR-432 - Failed to create NDI Canvas Output '%s'", QT_TO_UTF8(output_name));
		obs_canvas_release(canvas);
		obs_log(LOG_DEBUG, "-canvas_output_init()");
		return;
	}

	// Bind the output to the canvas video instead of the default Program video.
	// Audio is the global mix: canvases do not have a separate audio output.
	obs_output_set_media(context.output, obs_canvas_get_video(canvas), obs_get_audio());
	// Send only the selected OBS audio track (mixer index is 0-based). Must be set before the output starts.
	obs_output_set_mixer(context.output, audio_track - 1);

	context.canvas = canvas;
	signal_handler_connect(obs_canvas_get_signal_handler(canvas), "remove", on_canvas_removed, nullptr);

	// Start handling "remote" start/stop events (ex: from obs-websocket)
	auto sh = obs_output_get_signal_handler(context.output);
	signal_handler_connect(sh, "start", on_canvas_output_started, nullptr);
	signal_handler_connect(sh, "stop", on_canvas_output_stopped, nullptr);

	context.ndi_name = output_name;
	context.ndi_groups = output_groups;
	context.canvas_name = QString::fromUtf8(obs_canvas_get_name(canvas));

	canvas_output_start();

	obs_log(LOG_DEBUG, "-canvas_output_init()");
}
