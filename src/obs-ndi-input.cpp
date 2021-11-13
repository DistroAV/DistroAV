#include <obs-module.h>

#include "obs-ndi.h"
#include "obs-ndi-input.h"

ndi_input::ndi_input(obs_data_t *settings, obs_source_t *source) :
	source(source)
{
	;
}

ndi_input::~ndi_input()
{
	;
}

// Static
void ndi_input::defaults(obs_data_t *settings)
{
	;
}

obs_properties_t *ndi_input::properties()
{
	;
}

void ndi_input::update(obs_data_t *settings)
{
	;
}

void ndi_input::activate()
{
	;
}

void ndi_input::deactivate()
{
	;
}

void ndi_input::show()
{
	;
}

void ndi_input::hide()
{
	;
}

void register_ndi_source_info()
{
	struct obs_source_info info = {};
	info.id				= "ndi_input_v5";
	info.type			= OBS_SOURCE_TYPE_INPUT;
	info.output_flags	= OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name		= [](void *) { return obs_module_text("Input.Name"); };
	info.create			= [](obs_data_t *settings, obs_source_t *source) -> void * { return new ndi_input(settings, source); };
	info.destroy		= [](void *data) { delete static_cast<ndi_input *>(data); };
	info.get_defaults	= ndi_input::defaults;
	info.get_properties	= [](void *data) -> obs_properties_t * { return static_cast<ndi_input *>(data)->properties(); };
	info.update			= [](void *data, obs_data_t *settings) { static_cast<ndi_input *>(data)->update(settings); };
	info.activate		= [](void *data) { static_cast<ndi_input *>(data)->activate(); };
	info.deactivate		= [](void *data) { static_cast<ndi_input *>(data)->deactivate(); };
	info.show			= [](void *data) { static_cast<ndi_input *>(data)->show(); };
	info.hide			= [](void *data) { static_cast<ndi_input *>(data)->hide(); };

	obs_register_source(&info);
}
