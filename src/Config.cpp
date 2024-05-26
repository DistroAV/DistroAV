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
#include "plugin-main.h"

#include <QtCore/QCoreApplication>

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <QRandomGenerator>

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

config_t *GetGlobalConfig()
{
	return obs_frontend_get_global_config();
}

// Copied from OBS UI/update/shared-update.cpp GenerateGUID(...)
void GenerateGUID(std::string &guid)
{
	const char alphabet[] = "0123456789abcdef";
	QRandomGenerator *rng = QRandomGenerator::system();

	guid.resize(40);

	for (size_t i = 0; i < 40; i++) {
		guid[i] = alphabet[rng->bounded(0, 16)];
	}
}

// Copied from OBS UI/update/shared-update.cpp GetProgramGUID()
// Changed only to return QString instead of std::string
QString Config::GetProgramGUID()
{
	static std::mutex m;
	std::lock_guard<std::mutex> lock(m);

	/* NOTE: this is an arbitrary random number that we use to count the
	 * number of unique OBS installations and is not associated with any
	 * kind of identifiable information */
	const char *pguid =
		config_get_string(GetGlobalConfig(), "General", "InstallGUID");
	std::string guid;
	if (pguid)
		guid = pguid;

	if (guid.empty()) {
		GenerateGUID(guid);

		if (!guid.empty())
			config_set_string(GetGlobalConfig(), "General",
					  "InstallGUID", guid.c_str());
	}

	return QString(guid.c_str());
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
				  version.toString().toUtf8().constData());
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

Config *Config::_instance = nullptr;

Config *Config::Current()
{
	if (!_instance) {
		_instance = new Config();
	}
	return _instance;
}

bool Config::VerboseLog()
{
	return Current()->_VerboseLog;
}

Config::Config()
	: OutputEnabled(false),
	  OutputName("OBS"),
	  OutputGroups(""),
	  PreviewOutputEnabled(false),
	  PreviewOutputName("OBS Preview"),
	  PreviewOutputGroups(""),
	  TallyProgramEnabled(true),
	  TallyPreviewEnabled(true),
	  _VerboseLog(false)
{
	auto arguments = QCoreApplication::arguments();
	if (arguments.contains("--obs-ndi-verbose")) {
		blog(LOG_INFO,
		     "[obs-ndi] Config: obs-ndi verbose logging enabled");
		_VerboseLog = true;
	}

	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_MAIN_OUTPUT_ENABLED,
					OutputEnabled);
		config_set_default_string(obs_config, SECTION_NAME,
					  PARAM_MAIN_OUTPUT_NAME,
					  OutputName.toUtf8().constData());
		config_set_default_string(obs_config, SECTION_NAME,
					  PARAM_MAIN_OUTPUT_GROUPS,
					  OutputGroups.toUtf8().constData());

		config_set_default_bool(obs_config, SECTION_NAME,
					PARAM_PREVIEW_OUTPUT_ENABLED,
					PreviewOutputEnabled);
		config_set_default_string(
			obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_NAME,
			PreviewOutputName.toUtf8().constData());
		config_set_default_string(
			obs_config, SECTION_NAME, PARAM_PREVIEW_OUTPUT_GROUPS,
			PreviewOutputGroups.toUtf8().constData());

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

void Config::Load()
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
}

void Config::Save()
{
	auto obs_config = GetGlobalConfig();
	if (obs_config) {
		config_set_bool(obs_config, SECTION_NAME,
				PARAM_MAIN_OUTPUT_ENABLED, OutputEnabled);
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_MAIN_OUTPUT_NAME,
				  OutputName.toUtf8().constData());
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_MAIN_OUTPUT_GROUPS,
				  OutputGroups.toUtf8().constData());

		config_set_bool(obs_config, SECTION_NAME,
				PARAM_PREVIEW_OUTPUT_ENABLED,
				PreviewOutputEnabled);
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_PREVIEW_OUTPUT_NAME,
				  PreviewOutputName.toUtf8().constData());
		config_set_string(obs_config, SECTION_NAME,
				  PARAM_PREVIEW_OUTPUT_GROUPS,
				  PreviewOutputGroups.toUtf8().constData());

		config_set_bool(obs_config, SECTION_NAME,
				PARAM_TALLY_PROGRAM_ENABLED,
				TallyProgramEnabled);
		config_set_bool(obs_config, SECTION_NAME,
				PARAM_TALLY_PREVIEW_ENABLED,
				TallyPreviewEnabled);

		config_save(obs_config);
	}
}
