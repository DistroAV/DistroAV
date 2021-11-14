#include <obs-module.h>

#include "obs-ndi.h"
#include "obs-ndi-output.h"

ndi_output::ndi_output(obs_data_t *settings, obs_output_t *output) :
	output(output),
	ndi_send(nullptr),
	running(false)
{
	update(settings);
}

ndi_output::~ndi_output()
{
	;
}

bool ndi_output::start()
{
	return false;
}

void ndi_output::stop(uint64_t ts)
{
	;
}

void ndi_output::raw_video(struct video_data *frame)
{
	;
}

void ndi_output::raw_audio(struct audio_data *frames)
{
	;
}

void ndi_output::update(obs_data_t *settings)
{
	;
}

// Static
void ndi_output::defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, P_OUTPUT_SOURCE_NAME, T_OUTPUT_SOURCE_NAME_DEFAULT);
}

obs_properties_t *ndi_output::properties()
{
	obs_properties_t* props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_properties_add_text(props, P_OUTPUT_SOURCE_NAME, T_OUTPUT_SOURCE_NAME, OBS_TEXT_DEFAULT);

	return props;
}

void register_ndi_output_info()
{
	struct obs_output_info info = {};
	info.id				= "ndi_output_v5";
	info.flags			= OBS_OUTPUT_AV;
	info.get_name		= [](void *) { return obs_module_text("Output.Name"); };
	info.create			= [](obs_data_t *settings, obs_output_t *output) -> void * { return new ndi_output(settings, output); };
	info.destroy		= [](void *data) { delete static_cast<ndi_output *>(data); };
	info.start			= [](void *data) -> bool { return static_cast<ndi_output *>(data)->start(); };
	info.stop			= [](void *data, uint64_t ts) { static_cast<ndi_output *>(data)->stop(ts); };
	info.raw_video		= [](void *data, struct video_data *frame) { static_cast<ndi_output *>(data)->raw_video(frame); };
	info.raw_audio		= [](void *data, struct audio_data *frames) { static_cast<ndi_output *>(data)->raw_audio(frames); };
	info.update			= [](void *data, obs_data_t *settings) { static_cast<ndi_output *>(data)->update(settings); };
	info.get_defaults	= ndi_output::defaults;
	info.get_properties	= [](void *data) -> obs_properties_t * { return static_cast<ndi_output *>(data)->properties(); };

	obs_register_output(&info);
}
