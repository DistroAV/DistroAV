#include <obs-frontend-api.h>

#include "obs-ndi.h"
#include "obs-ndi-config.h"

#define CONFIG_SECTION_NAME "OBSNdi"

#define PARAM_NDI_EXTRA_IPS "NdiExtraIps"

obs_ndi_config::obs_ndi_config() :
	ndi_extra_ips("")
{
	set_defaults();
}

void obs_ndi_config::load()
{
	config_t* obs_config = get_config_store();
	if (!obs_config) {
		blog(LOG_ERROR, "[obs_ndi_config::load] Unable to fetch OBS config!");
		return;
	}

	ndi_extra_ips = config_get_string(obs_config, CONFIG_SECTION_NAME, PARAM_NDI_EXTRA_IPS);
}

void obs_ndi_config::save()
{
	config_t* obs_config = get_config_store();
	if (!obs_config) {
		blog(LOG_ERROR, "[obs_ndi_config::save] Unable to fetch OBS config!");
		return;
	}

	config_set_bool(obs_config, CONFIG_SECTION_NAME, PARAM_NDI_EXTRA_IPS, ndi_extra_ips.c_str());

	config_save(obs_config);
}

void obs_ndi_config::set_defaults()
{
	config_t* obs_config = get_config_store();
	if (!obs_config) {
		blog(LOG_ERROR, "[obs_ndi_config::set_defaults] Unable to fetch OBS config!");
		return;
	}

	// Defaults defined by constructor
	config_set_default_string(obs_config, CONFIG_SECTION_NAME, PARAM_NDI_EXTRA_IPS, ndi_extra_ips.c_str());
}

config_t* obs_ndi_config::get_config_store()
{
	return obs_frontend_get_global_config();
}
