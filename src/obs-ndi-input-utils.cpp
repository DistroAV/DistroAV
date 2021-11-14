#include <obs-module.h>

#include "obs-ndi.h"
#include "obs-ndi-input.h"

enum video_format ndi_video_format_to_obs(enum NDIlib_FourCC_video_type_e in)
{
	switch (in) {
		case NDIlib_FourCC_type_UYVY:
		case NDIlib_FourCC_type_UYVA:
			return VIDEO_FORMAT_UYVY;

		case NDIlib_FourCC_type_I420:
			return VIDEO_FORMAT_I420;

		case NDIlib_FourCC_type_NV12:
			return VIDEO_FORMAT_NV12;

		case NDIlib_FourCC_type_BGRA:
			return VIDEO_FORMAT_BGRA;

		case NDIlib_FourCC_type_BGRX:
			return VIDEO_FORMAT_BGRX;

		case NDIlib_FourCC_type_RGBX:
		case NDIlib_FourCC_type_RGBA:
			return VIDEO_FORMAT_RGBA;

		default:
			blog(LOG_WARNING, "Unsupported NDI video pixel format: %d", in);
			return VIDEO_FORMAT_NONE;
	}
}

enum speaker_layout ndi_audio_layout_to_obs(size_t channel_count)
{
	switch (channel_count) {
		case 1:
			return SPEAKERS_MONO;
		case 2:
			return SPEAKERS_STEREO;
		case 3:
			return SPEAKERS_2POINT1;
		case 4:
			return SPEAKERS_4POINT0;
		case 5:
			return SPEAKERS_4POINT1;
		case 6:
			return SPEAKERS_5POINT1;
		case 8:
			return SPEAKERS_7POINT1;
		default:
			return SPEAKERS_UNKNOWN;
	}
}
