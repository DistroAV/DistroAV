#pragma once

// NOTE : The following MIT license applies to this file ONLY and not to the SDK as a whole. Please review
// the SDK documentation for the description of the full license terms, which are also provided in the file
// "NDI License Agreement.pdf" within the SDK or online at http://ndi.link/ndisdk_license. Your use of any
// part of this SDK is acknowledgment that you agree to the SDK license terms. The full NDI SDK may be
// downloaded at http://ndi.video/
//
//***********************************************************************************************************
//
// Copyright (C) 2023-2024 Vizrt NDI AB. All rights reserved.
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

// Because many applications like submitting 16-bit interleaved audio, these functions will convert in and
// out of that format. It is important to note that the NDI SDK does define fully audio levels, something
// that most applications that you use do not. Specifically, the floating-point range, -1.0 to +1.0, is
// defined as a professional audio reference level of +4 dBU. If we take 16-bit audio and scale it into this
// range it is almost always correct for sending and will cause no problems. For receiving however it is not
// at all uncommon that the user has audio that exceeds reference level and in this case it is likely that
// audio exceeds the reference level and so if you are not careful you will end up having audio clipping when
// you use the 16-bit range.

// This describes an audio frame.
typedef struct NDIlib_audio_frame_interleaved_16s_t {
	// The sample-rate of this buffer.
	int sample_rate;

	// The number of audio channels.
	int no_channels;

	// The number of audio samples per channel.
	int no_samples;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The audio reference level in dB. This specifies how many dB above the reference level (+4 dBU) is the
	// full range of 16-bit audio. If you do not understand this and want to just use numbers:
	// - If you are sending audio, specify +0 dB. Most common applications produce audio at reference level.
	// - If receiving audio, specify +20 dB. This means that the full 16-bit range corresponds to
	//   professional level audio with 20 dB of headroom. Note that if you are writing it into a file it
	//   might sound soft because you have 20 dB of headroom before clipping.
	int reference_level;

	// The audio data, interleaved 16-bit samples.
	int16_t* p_data;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_interleaved_16s_t(
		int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		int reference_level_ = 0,
		int16_t* p_data_ = NULL
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_audio_frame_interleaved_16s_t;

// This describes an audio frame.
typedef struct NDIlib_audio_frame_interleaved_32s_t {
	// The sample-rate of this buffer.
	int sample_rate;

	// The number of audio channels.
	int no_channels;

	// The number of audio samples per channel.
	int no_samples;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The audio reference level in dB. This specifies how many dB above the reference level (+4 dBU) is the
	// full range of 32-bit audio. If you do not understand this and want to just use numbers:
	// - If you are sending audio, specify +0 dB. Most common applications produce audio at reference level.
	// - If receiving audio, specify +20 dB. This means that the full 32-bit range corresponds to
	//   professional level audio with 20 dB of headroom. Note that if you are writing it into a file it
	//   might sound soft because you have 20 dB of headroom before clipping.
	int reference_level;

	// The audio data, interleaved 32-bit samples.
	int32_t* p_data;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_interleaved_32s_t(
		int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		int reference_level_ = 0,
		int32_t* p_data_ = NULL
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_audio_frame_interleaved_32s_t;

// This describes an audio frame.
typedef struct NDIlib_audio_frame_interleaved_32f_t {
	// The sample-rate of this buffer.
	int sample_rate;

	// The number of audio channels.
	int no_channels;

	// The number of audio samples per channel.
	int no_samples;

	// The timecode of this frame in 100-nanosecond intervals.
	int64_t timecode;

	// The audio data, interleaved 32-bit floating-point samples.
	float* p_data;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_interleaved_32f_t(
		int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0,
		int64_t timecode_ = NDIlib_send_timecode_synthesize,
		float* p_data_ = NULL
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_audio_frame_interleaved_32f_t;

// This will add an audio frame in interleaved 16-bit.
PROCESSINGNDILIB_API
void NDIlib_util_send_send_audio_interleaved_16s(
	NDIlib_send_instance_t p_instance,
	const NDIlib_audio_frame_interleaved_16s_t* p_audio_data
);

// This will add an audio frame in interleaved 32-bit.
PROCESSINGNDILIB_API
void NDIlib_util_send_send_audio_interleaved_32s(
	NDIlib_send_instance_t p_instance,
	const NDIlib_audio_frame_interleaved_32s_t* p_audio_data
);

// This will add an audio frame in interleaved floating point.
PROCESSINGNDILIB_API
void NDIlib_util_send_send_audio_interleaved_32f(
	NDIlib_send_instance_t p_instance,
	const NDIlib_audio_frame_interleaved_32f_t* p_audio_data
);

// Convert to interleaved 16-bit.
PROCESSINGNDILIB_API
void NDIlib_util_audio_to_interleaved_16s_v2(
	const NDIlib_audio_frame_v2_t* p_src,
	NDIlib_audio_frame_interleaved_16s_t* p_dst
);

// Convert from interleaved 16-bit.
PROCESSINGNDILIB_API
void NDIlib_util_audio_from_interleaved_16s_v2(
	const NDIlib_audio_frame_interleaved_16s_t* p_src,
	NDIlib_audio_frame_v2_t* p_dst
);

// Convert to interleaved 32-bit.
PROCESSINGNDILIB_API
void NDIlib_util_audio_to_interleaved_32s_v2(
	const NDIlib_audio_frame_v2_t* p_src,
	NDIlib_audio_frame_interleaved_32s_t* p_dst
);

// Convert from interleaved 32-bit.
PROCESSINGNDILIB_API
void NDIlib_util_audio_from_interleaved_32s_v2(
	const NDIlib_audio_frame_interleaved_32s_t* p_src,
	NDIlib_audio_frame_v2_t* p_dst
);

// Convert to interleaved floating point.
PROCESSINGNDILIB_API
void NDIlib_util_audio_to_interleaved_32f_v2(
	const NDIlib_audio_frame_v2_t* p_src,
	NDIlib_audio_frame_interleaved_32f_t* p_dst
);

// Convert from interleaved floating point.
PROCESSINGNDILIB_API
void NDIlib_util_audio_from_interleaved_32f_v2(
	const NDIlib_audio_frame_interleaved_32f_t* p_src,
	NDIlib_audio_frame_v2_t* p_dst
);

// This is a helper function that you may use to convert from 10-bit packed UYVY into 16-bit semi-planar. The
// FourCC on the source is ignored in this function since we do not define a V210 format in NDI. You must
// make sure that there is memory and a stride allocated in p_dst.
PROCESSINGNDILIB_API
void NDIlib_util_V210_to_P216(const NDIlib_video_frame_v2_t* p_src_v210, NDIlib_video_frame_v2_t* p_dst_p216);

// This converts from 16-bit semi-planar to 10-bit. You must make sure that there is memory and a stride
// allocated in p_dst.
PROCESSINGNDILIB_API
void NDIlib_util_P216_to_V210(const NDIlib_video_frame_v2_t* p_src_p216, NDIlib_video_frame_v2_t* p_dst_v210);
