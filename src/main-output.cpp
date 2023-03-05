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

#include "obs-ndi.h"

static obs_output_t *main_out = nullptr;
static bool main_output_running = false;

void main_output_init(const char *default_name)
{
	if (main_out)
		return;

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "ndi_name", default_name);
	main_out = obs_output_create("ndi_output", "NDI Main Output", settings,
				     nullptr);
	obs_data_release(settings);
}

void main_output_start(const char *output_name)
{
	if (main_output_running || !main_out)
		return;

	blog(LOG_INFO,
	     "main_output_start: starting NDI main output with name '%s'",
	     output_name);

	obs_data_t *settings = obs_output_get_settings(main_out);
	obs_data_set_string(settings, "ndi_name", output_name);
	obs_output_update(main_out, settings);
	obs_data_release(settings);

	obs_output_start(main_out);
	main_output_running = true;

	blog(LOG_INFO, "main_output_start: started NDI main output");
}

void main_output_stop()
{
	if (!main_output_running)
		return;

	blog(LOG_INFO, "main_output_stop: stopping NDI main output");

	obs_output_stop(main_out);

	main_output_running = false;

	blog(LOG_INFO, "main_output_stop: stopped NDI main output");
}

void main_output_deinit()
{
	blog(LOG_INFO, "+main_output_deinit()");
	obs_output_release(main_out);
	main_out = nullptr;
	main_output_running = false;
	blog(LOG_INFO, "-main_output_deinit()");
}

bool main_output_is_running()
{
	return main_output_running;
}