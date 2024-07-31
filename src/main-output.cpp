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

static obs_output_t *main_out = nullptr;
static bool main_output_running = false;

void main_output_start(const char *output_name, const char *output_groups)
{
	blog(LOG_INFO, "[obs-ndi] +main_output_start(`%s`, `%s`)", output_name,
	     output_groups);
	if (!main_output_running) {
		blog(LOG_INFO,
		     "[DistroAV] main_output_start: starting NDI main output with name '%s'",
		     output_name);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "ndi_name", output_name);
		obs_data_set_string(settings, "ndi_groups", output_groups);
		main_out = obs_output_create("ndi_output", "NDI Main Output",
					     settings, nullptr);
		obs_data_release(settings);
		if (main_out) {
			auto result = obs_output_start(main_out);
			blog(LOG_INFO,
			     "[DistroAV] main_output_start: obs_output_start result=%d",
			     result);

			main_output_running = true;

			blog(LOG_INFO,
			     "[DistroAV] main_output_start: started NDI main output");
		} else {
			blog(LOG_ERROR,
			     "[DistroAV] main_output_start: failed to create NDI main output");
		}
	} else {
		blog(LOG_INFO,
		     "[DistroAV] main_output_start: NDI main output already running");
	}
	blog(LOG_INFO, "[obs-ndi] -main_output_start(`%s`, `%s`)", output_name,
	     output_groups);
}

void main_output_stop()
{
	blog(LOG_INFO, "[obs-ndi] +main_output_stop()");
	if (main_output_running) {
		blog(LOG_INFO,
		     "[DistroAV] main_output_stop: stopping NDI main output");

		obs_output_stop(main_out);
		obs_output_release(main_out);
		main_out = nullptr;

		main_output_running = false;

		blog(LOG_INFO,
		     "[DistroAV] main_output_stop: stopped NDI main output");
	} else {
		blog(LOG_INFO,
		     "[DistroAV] main_output_stop: NDI main output not running");
	}
	blog(LOG_INFO, "[obs-ndi] -main_output_stop()");
}

bool main_output_is_running()
{
	return main_output_running;
}
