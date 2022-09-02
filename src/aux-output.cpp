#include "obs-ndi.h"
#include "aux-output.h"

aux_output::aux_output(std::string output_name) : _output(nullptr), _view(nullptr), _video(nullptr)
{
	_output = obs_output_create("ndi_output_v5", output_name.c_str(), nullptr, nullptr);
	if (!_output) {
		blog(LOG_ERROR, "[aux_output::aux_output] Failed to create output: %s", output_name.c_str());
		return;
	}
}

aux_output::~aux_output()
{
	stop();
}

bool aux_output::start(bool audio_enabled)
{
	if (!_output)
		return false;

	if (obs_output_active(_output))
		return true;

	if (!_view)
		_view = obs_view_create();
	if (!_view) {
		blog(LOG_ERROR, "[aux_output::start_output] Failed to create obs_view_t for output: %s",
		     obs_output_get_name(_output));
		return false;
	}

	if (!_video)
		_video = obs_view_add(_view);
	if (!_video) {
		blog(LOG_ERROR, "[aux_output::start_output] Failed to activate obs_view_t for output: %s",
		     obs_output_get_name(_output));
		stop();
		return false;
	}

	audio_t *audio = nullptr;
	if (audio_enabled)
		audio = obs_get_audio();
	obs_output_set_media(_output, _video, audio);

	bool ret = obs_output_start(_output);
	if (!ret)
		stop();

	return ret;
}

void aux_output::stop()
{
	if (_output) {
		obs_output_stop(_output);
		obs_output_set_media(_output, nullptr, nullptr);
	}

	if (_view) {
		if (_video) {
			obs_view_remove(_view);
			obs_view_set_source(_view, 0, nullptr);
			_video = nullptr;
		}
		obs_view_destroy(_view);
		_view = nullptr;
	}
}

bool aux_output::set_ndi_source_name(std::string name)
{
	if (!_output)
		return false;

	bool was_running = obs_output_active(_output);
	OBSDataAutoRelease settings = obs_output_get_settings(_output);

	if (name == obs_data_get_string(settings, "source_name"))
		return true;

	if (was_running)
		obs_output_stop(_output);

	obs_data_set_string(settings, "source_name", name.c_str());
	obs_output_update(_output, settings);

	if (was_running)
		return start();

	return true;
}

obs_source_t *aux_output::get_source()
{
	if (!_view)
		return nullptr;

	return obs_view_get_source(_view, 0);
}

void aux_output::set_source(obs_source_t *source)
{
	if (!_view)
		return;

	OBSSourceAutoRelease current_source = obs_view_get_source(_view, 0);
	if (source == current_source)
		return;

	obs_view_set_source(_view, 0, source);
}
