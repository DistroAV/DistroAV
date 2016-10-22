#ifdef _WIN32
#include <Windows.h>
#endif
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <Processing.NDI.Lib.h>

extern NDIlib_find_instance_t ndi_finder;

struct ndi_source {
	obs_source_t *source;
	NDIlib_recv_instance_t ndi_receiver;
	pthread_t frame_thread;
	bool running;
	DWORD no_sources;
	const NDIlib_source_t *ndi_sources;
};

const char* ndi_source_getname(void *data) {
	UNUSED_PARAMETER(data);
	return "NDI Source";
}

obs_properties_t* ndi_source_getproperties(void *data) {
	struct ndi_source *s = static_cast<ndi_source *>(data);

	obs_properties_t* props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	obs_property_t* source_list = obs_properties_add_list(props, "ndi_source", "NDI source name",
		OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);

	obs_property_set_modified_callback(source_list, [](obs_properties_t *pps, obs_property_t *p, obs_data_t *settings) {
		size_t selected_item = obs_data_get_int(settings, "ndi_source");
		obs_data_set_string(settings, "ndi_source_name", obs_property_list_item_name(p, selected_item));
		obs_data_set_string(settings, "ndi_source_ip", obs_property_list_item_string(p, selected_item));
		return true;
	});

	s->no_sources = 0;
	s->ndi_sources = NDIlib_find_get_sources(ndi_finder, &s->no_sources, 0);
	
	for (uint32_t i = 0; i < s->no_sources; i++) {
		obs_property_list_add_int(source_list, s->ndi_sources[i].p_ndi_name, i);
	}

	return props;
}

void *ndi_source_pollframe(void *data) {
	struct ndi_source *s = static_cast<ndi_source *>(data);

	NDIlib_video_frame_t video_frame;
	NDIlib_audio_frame_t audio_frame;
	NDIlib_audio_frame_interleaved_16s_t audio_frame_16s;

	obs_source_frame obs_video_frame = { 0 };
	obs_source_audio obs_audio_frame = { 0 };

	uint8_t *audio_data;
	long audio_data_size;

	while (s->running) {
		switch (NDIlib_recv_capture(s->ndi_receiver, &video_frame, &audio_frame, NULL, 0)) {
			case NDIlib_frame_type_video:
				video_format frame_format;
				switch (video_frame.FourCC) {
				case NDIlib_FourCC_type_BGRA:
					frame_format = VIDEO_FORMAT_BGRA;
					break;
				case NDIlib_FourCC_type_BGRX:
					frame_format = VIDEO_FORMAT_BGRX;
					break;
				}

				obs_video_frame.width = video_frame.xres;
				obs_video_frame.height = video_frame.yres;
				obs_video_frame.format = frame_format;
				obs_video_frame.linesize[0] = video_frame.line_stride_in_bytes;
				obs_video_frame.data[0] = video_frame.p_data;

				obs_source_output_video(s->source, &obs_video_frame);

				NDIlib_recv_free_video(s->ndi_receiver, &video_frame);
				break;

			case NDIlib_frame_type_audio:
				audio_frame_16s.reference_level = 0;
				audio_frame_16s.p_data = static_cast<short *>(malloc(audio_frame.no_samples * audio_frame.no_channels * sizeof(short)));
				NDIlib_util_audio_to_interleaved_16s(&audio_frame, &audio_frame_16s);

				switch (audio_frame.no_channels) {
				case 1:
					obs_audio_frame.speakers = SPEAKERS_MONO;
					break;
				case 2:
					obs_audio_frame.speakers = SPEAKERS_STEREO;
					break;
				case 3:
					obs_audio_frame.speakers = SPEAKERS_2POINT1;
					break;
				case 4:
					obs_audio_frame.speakers = SPEAKERS_QUAD;
					break;
				case 5:
					obs_audio_frame.speakers = SPEAKERS_4POINT1;
					break;
				case 6:
					obs_audio_frame.speakers = SPEAKERS_5POINT1;
					break;
				case 8:
					obs_audio_frame.speakers = SPEAKERS_7POINT1;
					break;
				default:
					obs_audio_frame.speakers = SPEAKERS_UNKNOWN;
				}

				obs_audio_frame.timestamp = os_gettime_ns() - (audio_frame_16s.sample_rate * audio_frame_16s.no_samples);
				obs_audio_frame.samples_per_sec = audio_frame_16s.sample_rate;
				obs_audio_frame.format = AUDIO_FORMAT_16BIT;
				obs_audio_frame.frames = audio_frame_16s.no_samples;
				obs_audio_frame.data[0] = (uint8_t*)(void*)audio_frame_16s.p_data;

				obs_source_output_audio(s->source, &obs_audio_frame);

				NDIlib_recv_free_audio(s->ndi_receiver, &audio_frame);
				break;

			case NDIlib_frame_type_none:
				break;
		}
	}

	return NULL;
}

void ndi_source_update(void *data, obs_data_t *settings) {
	struct ndi_source *s = static_cast<ndi_source *>(data);

	NDIlib_source_t selected_source;
	selected_source.p_ndi_name = obs_data_get_string(settings, "ndi_source_name");
	selected_source.p_ip_address = obs_data_get_string(settings, "ndi_source_ip");

	NDIlib_recv_create_t recv_desc;
	recv_desc.source_to_connect_to = selected_source;
	recv_desc.color_format = NDIlib_recv_color_format_BGRX_BGRA;
	recv_desc.bandwidth = NDIlib_recv_bandwidth_highest;
	recv_desc.allow_video_fields = true;

	blog(LOG_INFO, "[obs-ndi] updating OBS source '%s' with NDI source '%s'", obs_source_get_name(s->source), recv_desc.source_to_connect_to.p_ndi_name);

	s->running = false;
	pthread_cancel(s->frame_thread);
	NDIlib_recv_destroy(s->ndi_receiver);
	
	s->ndi_receiver = NDIlib_recv_create2(&recv_desc);
	if (s->ndi_receiver) {
		blog(LOG_INFO, "[obs-ndi] OBS source '%s' updated successfully", obs_source_get_name(s->source));	
		s->running = true;
		pthread_create(&s->frame_thread, NULL, ndi_source_pollframe, data);
	}
	else {
		blog(LOG_ERROR, "[obs-ndi] can't create a receiver for NDI source '%s'", recv_desc.source_to_connect_to.p_ndi_name);
	}
}

void* ndi_source_create(obs_data_t *settings, obs_source_t *source) {
	UNUSED_PARAMETER(settings);

	struct ndi_source *s = static_cast<ndi_source *>(bzalloc(sizeof(struct ndi_source)));
	s->source = source;
	s->running = true;

	ndi_source_update(s, settings);
	return s;
}

void ndi_source_destroy(void *data) {
	struct ndi_source *s = static_cast<ndi_source *>(data);
	s->running = false;
	NDIlib_recv_destroy(s->ndi_receiver);
	// TODO : free s->ndi_sources and s
}

struct obs_source_info create_ndi_source_info() {
	struct obs_source_info ndi_source_info = {};
	ndi_source_info.id				= "ndi_source";
	ndi_source_info.type			= OBS_SOURCE_TYPE_INPUT;
	ndi_source_info.output_flags	= OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO |
									  OBS_SOURCE_DO_NOT_DUPLICATE;
	ndi_source_info.get_name		= ndi_source_getname;
	ndi_source_info.get_properties	= ndi_source_getproperties;
	ndi_source_info.update			= ndi_source_update;
	ndi_source_info.create			= ndi_source_create;
	ndi_source_info.destroy			= ndi_source_destroy;

	return ndi_source_info;
}

