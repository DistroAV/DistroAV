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

	blog(LOG_INFO, "starting NDI main output with name '%s'", output_name);

	obs_data_t *settings = obs_output_get_settings(main_out);
	obs_data_set_string(settings, "ndi_name", output_name);
	obs_output_update(main_out, settings);
	obs_data_release(settings);

	obs_output_start(main_out);
	main_output_running = true;
}

void main_output_stop()
{
	if (!main_output_running)
		return;

	blog(LOG_INFO, "stopping NDI main output");

	obs_output_stop(main_out);
	main_output_running = false;
}

void main_output_deinit()
{
	obs_output_release(main_out);
	main_out = nullptr;
	main_output_running = false;
}

bool main_output_is_running()
{
	return main_output_running;
}