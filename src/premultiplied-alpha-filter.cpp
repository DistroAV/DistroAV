/*
obs-ndi
Copyright (C) 2016-2023 Stéphane Lepin <stephane.lepin@gmail.com>

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

#include <obs-module.h>

#include "obs-ndi.h"

struct alpha_filter {
	obs_source_t *context;
	gs_effect_t *effect;
};

const char *alpha_filter_getname(void *data)
{
	UNUSED_PARAMETER(data);
	return obs_module_text("NDIPlugin.PremultipliedAlphaFilterName");
}

obs_properties_t *alpha_filter_getproperties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	return props;
}

void alpha_filter_update(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

void *alpha_filter_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	struct alpha_filter *s =
		(struct alpha_filter *)bzalloc(sizeof(struct alpha_filter));
	s->context = source;
	s->effect = obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
	return s;
}

void alpha_filter_destroy(void *data)
{
	struct alpha_filter *s = (struct alpha_filter *)data;
	bfree(s);
}

void alpha_filter_videorender(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct alpha_filter *s = (struct alpha_filter *)data;

	if (!obs_source_process_filter_begin(s->context, GS_RGBA,
					     OBS_ALLOW_DIRECT_RENDERING))
		return;

	obs_source_process_filter_end(s->context, s->effect, 0, 0);
}

struct obs_source_info create_alpha_filter_info()
{
	struct obs_source_info alpha_filter_info = {};
	alpha_filter_info.id = OBS_NDI_ALPHA_FILTER_ID;
	alpha_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	alpha_filter_info.output_flags = OBS_SOURCE_VIDEO;
	alpha_filter_info.get_name = alpha_filter_getname;
	alpha_filter_info.get_properties = alpha_filter_getproperties;
	alpha_filter_info.create = alpha_filter_create;
	alpha_filter_info.destroy = alpha_filter_destroy;
	alpha_filter_info.update = alpha_filter_update;
	alpha_filter_info.video_render = alpha_filter_videorender;

	return alpha_filter_info;
}
