/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "Config.h"

#include <obs-frontend-api.h>

#include <QtCore/QCoreApplication>

#define CONFIG_SECTION_NAME "NDIPlugin"
#define PARAM_MAIN_OUTPUT_ENABLED "MainOutputEnabled"
#define PARAM_MAIN_OUTPUT_NAME "MainOutputName"
#define PARAM_PREVIEW_OUTPUT_ENABLED "PreviewOutputEnabled"
#define PARAM_PREVIEW_OUTPUT_NAME "PreviewOutputName"
#define PARAM_TALLY_PROGRAM_ENABLED "TallyProgramEnabled"
#define PARAM_TALLY_PREVIEW_ENABLED "TallyPreviewEnabled"

Config::Config()
	: OutputEnabled(false),
	  OutputName("OBS"),
	  PreviewOutputEnabled(false),
	  PreviewOutputName("OBS Preview"),
	  TallyProgramEnabled(true),
	  TallyPreviewEnabled(true),
	  VerboseLog(false)
{
	SetDefaultsToGlobalStore();

	QStringList arguments = QCoreApplication::arguments();
	if (arguments.contains("--obs-ndi-verbose")) {
		blog(LOG_INFO, "[obs-ndi] obs-ndi verbose logging enabled");
		VerboseLog = true;
	}
}

config_t *Config::GetConfigStore()
{
	return obs_frontend_get_global_config();
}

void Config::SetDefaultsToGlobalStore()
{
	config_t *obs_config = GetConfigStore();
	if (!obs_config) {
		blog(LOG_ERROR,
		     "[Config::SetDefaultsToGlobalStore] Unable to fetch OBS config!");
		return;
	}

	config_set_default_bool(obs_config, CONFIG_SECTION_NAME,
				PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
	config_set_default_string(obs_config, CONFIG_SECTION_NAME,
				  PARAM_MAIN_OUTPUT_NAME,
				  OutputName.toUtf8().constData());

	config_set_default_bool(obs_config, CONFIG_SECTION_NAME,
				PARAM_PREVIEW_OUTPUT_ENABLED,
				PreviewOutputEnabled);
	config_set_default_string(obs_config, CONFIG_SECTION_NAME,
				  PARAM_PREVIEW_OUTPUT_NAME,
				  PreviewOutputName.toUtf8().constData());

	config_set_default_bool(obs_config, CONFIG_SECTION_NAME,
				PARAM_TALLY_PROGRAM_ENABLED,
				TallyProgramEnabled);
	config_set_default_bool(obs_config, CONFIG_SECTION_NAME,
				PARAM_TALLY_PREVIEW_ENABLED,
				TallyPreviewEnabled);
}

void Config::Load()
{
	config_t *obs_config = GetConfigStore();
	if (!obs_config) {
		blog(LOG_ERROR, "[Config::Load] Unable to fetch OBS config!");
		return;
	}

	OutputEnabled = config_get_bool(obs_config, CONFIG_SECTION_NAME,
					PARAM_MAIN_OUTPUT_ENABLED);
	OutputName = config_get_string(obs_config, CONFIG_SECTION_NAME,
				       PARAM_MAIN_OUTPUT_NAME);

	PreviewOutputEnabled = config_get_bool(obs_config, CONFIG_SECTION_NAME,
					       PARAM_PREVIEW_OUTPUT_ENABLED);
	PreviewOutputName = config_get_string(obs_config, CONFIG_SECTION_NAME,
					      PARAM_PREVIEW_OUTPUT_NAME);

	TallyProgramEnabled = config_get_bool(obs_config, CONFIG_SECTION_NAME,
					      PARAM_TALLY_PROGRAM_ENABLED);
	TallyPreviewEnabled = config_get_bool(obs_config, CONFIG_SECTION_NAME,
					      PARAM_TALLY_PREVIEW_ENABLED);
}

void Config::Save()
{
	config_t *obs_config = GetConfigStore();
	if (!obs_config) {
		blog(LOG_ERROR, "[Config::Save] Unable to fetch OBS config!");
		return;
	}

	config_set_bool(obs_config, CONFIG_SECTION_NAME,
			PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
	config_set_string(obs_config, CONFIG_SECTION_NAME,
			  PARAM_MAIN_OUTPUT_NAME,
			  OutputName.toUtf8().constData());

	config_set_bool(obs_config, CONFIG_SECTION_NAME,
			PARAM_PREVIEW_OUTPUT_ENABLED, PreviewOutputEnabled);
	config_set_string(obs_config, CONFIG_SECTION_NAME,
			  PARAM_PREVIEW_OUTPUT_NAME,
			  PreviewOutputName.toUtf8().constData());

	config_set_bool(obs_config, CONFIG_SECTION_NAME,
			PARAM_TALLY_PROGRAM_ENABLED, TallyProgramEnabled);
	config_set_bool(obs_config, CONFIG_SECTION_NAME,
			PARAM_TALLY_PREVIEW_ENABLED, TallyPreviewEnabled);

	config_save(obs_config);
}
