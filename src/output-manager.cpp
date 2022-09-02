#include "obs-ndi.h"
#include "output-manager.h"
#include "config.h"

static obs_source_t *get_scene_for_preview_output()
{
	if (obs_frontend_preview_program_mode_active())
		return obs_frontend_get_current_preview_scene();
	return obs_frontend_get_current_scene();
}

output_manager::output_manager()
	: _program_output(obs_output_create("ndi_output_v5", "NDI 5 Program Mix", nullptr, nullptr)),
	  _preview_output("NDI 5 Preview Mix")
{
	obs_frontend_add_event_callback(on_frontend_event, this);
}

output_manager::~output_manager()
{
	if (_program_output)
		obs_output_stop(_program_output);

	obs_frontend_remove_event_callback(on_frontend_event, this);
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

	OBSDataAutoRelease settings = obs_output_get_settings(_program_output);

	if (conf->program_output_enabled == obs_output_active(_program_output) &&
	    conf->program_output_name == obs_data_get_string(settings, "source_name"))
		return;

	obs_output_stop(_program_output);

	obs_data_set_string(settings, "source_name", conf->program_output_name.c_str());
	obs_output_update(_program_output, settings);

	if (conf->program_output_enabled) {
		if (obs_output_start(_program_output))
			blog(LOG_INFO, "Program output started.");
		else
			blog(LOG_ERROR, "Program output failed to start!");
	}
}

void output_manager::update_preview_output()
{
	auto conf = get_config();
	if (!conf)
		return;

	OBSSourceAutoRelease scene = get_scene_for_preview_output();
	if (!scene)
		return;

	_preview_output.set_source(scene);

	if (conf->preview_output_enabled) {
		_preview_output.set_ndi_source_name(conf->preview_output_name);
		_preview_output.start();
	} else {
		_preview_output.stop();
	}
}

void output_manager::on_frontend_event(enum obs_frontend_event event, void *priv_data)
{
	auto manager = static_cast<output_manager *>(priv_data);

	auto conf = get_config();
	if (!conf)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		manager->update_program_output();
		manager->update_preview_output();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		if (manager->_program_output)
			obs_output_stop(manager->_program_output);
		manager->_preview_output.stop();
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED: {
		OBSSourceAutoRelease scene = get_scene_for_preview_output();
		if (scene)
			manager->_preview_output.set_source(scene);
	} break;
	default:
		break;
	}
}
