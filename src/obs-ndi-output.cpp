/*
obs-ndi (NDI I/O in OBS Studio)
Copyright (C) 2016-2017 Stéphane Lepin <stephane.lepin@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

#include "obs-ndi.h"

struct ndi_output {
    obs_output_t *output;
    const char* ndi_name;
    obs_video_info video_info;
    obs_audio_info audio_info;

    bool started;
    NDIlib_send_instance_t ndi_sender;
};

const char* ndi_output_getname(void* data) {
    UNUSED_PARAMETER(data);
    return obs_module_text("NDIPlugin.OutputName");
}

obs_properties_t* ndi_output_getproperties(void* data) {
    UNUSED_PARAMETER(data);

    obs_properties_t* props = obs_properties_create();
    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

    obs_properties_add_text(props, "ndi_name",
        obs_module_text("NDIPlugin.OutputProps.NDIName"), OBS_TEXT_DEFAULT);

    return props;
}

bool ndi_output_start(void* data) {
    struct ndi_output* o = static_cast<ndi_output*>(data);

    NDIlib_send_create_t send_desc;
    send_desc.p_ndi_name = o->ndi_name;
    send_desc.p_groups = NULL;
    send_desc.clock_video = false;
    send_desc.clock_audio = false;

    o->ndi_sender = ndiLib->NDIlib_send_create(&send_desc);

    if (o->ndi_sender) {
        o->started = true;
        obs_output_begin_data_capture(o->output, 0);
    } else {
        o->started = false;
    }

    return o->started;
}

void ndi_output_stop(void* data, uint64_t ts) {
    struct ndi_output* o = static_cast<ndi_output*>(data);
    o->started = false;
    obs_output_end_data_capture(o->output);
    ndiLib->NDIlib_send_destroy(o->ndi_sender);
}

void* ndi_output_create(obs_data_t* settings, obs_output_t* output) {
    UNUSED_PARAMETER(settings);

    struct ndi_output* o =
        static_cast<ndi_output*>(bzalloc(sizeof(struct ndi_output)));
    o->output = output;

    o->ndi_name = obs_data_get_string(settings, "ndi_name");
    o->started = false;

    obs_get_video_info(&o->video_info);
    obs_get_audio_info(&o->audio_info);

    struct video_scale_info vconv = {};
    vconv.format = VIDEO_FORMAT_UYVY;
    vconv.width = o->video_info.output_width;
    vconv.height = o->video_info.output_height;
    vconv.colorspace = VIDEO_CS_DEFAULT;
    vconv.range = VIDEO_RANGE_DEFAULT;
    obs_output_set_video_conversion(o->output, &vconv);

    return o;
}

void ndi_output_destroy(void* data) {
    struct ndi_output* o = static_cast<ndi_output*>(data);

    if (o)
        bfree(o);
}

void ndi_output_rawvideo(void* data, struct video_data* frame) {
    struct ndi_output* o = static_cast<ndi_output*>(data);
    if (!o->started)
        return;

    uint32_t width = o->video_info.output_width;
    uint32_t height = o->video_info.output_height;

    NDIlib_video_frame_v2_t video_frame = {0};
    video_frame.xres = width;
    video_frame.yres = height;
    video_frame.FourCC = NDIlib_FourCC_type_UYVY;
    video_frame.frame_rate_N = o->video_info.fps_num;
    video_frame.frame_rate_D = o->video_info.fps_den;
    video_frame.picture_aspect_ratio = (float)width / (float)height;
    video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
    
    video_frame.timecode = frame->timestamp;
    video_frame.p_data = frame->data[0];
    video_frame.line_stride_in_bytes = frame->linesize[0];

    ndiLib->NDIlib_send_send_video_async_v2(o->ndi_sender, &video_frame);
}

void ndi_output_rawaudio(void* data, struct audio_data* frame) {
    struct ndi_output* o = static_cast<ndi_output*>(data);
    if (!o->started) return;

    NDIlib_audio_frame_v2_t audio_frame = {0};
    audio_frame.sample_rate = o->audio_info.samples_per_sec;
    audio_frame.no_channels = o->audio_info.speakers;
    audio_frame.no_samples = frame->frames;
    audio_frame.channel_stride_in_bytes = frame->frames * 4;

    size_t data_size =
        audio_frame.no_channels * audio_frame.channel_stride_in_bytes;
    uint8_t* audio_data = (uint8_t*)bmalloc(data_size);

    for (int i = 0; i < audio_frame.no_channels; i++) {
        memcpy(&audio_data[i * audio_frame.channel_stride_in_bytes],
            frame->data[i],
            audio_frame.channel_stride_in_bytes);
    }

    audio_frame.p_data = (float*)audio_data;
    audio_frame.timecode = frame->timestamp;

    ndiLib->NDIlib_send_send_audio_v2(o->ndi_sender, &audio_frame);
    bfree(audio_data);
}

struct obs_output_info create_ndi_output_info() {
    struct obs_output_info ndi_output_info = {};
    ndi_output_info.id				= "ndi_output";
    ndi_output_info.flags			= OBS_OUTPUT_AV;
    ndi_output_info.get_name		= ndi_output_getname;
    ndi_output_info.get_properties	= ndi_output_getproperties;
    ndi_output_info.create			= ndi_output_create;
    ndi_output_info.destroy			= ndi_output_destroy;
    ndi_output_info.start			= ndi_output_start;
    ndi_output_info.stop			= ndi_output_stop;
    ndi_output_info.raw_video		= ndi_output_rawvideo;
    ndi_output_info.raw_audio		= ndi_output_rawaudio;

    return ndi_output_info;
}
