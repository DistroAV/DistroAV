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

void main_output_init(const char *default_name, const char *default_groups)
{
  blog(LOG_INFO, "[DistroAV] +main_output_init(`%s`, `%s`)", default_name,
     default_groups);
	if (!main_out) {
		blog(LOG_INFO,
		     "[DistroAV] main_output_start: creating NDI main output '%s'",
		     output_name);

		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "ndi_name", default_name);
		obs_data_set_string(output_settings, "ndi_groups", default_groups);
		main_out = obs_output_create("ndi_output", "NDI Main Output",
					     output_settings, nullptr);
		obs_data_release(output_settings);
    if (main_out) {
      blog(LOG_INFO,
         "[DistroAV] main_output_init: successfully created NDI main output '%s'",
		     output_name);
    } else {
      blog(LOG_ERROR,
         "[DistroAV] main_output_init: failed to create NDI main output '%s'",
		     output_name);
    }
  } else {
      blog(LOG_INFO,
         "[DistroAV] main_output_init: already created NDI main output '%s'",
		     output_name);
  }
	blog(LOG_INFO, "[DistroAV] -main_output_init(`%s`, `%s`)", default_name,
	   default_groups);
}

void main_output_start()
{
	blog(LOG_INFO, "[DistroAV] +main_output_start()");
	if (main_out) {
    if (!main_output_running) {
      main_output_running = obs_output_start(main_out);
      if (main_output_running) {
        blog(LOG_INFO,
           "[DistroAV] main_output_start: successfully started NDI main output '%s'",
  		     output_name);
      } else {
        auto error = obs_output_get_last_error(main_out);
        blog(LOG_ERROR,
           "[DistroAV] main_output_start: failed to start NDI main output '%s'; error='%s'",
           output_name, error);
      }
		} else {
			blog(LOG_ERROR,
         "[DistroAV] main_output_start: already started NDI main output '%s'",
         output_name);
		}
	} else {
    blog(LOG_ERROR,
       "[DistroAV] main_output_start: NDI main output '%s' not created yet",
       output_name);
	}
	blog(LOG_INFO, "[DistroAV] -main_output_start()");
}

void main_output_stop()
{
	blog(LOG_INFO, "[DistroAV] +main_output_stop()");
	if (main_output_running) {
		blog(LOG_INFO,
       "[DistroAV] main_output_stop: stopping NDI main output");

		obs_output_stop(main_out);

		main_output_running = false;

		blog(LOG_INFO,
       "[DistroAV] main_output_stop: stopped NDI main output");
	} else {
		blog(LOG_INFO,
       "[DistroAV] main_output_stop: NDI main output not running");
	}
	blog(LOG_INFO, "[DistroAV] -main_output_stop()");
}

bool main_output_is_running()
{
	return main_output_running;
}
