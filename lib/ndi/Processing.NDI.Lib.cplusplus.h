#pragma once

// NOTE : The following MIT license applies to this file ONLY and not to the SDK as a whole. Please review
// the SDK documentation for the description of the full license terms, which are also provided in the file
// "NDI License Agreement.pdf" within the SDK or online at http://new.tk/ndisdk_license/. Your use of any
// part of this SDK is acknowledgment that you agree to the SDK license terms. The full NDI SDK may be
// downloaded at http://ndi.tv/
//
//***********************************************************************************************************
//
// Copyright (C)2014-2022, NewTek, inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files(the "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
// following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
// EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//***********************************************************************************************************


// C++ implementations of default constructors are here to avoid them needing to be inline with all of the
// rest of the code.

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// All the structs used and reasonable defaults are here
inline NDIlib_source_t::NDIlib_source_t(const char* p_ndi_name_, const char* p_url_address_)
	: p_ndi_name(p_ndi_name_), p_url_address(p_url_address_) {}

inline NDIlib_video_frame_v2_t::NDIlib_video_frame_v2_t(int xres_, int yres_, NDIlib_FourCC_video_type_e FourCC_, int frame_rate_N_, int frame_rate_D_,
                                                        float picture_aspect_ratio_, NDIlib_frame_format_type_e frame_format_type_,
                                                        int64_t timecode_, uint8_t* p_data_, int line_stride_in_bytes_, const char* p_metadata_, int64_t timestamp_)
	: xres(xres_), yres(yres_), FourCC(FourCC_), frame_rate_N(frame_rate_N_), frame_rate_D(frame_rate_D_),
	  picture_aspect_ratio(picture_aspect_ratio_), frame_format_type(frame_format_type_),
	  timecode(timecode_), p_data(p_data_), line_stride_in_bytes(line_stride_in_bytes_), p_metadata(p_metadata_), timestamp(timestamp_) {}

inline NDIlib_audio_frame_v2_t::NDIlib_audio_frame_v2_t(int sample_rate_, int no_channels_, int no_samples_, int64_t timecode_, float* p_data_,
                                                        int channel_stride_in_bytes_, const char* p_metadata_, int64_t timestamp_)
	: sample_rate(sample_rate_), no_channels(no_channels_), no_samples(no_samples_), timecode(timecode_),
	  p_data(p_data_), channel_stride_in_bytes(channel_stride_in_bytes_), p_metadata(p_metadata_), timestamp(timestamp_) {}

inline NDIlib_audio_frame_v3_t::NDIlib_audio_frame_v3_t(int sample_rate_, int no_channels_, int no_samples_, int64_t timecode_,
                                                        NDIlib_FourCC_audio_type_e FourCC_, uint8_t* p_data_, int channel_stride_in_bytes_,
                                                        const char* p_metadata_, int64_t timestamp_)
	: sample_rate(sample_rate_), no_channels(no_channels_), no_samples(no_samples_), timecode(timecode_),
	  FourCC(FourCC_), p_data(p_data_), channel_stride_in_bytes(channel_stride_in_bytes_),
	  p_metadata(p_metadata_), timestamp(timestamp_) {}

inline NDIlib_video_frame_t::NDIlib_video_frame_t(int xres_, int yres_, NDIlib_FourCC_video_type_e FourCC_, int frame_rate_N_, int frame_rate_D_,
                                                  float picture_aspect_ratio_, NDIlib_frame_format_type_e frame_format_type_,
                                                  int64_t timecode_, uint8_t* p_data_, int line_stride_in_bytes_)
	: xres(xres_), yres(yres_), FourCC(FourCC_), frame_rate_N(frame_rate_N_), frame_rate_D(frame_rate_D_),
	  picture_aspect_ratio(picture_aspect_ratio_), frame_format_type(frame_format_type_),
	  timecode(timecode_), p_data(p_data_), line_stride_in_bytes(line_stride_in_bytes_) {}

