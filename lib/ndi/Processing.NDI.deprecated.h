#pragma once

// NOTE : The following MIT license applies to this file ONLY and not to the SDK as a whole. Please review
// the SDK documentation for the description of the full license terms, which are also provided in the file
// "NDI License Agreement.pdf" within the SDK or online at http://ndi.link/ndisdk_license. Your use of any
// part of this SDK is acknowledgment that you agree to the SDK license terms. The full NDI SDK may be
// downloaded at http://ndi.video/
//
//***********************************************************************************************************
//
// 
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

// This describes a video frame
PROCESSINGNDILIB_DEPRECATED
typedef struct NDIlib_video_frame_t {
	// The resolution of this frame.
	int xres, yres;

	// What FourCC this is with. This can be two values.
	NDIlib_FourCC_video_type_e FourCC;

	// What is the frame rate of this frame.
	// For instance NTSC is 30000,1001 = 30000/1001 = 29.97 fps
	int frame_rate_N, frame_rate_D;

	// What is the picture aspect ratio of this frame.
	// For instance 16.0/9.0 = 1.778 is 16:9 video. If this is zero, then square pixels are assumed (xres/yres).
	float picture_aspect_ratio;

	// Is this a fielded frame, or is it progressive.
	NDIlib_frame_format_type_e frame_format_type;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The video data itself.
	uint8_t* p_data;

	// The inter-line stride of the video data, in bytes.
	int line_stride_in_bytes;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_video_frame_t(
		int xres_ = 0, int yres_ = 0,
		NDIlib_FourCC_video_type_e FourCC_ = NDIlib_FourCC_type_UYVY,
		int frame_rate_N_ = 30000, int frame_rate_D_ = 1001,
		float picture_aspect_ratio_ = 0.0f,
		NDIlib_frame_format_type_e frame_format_type_ = NDIlib_frame_format_type_progressive,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		uint8_t* p_data_ = NULL, int line_stride_in_bytes_ = 0
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_video_frame_t;

// This describes an audio frame
PROCESSINGNDILIB_DEPRECATED
typedef struct NDIlib_audio_frame_t {
	// The sample-rate of this buffer.
	int sample_rate;

	// The number of audio channels.
	int no_channels;

	// The number of audio samples per channel.
	int no_samples;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The audio data.
	float* p_data;

	// The inter channel stride of the audio channels, in bytes.
	int channel_stride_in_bytes;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_t(
		int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		float* p_data_ = NULL, int channel_stride_in_bytes_ = 0
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_audio_frame_t;

// For legacy reasons I called this the wrong thing. For backwards compatibility.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_find_instance_t NDIlib_find_create2(const NDIlib_find_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_find_instance_t NDIlib_find_create(const NDIlib_find_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// DEPRECATED. This function is basically exactly the following and was confusing to use.
//    if ((!timeout_in_ms) || (NDIlib_find_wait_for_sources(timeout_in_ms)))
//        return NDIlib_find_get_current_sources(p_instance, p_no_sources);
//    return NULL;
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_source_t* NDIlib_find_get_sources(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources, uint32_t timeout_in_ms);

// The creation structure that is used when you are creating a receiver.
PROCESSINGNDILIB_DEPRECATED
typedef struct NDIlib_recv_create_t {
	// The source that you wish to connect to.
	NDIlib_source_t source_to_connect_to;

	// Your preference of color space. See above.
	NDIlib_recv_color_format_e color_format;

	// The bandwidth setting that you wish to use for this video source. Bandwidth
	// controlled by changing both the compression level and the resolution of the source.
	// A good use for low bandwidth is working on WIFI connections.
	NDIlib_recv_bandwidth_e bandwidth;

	// When this flag is FALSE, all video that you receive will be progressive. For sources that provide
	// fields, this is de-interlaced on the receiving side (because we cannot change what the up-stream
	// source was actually rendering. This is provided as a convenience to down-stream sources that do not
	// wish to understand fielded video. There is almost no performance impact of using this function.
	bool allow_video_fields;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_create_t(
		const NDIlib_source_t source_to_connect_to_ = NDIlib_source_t(),
		NDIlib_recv_color_format_e color_format_ = NDIlib_recv_color_format_UYVY_BGRA,
		NDIlib_recv_bandwidth_e bandwidth_ = NDIlib_recv_bandwidth_highest,
		bool allow_video_fields_ = true
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_recv_create_t;

// This function is deprecated, please use NDIlib_recv_create_v3 if you can. Using this function will
// continue to work, and be supported for backwards compatibility. If the input parameter is NULL it will be
// created with default settings and an automatically determined receiver name.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_recv_instance_t NDIlib_recv_create_v2(const NDIlib_recv_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// For legacy reasons I called this the wrong thing. For backwards compatibility. If the input parameter is
// NULL it will be created with default settings and an automatically determined receiver name.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_recv_instance_t NDIlib_recv_create2(const NDIlib_recv_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// This function is deprecated, please use NDIlib_recv_create_v3 if you can. Using this function will
// continue to work, and be supported for backwards compatibility. This version sets bandwidth to highest and
// allow fields to true. If the input parameter is NULL it will be created with default settings and an
// automatically determined receiver name.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_recv_instance_t NDIlib_recv_create(const NDIlib_recv_create_t* p_create_settings);

// This will allow you to receive video, audio and metadata frames. Any of the buffers can be NULL, in which
// case data of that type will not be captured in this call. This call can be called simultaneously on
// separate threads, so it is entirely possible to receive audio, video, metadata all on separate threads.
// This function will return NDIlib_frame_type_none if no data is received within the specified timeout and
// NDIlib_frame_type_error if the connection is lost. Buffers captured with this must be freed with the
// appropriate free function below.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_frame_type_e NDIlib_recv_capture(
	NDIlib_recv_instance_t p_instance,   // The library instance.
	NDIlib_video_frame_t* p_video_data,  // The video data received (can be NULL).
	NDIlib_audio_frame_t* p_audio_data,  // The audio data received (can be NULL).
	NDIlib_metadata_frame_t* p_metadata, // The metadata received (can be NULL).
	uint32_t timeout_in_ms               // The amount of time in milliseconds to wait for data.
);

// Free the buffers returned by capture for video.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_recv_free_video(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);

// Free the buffers returned by capture for audio.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_recv_free_audio(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);

// This will add a video frame.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_send_send_video(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);

// This will add a video frame and will return immediately, having scheduled the frame to be displayed. All
// processing and sending of the video will occur asynchronously. The memory accessed by NDIlib_video_frame_t
// cannot be freed or re-used by the caller until a synchronizing event has occurred. In general the API is
// better able to take advantage of asynchronous processing than you might be able to by simple having a
// separate thread to submit frames.
//
// This call is particularly beneficial when processing BGRA video since it allows any color conversion,
// compression and network sending to all be done on separate threads from your main rendering thread.
//
// Synchronizing events are :
// - a call to NDIlib_send_send_video
// - a call to NDIlib_send_send_video_async with another frame to be sent
// - a call to NDIlib_send_send_video with p_video_data=NULL
// - a call to NDIlib_send_destroy
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_send_send_video_async(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);

// This will add an audio frame
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_send_send_audio(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);

// Convert an planar floating point audio buffer into a interleaved short audio buffer.
// IMPORTANT : You must allocate the space for the samples in the destination to allow for your own memory management.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_util_audio_to_interleaved_16s(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);

// Convert an interleaved short audio buffer audio buffer into a planar floating point one.
// IMPORTANT : You must allocate the space for the samples in the destination to allow for your own memory management.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_util_audio_from_interleaved_16s(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_t* p_dst);

// Convert an planar floating point audio buffer into a interleaved floating point audio buffer.
// IMPORTANT : You must allocate the space for the samples in the destination to allow for your own memory management.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_util_audio_to_interleaved_32f(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);

// Convert an interleaved floating point audio buffer into a planar floating point one.
// IMPORTANT : You must allocate the space for the samples in the destination to allow for your own memory management.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_util_audio_from_interleaved_32f(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_t* p_dst);
