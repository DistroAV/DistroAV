/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "obs-ndi.h"
#include "Config.h"

static obs_output_t* main_out;
static bool main_output_running = false;

void main_output_start(const char* output_name)
{
	if (!main_output_running) {
		blog(LOG_INFO, "starting main NDI output with name '%s'",
			qPrintable(Config::Current()->OutputName));

		obs_data_t* output_settings = obs_data_create();
		obs_data_set_string(output_settings, "ndi_name", output_name);

		main_out = obs_output_create("ndi_output", "main_ndi_output",
			output_settings, NULL);

		obs_output_start(main_out);
		obs_data_release(output_settings);

		main_output_running = true;
	}
}

void main_output_stop()
{
	if (main_output_running) {
		blog(LOG_INFO, "stopping main NDI output");

		obs_output_stop(main_out);
		obs_output_release(main_out);
		main_output_running = false;
	}
}

bool main_output_is_running()
{
	return main_output_running;
}