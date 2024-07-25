/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#include "config.h"

#include "plugin-main.h"

#include <util/config-file.h>

#include <QCoreApplication>

#define SECTION_NAME "NDIPlugin"
#define PARAM_MAIN_OUTPUT_ENABLED "MainOutputEnabled"
#define PARAM_MAIN_OUTPUT_NAME "MainOutputName"
#define PARAM_MAIN_OUTPUT_GROUPS "MainOutputGroups"
#define PARAM_PREVIEW_OUTPUT_ENABLED "PreviewOutputEnabled"
#define PARAM_PREVIEW_OUTPUT_NAME "PreviewOutputName"
#define PARAM_PREVIEW_OUTPUT_GROUPS "PreviewOutputGroups"
#define PARAM_TALLY_PROGRAM_ENABLED "TallyProgramEnabled"
#define PARAM_TALLY_PREVIEW_ENABLED "TallyPreviewEnabled"
#define PARAM_AUTO_CHECK_FOR_UPDATES "AutoCheckForUpdates"
#define PARAM_SKIP_UPDATE_VERSION "SkipUpdateVersion"

Config *Config::_instance = nullptr;

bool _LogVerbose = false;
bool _LogDebug = false;
int _LogLevel = LOG_INFO;
bool _UpdateForce = false;

bool Config::LogVerbose()
{
	return _LogVerbose;
}

bool Config::LogDebug()
{
	return _LogDebug;
}

bool Config::UpdateForce()
{
	return _UpdateForce;
}

Config::Config()
	: OutputEnabled(false),
	  OutputName("OBS"),
	  OutputGroups(""),
	  PreviewOutputEnabled(false),
	  PreviewOutputName("OBS Preview"),
	  PreviewOutputGroups(""),
	  TallyProgramEnabled(true),
	  TallyPreviewEnabled(true)
{
	auto arguments = QCoreApplication::arguments();
	if (arguments.contains("--DistroAV-verbose",
			       Qt::CaseSensitivity::CaseInsensitive)) {
		blog(LOG_INFO,
		     "[DistroAV] config: DistroAV verbose logging enabled");
		_LogVerbose = true;
	}
	if (arguments.contains("--DistroAV-debug",
			       Qt::CaseSensitivity::CaseInsensitive)) {
		blog(LOG_INFO,
		     "[DistroAV] config: DistroAV debug logging enabled");
		_LogDebug = true;
	}
	if (arguments.contains("--DistroAV-update-force",
			       Qt::CaseSensitivity::CaseInsensitive)) {
		blog(LOG_INFO,
		     "[DistroAV] config: DistroAV update force enabled");
		_UpdateForce = true;
	}

	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_MAIN_OUTPUT_ENABLED,
					OutputEnabled);
		config_set_default_string(obs_config, SECTION_NAME,
					  PARAM_MAIN_OUTPUT_NAME,
					  QT_TO_UTF8(OutputName));
		config_set_default_string(obs_config, SECTION_NAME,
					  PARAM_MAIN_OUTPUT_GROUPS,
					  QT_TO_UTF8(OutputGroups));

		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_PREVIEW_OUTPUT_ENABLED,
					PreviewOutputEnabled);
		config_set_default_string(obs_config, SECTION_NAME,
					  PARAM_PREVIEW_OUTPUT_NAME,
					  QT_TO_UTF8(PreviewOutputName));
		config_set_default_string(obs_config, SECTION_NAME,
					  PARAM_PREVIEW_OUTPUT_GROUPS,
					  QT_TO_UTF8(PreviewOutputGroups));

		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_TALLY_PROGRAM_ENABLED,
					TallyProgramEnabled);
		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_TALLY_PREVIEW_ENABLED,
					TallyPreviewEnabled);

		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_AUTO_CHECK_FOR_UPDATES, true);
	}
}

Config *Config::Load()
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		OutputEnabled = config_get_bool(obs_config, SECTION_NAME,
						PARAM_MAIN_OUTPUT_ENABLED);
		OutputName = config_get_string(obs_config, SECTION_NAME,
					       PARAM_MAIN_OUTPUT_NAME);
		OutputGroups = config_get_string(obs_config, SECTION_NAME,
						 PARAM_MAIN_OUTPUT_GROUPS);

		PreviewOutputEnabled = config_get_bool(
			obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_ENABLED);
		PreviewOutputName = config_get_string(
			obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME);
		PreviewOutputGroups = config_get_string(
			obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_GROUPS);

		TallyProgramEnabled = config_get_bool(
			obs_config, SECTION_NAME, PARAM_TALLY_PROGRAM_ENABLED);
		TallyPreviewEnabled = config_get_bool(
			obs_config, SECTION_NAME, PARAM_TALLY_PREVIEW_ENABLED);
	}
	return this;
}

Config *Config::Save()
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		config_set_bool(obs_config, SECTION_NAME,
				PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_MAIN_OUTPUT_NAME,
				  QT_TO_UTF8(OutputName));
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_MAIN_OUTPUT_GROUPS,
				  QT_TO_UTF8(OutputGroups));

		config_set_bool(obs_config, SECTION_NAME,
				PARAM_PREVIEW_OUTPUT_ENABLED,
				PreviewOutputEnabled);
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_PREVIEW_OUTPUT_NAME,
				  QT_TO_UTF8(PreviewOutputName));
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_PREVIEW_OUTPUT_GROUPS,
				  QT_TO_UTF8(PreviewOutputGroups));

		config_set_bool(obs_config, SECTION_NAME,
				PARAM_TALLY_PROGRAM_ENABLED,
				TallyProgramEnabled);
		config_set_bool(obs_config, SECTION_NAME,
				PARAM_TALLY_PREVIEW_ENABLED,
				TallyPreviewEnabled);

		config_save(obs_config);
	}
	return this;
}

bool Config::AutoCheckForUpdates()
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		return config_get_bool(obs_config, SECTION_NAME,
				       PARAM_AUTO_CHECK_FOR_UPDATES);
	}
	return false;
}

void Config::AutoCheckForUpdates(bool value)
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		config_set_bool(obs_config, SECTION_NAME,
				PARAM_AUTO_CHECK_FOR_UPDATES, value);
		config_save(obs_config);
	}
}

void Config::SkipUpdateVersion(const QVersionNumber &version)
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_SKIP_UPDATE_VERSION,
				  QT_TO_UTF8(version.toString()));
		config_save(obs_config);
	}
}

QVersionNumber Config::SkipUpdateVersion()
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		auto version = config_get_string(obs_config, SECTION_NAME,
						 PARAM_SKIP_UPDATE_VERSION);
		if (version) {
			return QVersionNumber::fromString(version);
		}
	}
	return QVersionNumber();
}

Config *Config::Current()
{
	if (!_instance) {
		_instance = new Config();
	}
	return _instance;
}

void Config::Destroy()
{
	if (_instance) {
		delete _instance;
		_instance = nullptr;
	}
}