inline NDIlib_audio_frame_t::NDIlib_audio_frame_t(int sample_rate_, int no_channels_, int no_samples_, int64_t timecode_, float* p_data_,
                                                  int channel_stride_in_bytes_)
	: sample_rate(sample_rate_), no_channels(no_channels_), no_samples(no_samples_), timecode(timecode_),
	  p_data(p_data_), channel_stride_in_bytes(channel_stride_in_bytes_) {}

inline NDIlib_metadata_frame_t::NDIlib_metadata_frame_t(int length_, int64_t timecode_, char* p_data_)
	: length(length_), timecode(timecode_), p_data(p_data_) {}

inline NDIlib_tally_t::NDIlib_tally_t(bool on_program_, bool on_preview_)
	: on_program(on_program_), on_preview(on_preview_) {}

inline NDIlib_routing_create_t::NDIlib_routing_create_t(const char* p_ndi_name_, const char* p_groups_)
	: p_ndi_name(p_ndi_name_), p_groups(p_groups_) {}

inline NDIlib_recv_create_v3_t::NDIlib_recv_create_v3_t(const NDIlib_source_t source_to_connect_to_, NDIlib_recv_color_format_e color_format_,
                                                        NDIlib_recv_bandwidth_e bandwidth_, bool allow_video_fields_, const char* p_ndi_name_)
	: source_to_connect_to(source_to_connect_to_), color_format(color_format_), bandwidth(bandwidth_), allow_video_fields(allow_video_fields_), p_ndi_recv_name(p_ndi_name_) {}

inline NDIlib_recv_create_t::NDIlib_recv_create_t(const NDIlib_source_t source_to_connect_to_, NDIlib_recv_color_format_e color_format_,
                                                  NDIlib_recv_bandwidth_e bandwidth_, bool allow_video_fields_)
	: source_to_connect_to(source_to_connect_to_), color_format(color_format_), bandwidth(bandwidth_), allow_video_fields(allow_video_fields_) {}

inline NDIlib_recv_performance_t::NDIlib_recv_performance_t(void)
	: video_frames(0), audio_frames(0), metadata_frames(0) {}

inline NDIlib_recv_queue_t::NDIlib_recv_queue_t(void)
	: video_frames(0), audio_frames(0), metadata_frames(0) {}

inline NDIlib_recv_recording_time_t::NDIlib_recv_recording_time_t(void)
	: no_frames(0), start_time(0), last_time(0) {}

inline NDIlib_send_create_t::NDIlib_send_create_t(const char* p_ndi_name_, const char* p_groups_, bool clock_video_, bool clock_audio_)
	: p_ndi_name(p_ndi_name_), p_groups(p_groups_), clock_video(clock_video_), clock_audio(clock_audio_) {}

inline NDIlib_find_create_t::NDIlib_find_create_t(bool show_local_sources_, const char* p_groups_, const char* p_extra_ips_)
	: show_local_sources(show_local_sources_), p_groups(p_groups_), p_extra_ips(p_extra_ips_) {}

inline NDIlib_audio_frame_interleaved_16s_t::NDIlib_audio_frame_interleaved_16s_t(int sample_rate_, int no_channels_, int no_samples_, int64_t timecode_, int reference_level_, int16_t* p_data_)
	: sample_rate(sample_rate_), no_channels(no_channels_), no_samples(no_samples_), timecode(timecode_),
	  reference_level(reference_level_), p_data(p_data_) {}

inline NDIlib_audio_frame_interleaved_32s_t::NDIlib_audio_frame_interleaved_32s_t(int sample_rate_, int no_channels_, int no_samples_, int64_t timecode_, int reference_level_, int32_t* p_data_)
	: sample_rate(sample_rate_), no_channels(no_channels_), no_samples(no_samples_), timecode(timecode_),
	  reference_level(reference_level_), p_data(p_data_) {}

inline NDIlib_audio_frame_interleaved_32f_t::NDIlib_audio_frame_interleaved_32f_t(int sample_rate_, int no_channels_, int no_samples_, int64_t timecode_, float* p_data_)
	: sample_rate(sample_rate_), no_channels(no_channels_), no_samples(no_samples_), timecode(timecode_), p_data(p_data_) {}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
