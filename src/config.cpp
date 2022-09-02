#include <obs-frontend-api.h>

#include "obs-ndi.h"
#include "config.h"

#define CONFIG_SECTION_NAME "OBSNdi"

#define PARAM_NDI_EXTRA_IPS "NdiExtraIps"
#define PARAM_PROGRAM_OUTPUT_ENABLEED "program_output_enabled"
#define PARAM_PROGRAM_OUTPUT_NAME "program_output_name"
#define PARAM_PREVIEW_OUTPUT_ENABLED "preview_output_enabled"
#define PARAM_PREVIEW_OUTPUT_NAME "preview_output_name"

obs_ndi_config::obs_ndi_config()
	: ndi_extra_ips(""),
	  program_output_enabled(false),
	  program_output_name("obs-ndi Program Mix"),
	  preview_output_enabled(false),
	  preview_output_name("obs-ndi Preview Mix")
{
	set_defaults();
}

void obs_ndi_config::load()
{
	config_t *obs_config = get_config_store();
	if (!obs_config) {
		blog(LOG_ERROR, "[obs_ndi_config::load] Unable to fetch OBS config!");
		return;
	}

	ndi_extra_ips = config_get_string(obs_config, CONFIG_SECTION_NAME, PARAM_NDI_EXTRA_IPS);
	program_output_enabled = config_get_bool(obs_config, CONFIG_SECTION_NAME, PARAM_PROGRAM_OUTPUT_ENABLEED);
	program_output_name = config_get_string(obs_config, CONFIG_SECTION_NAME, PARAM_PROGRAM_OUTPUT_NAME);
	preview_output_enabled = config_get_bool(obs_config, CONFIG_SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED);
	preview_output_name = config_get_string(obs_config, CONFIG_SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME);
}

void obs_ndi_config::save()
{
	config_t *obs_config = get_config_store();
	if (!obs_config) {
		blog(LOG_ERROR, "[obs_ndi_config::save] Unable to fetch OBS config!");
		return;
	}

	config_set_string(obs_config, CONFIG_SECTION_NAME, PARAM_NDI_EXTRA_IPS, ndi_extra_ips.c_str());
	config_set_bool(obs_config, CONFIG_SECTION_NAME, PARAM_PROGRAM_OUTPUT_ENABLEED, program_output_enabled);
	config_set_string(obs_config, CONFIG_SECTION_NAME, PARAM_PROGRAM_OUTPUT_NAME, program_output_name.c_str());
	config_set_bool(obs_config, CONFIG_SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED, preview_output_enabled);
	config_set_string(obs_config, CONFIG_SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME, preview_output_name.c_str());

	config_save(obs_config);
}

void obs_ndi_config::set_defaults()
{
	config_t *obs_config = get_config_store();
	if (!obs_config) {
		blog(LOG_ERROR, "[obs_ndi_config::set_defaults] Unable to fetch OBS config!");
		return;
	}

	// Defaults defined by constructor
	config_set_default_string(obs_config, CONFIG_SECTION_NAME, PARAM_NDI_EXTRA_IPS, ndi_extra_ips.c_str());
	config_set_default_bool(obs_config, CONFIG_SECTION_NAME, PARAM_PROGRAM_OUTPUT_ENABLEED, program_output_enabled);
	config_set_default_string(obs_config, CONFIG_SECTION_NAME, PARAM_PROGRAM_OUTPUT_NAME, program_output_name.c_str());
	config_set_default_bool(obs_config, CONFIG_SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED, preview_output_enabled);
	config_set_default_string(obs_config, CONFIG_SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME, preview_output_name.c_str());
}

config_t *obs_ndi_config::get_config_store()
{
	return obs_frontend_get_global_config();
}
