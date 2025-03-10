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
	obs_log(LOG_INFO, "+on_main_output_started()");
	Config::Current()->OutputEnabled = true;
	obs_log(LOG_INFO, "-on_main_output_started()");
}

void on_main_output_stopped(void *, calldata_t *)
{
	obs_log(LOG_INFO, "+on_main_output_stopped()");
	Config::Current()->OutputEnabled = false;
	obs_log(LOG_INFO, "-on_main_output_stopped()");
}

void main_output_stop()
{
	obs_log(LOG_INFO, "+main_output_stop()");
	if (context.is_running) {
		obs_log(LOG_INFO, "main_output_stop: stopping NDI main output '%s'", QT_TO_UTF8(context.ndi_name));
		obs_output_stop(context.output);

		context.is_running = false;

		obs_log(LOG_INFO, "main_output_stop: successfully stopped NDI main output '%s'",
			QT_TO_UTF8(context.ndi_name));
	} else {
		obs_log(LOG_ERROR, "main_output_stop: NDI main output `%s` is not running",
			QT_TO_UTF8(context.ndi_name));
	}
	obs_log(LOG_INFO, "-main_output_stop()");
}

void main_output_start()
{
	obs_log(LOG_INFO, "+main_output_start()");
	if (context.output) {
		if (context.is_running) {
			main_output_stop();
		}

		obs_log(LOG_INFO, "main_output_start: starting NDI main output '%s'", QT_TO_UTF8(context.ndi_name));

		context.is_running = obs_output_start(context.output);
		if (context.is_running) {
			obs_log(LOG_INFO, "main_output_start: successfully started NDI main output '%s'",
				QT_TO_UTF8(context.ndi_name));
			context.last_error = QString("");
		} else {
			context.last_error = obs_output_get_last_error(context.output);
			obs_log(LOG_ERROR, "main_output_start: failed to start NDI main output '%s'; error='%s'",
				QT_TO_UTF8(context.ndi_name), QT_TO_UTF8(context.last_error));
			obs_output_stop(context.output);
		}
	} else {
		obs_log(LOG_ERROR, "main_output_start: NDI main output '%s' is not initialized",
			QT_TO_UTF8(context.ndi_name));
	}
	obs_log(LOG_INFO, "-main_output_start()");
}

bool main_output_is_supported()
{
	main_output_init();
	bool is_supported = context.is_running;
	bool enabled = Config::Current()->OutputEnabled;
	main_output_deinit(); // will trigger a stop event if running, which will set OutputEnabled to false
	Config::Current()->OutputEnabled = true;
	return is_supported;
}

void main_output_deinit()
{
	obs_log(LOG_INFO, "+main_output_deinit()");
	if (context.output) {
		main_output_stop();

		obs_log(LOG_INFO, "main_output_deinit: releasing NDI main output '%s'", QT_TO_UTF8(context.ndi_name));

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
		obs_log(LOG_INFO, "main_output_deinit: successfully released NDI main output '%s'",
			QT_TO_UTF8(context.ndi_name));
	} else {
		obs_log(LOG_INFO, "main_output_deinit: NDI main output is not initialized");
	}
	obs_log(LOG_INFO, "-main_output_deinit()");
}

void main_output_init()
{
	obs_log(LOG_INFO, "+main_output_init()");

	auto config = Config::Current();
	auto output_name = config->OutputName;
	auto output_groups = config->OutputGroups;
	auto is_enabled = config->OutputEnabled;

	main_output_deinit();

	if (is_enabled && !output_name.isEmpty()) {
		obs_log(LOG_INFO, "main_output_init: creating NDI main output '%s'", QT_TO_UTF8(output_name));
		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "ndi_name", QT_TO_UTF8(output_name));
		obs_data_set_string(output_settings, "ndi_groups", QT_TO_UTF8(output_groups));

		context.output = obs_output_create("ndi_output", "NDI Main Output", output_settings, nullptr);
		obs_data_release(output_settings);
		if (context.output) {
			obs_log(LOG_INFO, "main_output_init: created NDI main output '%s'", QT_TO_UTF8(output_name));

			// Start handling "remote" start/stop events (ex: from obs-websocket)
			auto sh = obs_output_get_signal_handler(context.output);
			signal_handler_connect( //
				sh, "start", on_main_output_started, nullptr);
			signal_handler_connect( //
				sh, "stop", on_main_output_stopped, nullptr);

			context.ndi_name = output_name;
			context.ndi_groups = output_groups;
		} else {
			obs_log(LOG_ERROR, "main_output_init: failed to create NDI main output '%s'",
				QT_TO_UTF8(output_name));
		}
		main_output_start();
	}

	obs_log(LOG_INFO, "-main_output_init()");
}
