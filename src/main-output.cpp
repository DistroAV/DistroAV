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

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "plugin-main.h"

static obs_output_t *main_out = nullptr;
static bool main_output_running = false;

void main_output_init(const char *default_name, const char *default_groups)
{
	if (main_out)
		return;

	blog(LOG_INFO, "[obs-ndi] main_output_init('%s', '%s')", default_name,
	     default_groups);

	obs_data_t *output_settings = obs_data_create();
	obs_data_set_string(output_settings, "ndi_name", default_name);
	obs_data_set_string(output_settings, "ndi_groups", default_groups);
	main_out = obs_output_create("ndi_output", "NDI Main Output",
				     output_settings, nullptr);
	obs_data_release(output_settings);
	if (!main_out) {
		blog(LOG_ERROR,
		     "main_output_init: failed to create NDI main output");
		return;
	}
}

void main_output_start(const char *output_name, const char *output_groups)
{
	if (main_output_running)
		return;

	blog(LOG_INFO,
	     "[obs-ndi] main_output_start: starting NDI main output with name '%s'",
	     output_name);

	auto result = obs_output_start(main_out);
	blog(LOG_INFO,
	     "[obs-ndi] main_output_start: obs_output_start result=%d", result);

	main_output_running = true;

	blog(LOG_INFO, "[obs-ndi] main_output_start: started NDI main output");
}

void main_output_stop()
{
	if (!main_output_running)
		return;

	blog(LOG_INFO, "[obs-ndi] main_output_stop: stopping NDI main output");

	obs_output_stop(main_out);

	main_output_running = false;

	blog(LOG_INFO, "[obs-ndi] main_output_stop: stopped NDI main output");
}
bool main_output_is_running()
{
	return main_output_running;
}
