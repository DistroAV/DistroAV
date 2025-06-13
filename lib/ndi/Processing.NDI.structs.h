#pragma once

// NOTE : The following MIT license applies to this file ONLY and not to the SDK as a whole. Please review
// the SDK documentation for the description of the full license terms, which are also provided in the file
// "NDI License Agreement.pdf" within the SDK or online at http://ndi.link/ndisdk_license. Your use of any
// part of this SDK is acknowledgment that you agree to the SDK license terms. The full NDI SDK may be
// downloaded at http://ndi.video/
//
//***********************************************************************************************************
//
// Copyright (C) 2023-2025 Vizrt NDI AB. All rights reserved.
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

#ifndef NDI_LIB_FOURCC
#define NDI_LIB_FOURCC(ch0, ch1, ch2, ch3) \
	((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#endif

// An enumeration to specify the type of a packet returned by the functions.
typedef enum NDIlib_frame_type_e {
	// What frame type is this?
	NDIlib_frame_type_none = 0,
	NDIlib_frame_type_video = 1,
	NDIlib_frame_type_audio = 2,
	NDIlib_frame_type_metadata = 3,
	NDIlib_frame_type_error = 4,

	// This indicates that the settings on this input have changed. This value will be returned from one of
	// the NDIlib_recv_capture functions when the device is known to have new settings, for instance the web
	// URL has changed or the device is now known to be a PTZ camera.
	NDIlib_frame_type_status_change = 100,

	// This indicates that the source has changed. This value will be returned from one of the
	// NDIlib_recv_capture functions when the source that the receiver is connected to has changed.
	NDIlib_frame_type_source_change = 101,

	// Make sure this is a 32-bit enumeration.
	NDIlib_frame_type_max = 0x7fffffff
} NDIlib_frame_type_e;

// FourCC values for video frames.
typedef enum NDIlib_FourCC_video_type_e {
	// YCbCr color space using 4:2:2.
	NDIlib_FourCC_video_type_UYVY = NDI_LIB_FOURCC('U', 'Y', 'V', 'Y'),
	NDIlib_FourCC_type_UYVY = NDIlib_FourCC_video_type_UYVY,

	// YCbCr + Alpha color space, using 4:2:2:4.
	// In memory there are two separate planes. The first is a regular
	// UYVY 4:2:2 buffer. Immediately following this in memory is a
	// alpha channel buffer.
	NDIlib_FourCC_video_type_UYVA = NDI_LIB_FOURCC('U', 'Y', 'V', 'A'),
	NDIlib_FourCC_type_UYVA = NDIlib_FourCC_video_type_UYVA,

	// YCbCr color space using 4:2:2 in 16bpp.
	// In memory this is a semi-planar format. This is identical to a 16bpp version of the NV16 format.
	// The first buffer is a 16bpp luminance buffer.
	// Immediately after this is an interleaved buffer of 16bpp Cb, Cr pairs.
	NDIlib_FourCC_video_type_P216 = NDI_LIB_FOURCC('P', '2', '1', '6'),
	NDIlib_FourCC_type_P216 = NDIlib_FourCC_video_type_P216,

	// YCbCr color space with an alpha channel, using 4:2:2:4.
	// In memory this is a semi-planar format.
	// The first buffer is a 16bpp luminance buffer.
	// Immediately after this is an interleaved buffer of 16bpp Cb, Cr pairs.
	// Immediately after is a single buffer of 16bpp alpha channel.
	NDIlib_FourCC_video_type_PA16 = NDI_LIB_FOURCC('P', 'A', '1', '6'),
	NDIlib_FourCC_type_PA16 = NDIlib_FourCC_video_type_PA16,

	// Planar 8bit 4:2:0 video format.
	// The first buffer is an 8bpp luminance buffer.
	// Immediately following this is a 8bpp Cr buffer.
	// Immediately following this is a 8bpp Cb buffer.
	NDIlib_FourCC_video_type_YV12 = NDI_LIB_FOURCC('Y', 'V', '1', '2'),
	NDIlib_FourCC_type_YV12 = NDIlib_FourCC_video_type_YV12,

	// The first buffer is an 8bpp luminance buffer.
	// Immediately following this is a 8bpp Cb buffer.
	// Immediately following this is a 8bpp Cr buffer.
	NDIlib_FourCC_video_type_I420 = NDI_LIB_FOURCC('I', '4', '2', '0'),
	NDIlib_FourCC_type_I420 = NDIlib_FourCC_video_type_I420,

	// Planar 8bit 4:2:0 video format.
	// The first buffer is an 8bpp luminance buffer.
	// Immediately following this is in interleaved buffer of 8bpp Cb, Cr pairs
	NDIlib_FourCC_video_type_NV12 = NDI_LIB_FOURCC('N', 'V', '1', '2'),
	NDIlib_FourCC_type_NV12 = NDIlib_FourCC_video_type_NV12,

	// Planar 8bit, 4:4:4:4 video format.
	// Color ordering in memory is blue, green, red, alpha
	NDIlib_FourCC_video_type_BGRA = NDI_LIB_FOURCC('B', 'G', 'R', 'A'),
	NDIlib_FourCC_type_BGRA = NDIlib_FourCC_video_type_BGRA,

	// Planar 8bit, 4:4:4 video format, packed into 32bit pixels.
	// Color ordering in memory is blue, green, red, 255
	NDIlib_FourCC_video_type_BGRX = NDI_LIB_FOURCC('B', 'G', 'R', 'X'),
	NDIlib_FourCC_type_BGRX = NDIlib_FourCC_video_type_BGRX,

	// Planar 8bit, 4:4:4:4 video format.
	// Color ordering in memory is red, green, blue, alpha
	NDIlib_FourCC_video_type_RGBA = NDI_LIB_FOURCC('R', 'G', 'B', 'A'),
	NDIlib_FourCC_type_RGBA = NDIlib_FourCC_video_type_RGBA,

	// Planar 8bit, 4:4:4 video format, packed into 32bit pixels.
	// Color ordering in memory is red, green, blue, 255.
	NDIlib_FourCC_video_type_RGBX = NDI_LIB_FOURCC('R', 'G', 'B', 'X'),
	NDIlib_FourCC_type_RGBX = NDIlib_FourCC_video_type_RGBX,

	// Make sure this is a 32-bit enumeration.
	NDIlib_FourCC_video_type_max = 0x7fffffff
} NDIlib_FourCC_video_type_e;

// Really for backwards compatibility.
PROCESSINGNDILIB_DEPRECATED
typedef NDIlib_FourCC_video_type_e NDIlib_FourCC_type_e;

// FourCC values for audio frames.
typedef enum NDIlib_FourCC_audio_type_e {
	// Planar 32-bit floating point. Be sure to specify the channel stride.
	NDIlib_FourCC_audio_type_FLTP = NDI_LIB_FOURCC('F', 'L', 'T', 'p'),
	NDIlib_FourCC_type_FLTP = NDIlib_FourCC_audio_type_FLTP,

	// Make sure this is a 32-bit enumeration.
	NDIlib_FourCC_audio_type_max = 0x7fffffff
} NDIlib_FourCC_audio_type_e;

typedef enum NDIlib_frame_format_type_e {
	// A progressive frame.
	NDIlib_frame_format_type_progressive = 1,

	// A fielded frame with the field 0 being on the even lines and field 1 being
	// on the odd lines.
	NDIlib_frame_format_type_interleaved = 0,

	// Individual fields.
	NDIlib_frame_format_type_field_0 = 2,
	NDIlib_frame_format_type_field_1 = 3,

	// Make sure this is a 32-bit enumeration.
	NDIlib_frame_format_type_max = 0x7fffffff
} NDIlib_frame_format_type_e;

// When you specify this as a timecode, the timecode will be synthesized for you. This may be used when
// sending video, audio or metadata. If you never specify a timecode at all, asking for each to be
// synthesized, then this will use the current system time as the starting timecode and then generate
// synthetic ones, keeping your streams exactly in sync as long as the frames you are sending do not deviate
// from the system time in any meaningful way. In practice this means that if you never specify timecodes
// that they will always be generated for you correctly. Timecodes coming from different senders on the same
// machine will always be in sync with each other when working in this way. If you have NTP installed on your
// local network, then streams can be synchronized between multiple machines with very high precision.
//
// If you specify a timecode at a particular frame (audio or video), then ask for all subsequent ones to be
// synthesized. The subsequent ones will be generated to continue this sequence maintaining the correct
// relationship both the between streams and samples generated, avoiding them deviating in time from the
// timecode that you specified in any meaningful way.
//
// If you specify timecodes on one stream (e.g. video) and ask for the other stream (audio) to be
// synthesized, the correct timecodes will be generated for the other stream and will be synthesize exactly
// to match (they are not quantized inter-streams) the correct sample positions.
//
// When you send metadata messages and ask for the timecode to be synthesized, then it is chosen to match the
// closest audio or video frame timecode so that it looks close to something you might want ... unless there
// is no sample that looks close in which a timecode is synthesized from the last ones known and the time
// since it was sent.
static const int64_t NDIlib_send_timecode_synthesize = INT64_MAX;

// If the time-stamp is not available (i.e. a version of a sender before v2.5).
static const int64_t NDIlib_recv_timestamp_undefined = INT64_MAX;

// This is a descriptor of a NDI source available on the network.
typedef struct NDIlib_source_t {
	// A UTF8 string that provides a user readable name for this source. This can be used for serialization,
	// etc... and comprises the machine name and the source name on that machine. In the form,
	//     MACHINE_NAME (NDI_SOURCE_NAME)
	// If you specify this parameter either as NULL, or an EMPTY string then the specific IP address and port
	// number from below is used.
	const char* p_ndi_name;

	// A UTF8 string that provides the actual network address and any parameters. This is not meant to be
	// application readable and might well change in the future. This can be NULL if you do not know it and
	// the API internally will instantiate a finder that is used to discover it even if it is not yet
	// available on the network.
	union {	// The current way of addressing the value.
		const char* p_url_address;

		// We used to use an IP address before we used the more general URL notification this is now
		// depreciated but maintained for compatibility.
		PROCESSINGNDILIB_DEPRECATED const char* p_ip_address;
	};

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_source_t(const char* p_ndi_name_ = NULL, const char* p_url_address_ = NULL);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_source_t;

// This describes a video frame.
typedef struct NDIlib_video_frame_v2_t {
	// The resolution of this frame.
	int xres, yres;

	// What FourCC describing the type of data for this frame.
	NDIlib_FourCC_video_type_e FourCC;

	// What is the frame rate of this frame.
	// For instance NTSC is 30000,1001 = 30000/1001 = 29.97 fps.
	int frame_rate_N, frame_rate_D;

	// What is the picture aspect ratio of this frame.
	// For instance 16.0/9.0 = 1.778 is 16:9 video
	// 0 means square pixels.
	float picture_aspect_ratio;

	// Is this a fielded frame, or is it progressive.
	NDIlib_frame_format_type_e frame_format_type;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The video data itself.
	uint8_t* p_data;

	union {	// If the FourCC is not a compressed type, then this will be the inter-line stride of the video data
		// in bytes.  If the stride is 0, then it will default to sizeof(one pixel)*xres.
		int line_stride_in_bytes;

		// If the FourCC is a compressed type, then this will be the size of the p_data buffer in bytes.
		int data_size_in_bytes;
	};

	// Per frame metadata for this frame. This is a NULL terminated UTF8 string that should be in XML format.
	// If you do not want any metadata then you may specify NULL here.
	const char* p_metadata; // Present in >= v2.5

	// This is only valid when receiving a frame and is specified as a 100-nanosecond time that was the exact
	// moment that the frame was submitted by the sending side and is generated by the SDK. If this value is
	// NDIlib_recv_timestamp_undefined then this value is not available and is NDIlib_recv_timestamp_undefined.
	int64_t timestamp; // Present in >= v2.5

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_video_frame_v2_t(
		int xres_ = 0, int yres_ = 0,
		NDIlib_FourCC_video_type_e FourCC_ = NDIlib_FourCC_video_type_UYVY,
		int frame_rate_N_ = 30000, int frame_rate_D_ = 1001,
		float picture_aspect_ratio_ = 0.0f,
		NDIlib_frame_format_type_e frame_format_type_ = NDIlib_frame_format_type_progressive,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		uint8_t* p_data_ = NULL, int line_stride_in_bytes_ = 0,
		const char* p_metadata_ = NULL,
		int64_t timestamp_ = 0
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_video_frame_v2_t;

// This describes an audio frame.
typedef struct NDIlib_audio_frame_v2_t {
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

	// Per frame metadata for this frame. This is a NULL terminated UTF8 string that should be in XML format.
	// If you do not want any metadata then you may specify NULL here.
	const char* p_metadata; // Present in >= v2.5

	// This is only valid when receiving a frame and is specified as a 100-nanosecond time that was the exact
	// moment that the frame was submitted by the sending side and is generated by the SDK. If this value is
	// NDIlib_recv_timestamp_undefined then this value is not available and is NDIlib_recv_timestamp_undefined.
	int64_t timestamp; // Present in >= v2.5

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_v2_t(
		int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		float* p_data_ = NULL, int channel_stride_in_bytes_ = 0,
		const char* p_metadata_ = NULL,
		int64_t timestamp_ = 0
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_audio_frame_v2_t;

// This describes an audio frame.
typedef struct NDIlib_audio_frame_v3_t {
	// The sample-rate of this buffer.
	int sample_rate;

	// The number of audio channels.
	int no_channels;

	// The number of audio samples per channel.
	int no_samples;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// What FourCC describing the type of data for this frame.
	NDIlib_FourCC_audio_type_e FourCC;

	// The audio data.
	uint8_t* p_data;

	union {
		// If the FourCC is not a compressed type and the audio format is planar, then this will be the
		// stride in bytes for a single channel.
		int channel_stride_in_bytes;

		// If the FourCC is a compressed type, then this will be the size of the p_data buffer in bytes.
		int data_size_in_bytes;
	};

	// Per frame metadata for this frame. This is a NULL terminated UTF8 string that should be in XML format.
	// If you do not want any metadata then you may specify NULL here.
	const char* p_metadata;

	// This is only valid when receiving a frame and is specified as a 100-nanosecond time that was the exact
	// moment that the frame was submitted by the sending side and is generated by the SDK. If this value is
	// NDIlib_recv_timestamp_undefined then this value is not available and is NDIlib_recv_timestamp_undefined.
	int64_t timestamp;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_v3_t(
		int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		NDIlib_FourCC_audio_type_e FourCC_ = NDIlib_FourCC_audio_type_FLTP,
		uint8_t* p_data_ = NULL, int channel_stride_in_bytes_ = 0,
		const char* p_metadata_ = NULL,
		int64_t timestamp_ = 0
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_audio_frame_v3_t;

// The data description for metadata.
typedef struct NDIlib_metadata_frame_t {
	// The length of the string in UTF8 characters. This includes the NULL terminating character. If this is
	// 0, then the length is assume to be the length of a NULL terminated string.
	int length;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The metadata as a UTF8 XML string. This is a NULL terminated string.
	char* p_data;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_metadata_frame_t(int length_ = 0, int64_t timecode_ = NDIlib_send_timecode_synthesize, char* p_data_ = NULL);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_metadata_frame_t;

// Tally structures
typedef struct NDIlib_tally_t {
	// Is this currently on program output.
	bool on_program;

	// Is this currently on preview output.
	bool on_preview;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_tally_t(bool on_program_ = false, bool on_preview_ = false);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_tally_t;
