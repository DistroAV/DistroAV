#include "obs-ndi.h"
#include "output-manager.h"
#include "config.h"

output_manager::output_manager()
{
	_program_output = obs_output_create("ndi_output_v5", "NDI 5 Program Mix", nullptr, nullptr);
}

output_manager::~output_manager()
{
	if (_program_output)
		obs_output_stop(_program_output);
}

obs_output_t *output_manager::get_program_output()
{
	return obs_output_get_ref(_program_output);
}

void output_manager::update_program_output()
{
	if (!_program_output)
		return;

	auto conf = get_config();
	if (!conf)
		return;

	bool was_running = obs_output_active(_program_output);
	OBSDataAutoRelease settings = obs_output_get_settings(_program_output);

	if (conf->program_output_enabled == was_running &&
		conf->program_output_name == obs_data_get_string(settings, "source_name"))
		return;

	if (was_running)
		obs_output_stop(_program_output);

	obs_data_set_string(settings, "source_name", conf->program_output_name.c_str());
	obs_output_update(_program_output, settings);

	if (conf->program_output_enabled) {
		if (obs_output_start(_program_output))
			blog(LOG_INFO, "Program output started.");
		else
			blog(LOG_ERROR, "Program output not started!");
	}
}
