/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#include "Config.h"
#include "obs-ndi.h"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#define SECTION_NAME "NDIPlugin"
#define PARAM_MAIN_OUTPUT_ENABLED "MainOutputEnabled"
#define PARAM_MAIN_OUTPUT_NAME "MainOutputName"
#define PARAM_PREVIEW_OUTPUT_ENABLED "PreviewOutputEnabled"
#define PARAM_PREVIEW_OUTPUT_NAME "PreviewOutputName"
#define PARAM_TALLY_PROGRAM_ENABLED "TallyProgramEnabled"
#define PARAM_TALLY_PREVIEW_ENABLED "TallyPreviewEnabled"

Config* Config::_instance = nullptr;

Config::Config() :
	OutputEnabled(false),
	OutputName("OBS"),
	PreviewOutputEnabled(false),
	PreviewOutputName("OBS Preview"),
	TallyProgramEnabled(true),
	TallyPreviewEnabled(true)
{
	config_t* obs_config = obs_frontend_get_global_config();
	if (obs_config) {
		config_set_default_bool(obs_config,
			SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
		config_set_default_string(obs_config,
			SECTION_NAME, PARAM_MAIN_OUTPUT_NAME, OutputName.toUtf8().constData());

		config_set_default_bool(obs_config,
			SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED, PreviewOutputEnabled);
		config_set_default_string(obs_config,
			SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME, PreviewOutputName.toUtf8().constData());

		config_set_default_bool(obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED, TallyProgramEnabled);
		config_set_default_bool(obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED, TallyPreviewEnabled);
	}
}

void Config::Load() {
	config_t* obs_config = obs_frontend_get_global_config();
	if (obs_config) {
		OutputEnabled = config_get_bool(obs_config,
			SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED);
		OutputName = config_get_string(obs_config,
			SECTION_NAME, PARAM_MAIN_OUTPUT_NAME);

		PreviewOutputEnabled = config_get_bool(obs_config,
			SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED);
		PreviewOutputName = config_get_string(obs_config,
			SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME);

		TallyProgramEnabled = config_get_bool(obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED);
		TallyPreviewEnabled = config_get_bool(obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED);
	}
}

void Config::Save() {
	config_t* obs_config = obs_frontend_get_global_config();
	if (obs_config) {
		config_set_bool(obs_config,
			SECTION_NAME, PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
		config_set_string(obs_config,
			SECTION_NAME, PARAM_MAIN_OUTPUT_NAME, OutputName.toUtf8().constData());

		config_set_bool(obs_config,
			SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED, PreviewOutputEnabled);
		config_set_string(obs_config,
			SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME, PreviewOutputName.toUtf8().constData());

		config_set_bool(obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED, TallyProgramEnabled);
		config_set_bool(obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED, TallyPreviewEnabled);

		config_save(obs_config);
	}
}

Config* Config::Current() {
	if (!_instance) {
		_instance = new Config();
	}
	return _instance;
}
