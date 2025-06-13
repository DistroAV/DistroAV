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

#include "main-output.h"

#include "plugin-main.h"

struct main_output {
	bool is_running;
	QString ndi_name;
	QString ndi_groups;
	QString last_error;
	obs_source_t *current_source;
	obs_output_t *output;
};

static struct main_output context = {0};

QString main_output_last_error()
{
	return context.last_error;
};

void on_main_output_started(void *, calldata_t *)
{
	obs_log(LOG_DEBUG, "+on_main_output_started()");
	Config::Current()->OutputEnabled = true;
	obs_log(LOG_DEBUG, "-on_main_output_started()");
	obs_log(LOG_INFO, "NDI Main Output started");
}

void on_main_output_stopped(void *, calldata_t *)
{
	obs_log(LOG_DEBUG, "+on_main_output_stopped()");
	Config::Current()->OutputEnabled = false;
	obs_log(LOG_DEBUG, "-on_main_output_stopped()");
	obs_log(LOG_INFO, "NDI Main Output stopped");
}

void main_output_stop()
{
	obs_log(LOG_DEBUG, "+main_output_stop()");
	if (context.is_running) {
		obs_log(LOG_DEBUG, "main_output_stop: stopping NDI Main Output '%s'", QT_TO_UTF8(context.ndi_name));
		obs_output_stop(context.output);

		context.is_running = false;

		obs_log(LOG_DEBUG, "main_output_stop: successfully stopped NDI Main Output '%s'",
			QT_TO_UTF8(context.ndi_name));
	} else {
		obs_log(LOG_DEBUG, "main_output_stop: NDI Main Output '%s' is not running",
			QT_TO_UTF8(context.ndi_name));
	}
	obs_log(LOG_DEBUG, "-main_output_stop()");
}

void main_output_start()
{
	obs_log(LOG_DEBUG, "+main_output_start()");
	if (context.output) {
		if (context.is_running) {
			main_output_stop();
		}

		obs_log(LOG_DEBUG, "main_output_start: starting NDI Main Output '%s'", QT_TO_UTF8(context.ndi_name));

		context.is_running = obs_output_start(context.output);
		if (context.is_running) {
			obs_log(LOG_DEBUG, "main_output_start: successfully started NDI Main Output '%s'",
				QT_TO_UTF8(context.ndi_name));
			context.last_error = QString("");
		} else {
			context.last_error = obs_output_get_last_error(context.output);
			obs_log(LOG_DEBUG, "main_output_start: failed to start NDI Main Output '%s'; error='%s'",
				QT_TO_UTF8(context.ndi_name), QT_TO_UTF8(context.last_error));
			obs_log(LOG_ERROR, "ERR-400 - Failed to start NDI Main Output '%s'; error='%s'",
				QT_TO_UTF8(context.ndi_name), QT_TO_UTF8(context.last_error));
			// Could not start Output, still trigger it to stop.
			obs_output_stop(context.output);
		}
	} else {
		obs_log(LOG_DEBUG, "main_output_start: NDI Main Output '%s' is not initialized and cannot start.",
			QT_TO_UTF8(context.ndi_name));
		obs_log(LOG_ERROR, "ERR-411 - Failed to initialize NDI Main Output '%s'", QT_TO_UTF8(context.ndi_name));
	}
	obs_log(LOG_DEBUG, "-main_output_start()");
}

bool main_output_is_supported()
{
	obs_log(LOG_DEBUG, "+main_output_is_supported()");
	auto config = Config::Current();
	auto output_name = config->OutputName;
	auto output_groups = config->OutputGroups;

	obs_data_t *output_settings = obs_data_create();
	obs_data_set_string(output_settings, "ndi_name", "NDI Output Support Test");
	obs_data_set_string(output_settings, "ndi_groups", "DistroAV Config");

	bool is_supported = true;
	context.last_error = QString("");

	auto output = obs_output_create("ndi_output", "NDI Main Output", output_settings, nullptr);
	obs_data_release(output_settings);

	if (output != nullptr) {
		bool is_running = obs_output_start(output);

		if (!is_running) {
			is_supported = false;
			context.last_error = obs_output_get_last_error(output);
			obs_log(LOG_DEBUG, "main_output_is_supported: '%s'", QT_TO_UTF8(context.last_error));
		}
		obs_output_stop(output);
		obs_output_release(output);
	} else {
		is_supported = false;
		obs_log(LOG_DEBUG, "main_output_is_supported: NDI Main Output could not created");
	}
	obs_log(LOG_DEBUG, "-main_output_is_supported()");
	return is_supported;
}

void main_output_deinit()
{
	obs_log(LOG_DEBUG, "+main_output_deinit()");

	if (context.output) {
		main_output_stop();

		obs_log(LOG_DEBUG, "main_output_deinit: releasing NDI Main Output '%s'", QT_TO_UTF8(context.ndi_name));

		// Stop handling "remote" start/stop events (ex: from obs-websocket)
		auto sh = obs_output_get_signal_handler(context.output);
		signal_handler_disconnect(sh, "start", //
					  on_main_output_started, nullptr);
		signal_handler_disconnect(sh, "stop", //
					  on_main_output_stopped, nullptr);

		obs_output_release(context.output);
		context.output = nullptr;
		context.ndi_name.clear();
		context.ndi_groups.clear();
		obs_log(LOG_DEBUG, "main_output_deinit: successfully released NDI Main Output '%s'",
			QT_TO_UTF8(context.ndi_name));
	}
	obs_log(LOG_DEBUG, "-main_output_deinit()");
}

void main_output_init()
{
	obs_log(LOG_DEBUG, "+main_output_init()");

	auto config = Config::Current();
	auto output_name = config->OutputName;
	auto output_groups = config->OutputGroups;
	auto is_enabled = config->OutputEnabled;

	if (is_enabled && !main_output_is_supported()) {
		config->OutputEnabled = false;
		config->Save();
		is_enabled = false;
		obs_log(LOG_WARNING, "WARN-426 - NDI Main Output disabled, format not supported");
	}

	main_output_deinit();

	if (is_enabled && !output_name.isEmpty()) {
		obs_log(LOG_DEBUG, "main_output_init: creating NDI Main Output '%s'", QT_TO_UTF8(output_name));
		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "ndi_name", QT_TO_UTF8(output_name));
		obs_data_set_string(output_settings, "ndi_groups", QT_TO_UTF8(output_groups));

		context.output = obs_output_create("ndi_output", "NDI Main Output", output_settings, nullptr);
		obs_data_release(output_settings);
		if (context.output) {
			obs_log(LOG_DEBUG, "main_output_init: created NDI Main Output '%s'", QT_TO_UTF8(output_name));

			// Start handling "remote" start/stop events (ex: from obs-websocket)
			auto sh = obs_output_get_signal_handler(context.output);
			signal_handler_connect( //
				sh, "start", on_main_output_started, nullptr);
			signal_handler_connect( //
				sh, "stop", on_main_output_stopped, nullptr);

			context.ndi_name = output_name;
			context.ndi_groups = output_groups;
		} else {
			obs_log(LOG_DEBUG, "main_output_init: failed to create NDI Main Output '%s'",
				QT_TO_UTF8(output_name));
			obs_log(LOG_ERROR, "ERR-412 - Failed to create NDI Main Output '%s'",
				QT_TO_UTF8(context.ndi_name));
		}
		main_output_start();
	}

	obs_log(LOG_DEBUG, "-main_output_init()");
}
