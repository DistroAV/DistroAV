/*
	obs-ndi (NDI I/O in OBS Studio)
	Copyright (C) 2016 Stéphane Lepin <stephane.lepin@gmail.com>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#include "Config.h"
#include "obs-ndi.h"

#define CONFIG_SECTION_NAME "obs-ndi"
#define CONFIG_PARAM_ENABLED "output_enabled"
#define CONFIG_PARAM_NAME "output_name"

Config *Config::_instance = new Config();

Config::Config() {
	OutputEnabled = false;
	OutputName = "OBS";
}

void Config::OBSSaveCallback(obs_data_t *save_data, bool saving, void *private_data) {
	Config *conf = static_cast<Config*>(private_data);

	if (saving) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, CONFIG_PARAM_ENABLED, conf->OutputEnabled);
		obs_data_set_string(settings, CONFIG_PARAM_NAME, conf->OutputName);

		obs_data_set_obj(save_data, CONFIG_SECTION_NAME, settings);
	}
	else {
		obs_data_t *settings = obs_data_get_obj(save_data, CONFIG_SECTION_NAME);
		if (settings) {
			conf->OutputEnabled = obs_data_get_bool(settings, CONFIG_PARAM_ENABLED);
			conf->OutputName = obs_data_get_string(settings, CONFIG_PARAM_NAME);

			if (conf->OutputEnabled) {
				main_output_start();
			}
		}
	}
}

Config* Config::Current() {
	return _instance;
}