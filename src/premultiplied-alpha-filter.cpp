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

#include <obs-module.h>

#include "obs-ndi.h"

struct alpha_filter {
	obs_source_t* context;
	gs_effect_t* effect;
};

const char* alpha_filter_getname(void* data) {
	UNUSED_PARAMETER(data);
	return obs_module_text("NDIPlugin.PremultipliedAlphaFilterName");
}

void* alpha_filter_create(obs_data_t* settings, obs_source_t* source) {
	struct alpha_filter* s =
		(struct alpha_filter*)bzalloc(sizeof(struct alpha_filter));
	s->context = source;
	s->effect = obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
	return s;
}

void alpha_filter_destroy(void* data) {
	struct alpha_filter* s = (struct alpha_filter*)data;
	bfree(s);
}

void alpha_filter_videorender(void* data, gs_effect_t* effect) {
	UNUSED_PARAMETER(effect);
	struct alpha_filter* s = (struct alpha_filter*)data;

	if (!obs_source_process_filter_begin(s->context, GS_RGBA,
		OBS_ALLOW_DIRECT_RENDERING))
		return;

	obs_source_process_filter_end(s->context, s->effect, 0, 0);
}

extern const struct obs_source_info alpha_filter_info = {
	.id = OBS_NDI_ALPHA_FILTER_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = alpha_filter_getname,
	.create = alpha_filter_create,
	.destroy = alpha_filter_destroy,
	.video_render = alpha_filter_videorender,
};
