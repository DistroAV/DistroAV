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

typedef struct {
	obs_output_t *output;
} test_output_t;

const char *test_output_getname(void *)
{
	return "Test NDI Output";
}

void *test_output_create(obs_data_t *, obs_output_t *output)
{
	auto o = (test_output_t *)bzalloc(sizeof(test_output_t));
	o->output = output;
	return o;
}

static const std::map<video_format, std::string> test_video_to_color_format_map = {{VIDEO_FORMAT_P010, "P010"},
										   {VIDEO_FORMAT_I010, "I010"},
										   {VIDEO_FORMAT_P216, "P216"},
										   {VIDEO_FORMAT_P416, "P416"}};

bool test_output_start(void *data)
{
	auto o = (test_output_t *)data;

	video_t *video = obs_output_video(o->output);
	obs_output_set_last_error(o->output, "");

	if (video) {
		video_format format = video_output_get_format(video);

		switch (format) {
		case VIDEO_FORMAT_I444:
		case VIDEO_FORMAT_NV12:
		case VIDEO_FORMAT_I420:
		case VIDEO_FORMAT_RGBA:
		case VIDEO_FORMAT_BGRA:
		case VIDEO_FORMAT_BGRX:
			break;

		default:
			obs_log(LOG_ERROR, "ERR-410 - NDI Output cannot start : Unsupported pixel format %d.", format);
			auto error_string = obs_module_text("NDIPlugin.OutputSettings.LastError") +
					    test_video_to_color_format_map.at(format);
			obs_output_set_last_error(o->output, error_string.c_str());
			return false;
		}
	}
	return true;
}

void test_output_stop(void *, uint64_t) {}

void test_output_destroy(void *data)
{
	auto o = (test_output_t *)data;
	bfree(o);
}

void test_output_rawvideo(void *, video_data *) {}

obs_output_info create_test_output_info()
{
	obs_output_info test_output_info = {};
	test_output_info.id = "test_ndi_output";
	test_output_info.flags = OBS_OUTPUT_VIDEO;
	test_output_info.create = test_output_create;
	test_output_info.start = test_output_start;
	test_output_info.get_name = test_output_getname;
	test_output_info.stop = test_output_stop;
	test_output_info.destroy = test_output_destroy;
	test_output_info.raw_video = test_output_rawvideo;
	return test_output_info;
}
