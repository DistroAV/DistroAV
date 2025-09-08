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

#include "plugin-main.h"

#include <util/platform.h>
#include <util/threading.h>

#include <QDesktopServices>
#include <QUrl>

#include <thread>

#define PROP_SOURCE "source_name"
#define PROP_URL "url"

typedef struct {
	obs_source_t *obs_source;

} whip_source_t;

const char *whip_source_getname(void *)
{
	return "WHIP";
}

obs_properties_t *whip_source_getproperties(void *data)
{
	auto s = (whip_source_t *)data;
	obs_log(LOG_DEBUG, "+whip_source_getproperties(…)");

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, PROP_URL, "WHIP Endpoint", OBS_TEXT_DEFAULT );
	
	return props;
}

void whip_source_getdefaults(obs_data_t *settings)
{
	obs_log(LOG_DEBUG, "+whip_source_getdefaults(…)");
	obs_data_set_default_string(settings, PROP_URL, "https://127.0.0.1:8080/whip");

	obs_log(LOG_DEBUG, "-whip_source_getdefaults(…)");
}

void whip_source_update(void *data, obs_data_t *settings)
{
	auto s = (whip_source_t *)data;
	auto obs_source = s->obs_source;
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_update(…)", obs_source_name);

	obs_log(LOG_DEBUG, "'%s' -whip_source_update(…)", obs_source_name);
}

void whip_source_shown(void *data)
{

}

void whip_source_hidden(void *data)
{

}

void whip_source_activated(void *data)
{
	
}

void whip_source_deactivated(void *data)
{

}

void *whip_source_create(obs_data_t *settings, obs_source_t *obs_source)
{
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_create(…)", obs_source_name);

	auto s = (whip_source_t *)bzalloc(sizeof(whip_source_t));
	s->obs_source = obs_source;

	whip_source_update(s, settings);

	obs_log(LOG_DEBUG, "'%s' -whip_source_create(…)", obs_source_name);

	return s;
}

void whip_source_destroy(void *data)
{
	auto s = (whip_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_destroy(…)", obs_source_name);

	bfree(s);

	obs_log(LOG_DEBUG, "'%s' -whip_source_destroy(…)", obs_source_name);
}

uint32_t whip_source_get_width(void *data)
{
	auto s = (whip_source_t *)data;
	return 1920;
}

uint32_t whip_source_get_height(void *data)
{
	auto s = (whip_source_t *)data;
	return 1080;
}

obs_source_info create_whip_source_info()
{
	// https://docs.obsproject.com/reference-sources#source-definition-structure-obs-source-info
	obs_source_info whip_source_info = {};
	whip_source_info.id = "whip_source";
	whip_source_info.type = OBS_SOURCE_TYPE_INPUT;
	whip_source_info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;

	whip_source_info.get_name = whip_source_getname;
	whip_source_info.get_properties = whip_source_getproperties;
	whip_source_info.get_defaults = whip_source_getdefaults;

	whip_source_info.create = whip_source_create;
	whip_source_info.activate = whip_source_activated;
	whip_source_info.show = whip_source_shown;
	whip_source_info.update = whip_source_update;
	whip_source_info.hide = whip_source_hidden;
	whip_source_info.deactivate = whip_source_deactivated;
	whip_source_info.destroy = whip_source_destroy;

	whip_source_info.get_width = whip_source_get_width;
	whip_source_info.get_height = whip_source_get_height;

	return whip_source_info;
}
