#pragma once

// NOTE : The following MIT license applies to this file ONLY and not to the SDK as a whole. Please review the SDK documentation 
// for the description of the full license terms, which are also provided in the file "NDI License Agreement.pdf" within the SDK or 
// online at http://new.tk/ndisdk_license/. Your use of any part of this SDK is acknowledgment that you agree to the SDK license 
// terms. THe full NDI SDK may be downloaded at https://www.newtek.com/ndi/sdk/
//
//***********************************************************************************************************************************************
// 
// Copyright(c) 2014-2017 NewTek, inc
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation 
// files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, 
// merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//***********************************************************************************************************************************************

// This describes a video frame
PROCESSINGNDILIB_DEPRECATED
typedef struct NDIlib_video_frame_t
{	// The resolution of this frame
	int xres, yres;

	// What FourCC this is with. This can be two values
	NDIlib_FourCC_type_e FourCC;

	// What is the frame-rate of this frame.
	// For instance NTSC is 30000,1001 = 30000/1001 = 29.97fps
	int frame_rate_N, frame_rate_D;

	// What is the picture aspect ratio of this frame.
	// For instance 16.0/9.0 = 1.778 is 16:9 video
	float picture_aspect_ratio;

	// Is this a fielded frame, or is it progressive
	NDIlib_frame_format_type_e frame_format_type;

	// The timecode of this frame in 100ns intervals
	int64_t timecode;

	// The video data itself
	uint8_t* p_data;

	// The inter line stride of the video data, in bytes.
	int line_stride_in_bytes;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_video_frame_t(int xres_ = 0, int yres_ = 0, NDIlib_FourCC_type_e FourCC_ = NDIlib_FourCC_type_UYVY, int frame_rate_N_ = 30000, int frame_rate_D_ = 1001,
					     float picture_aspect_ratio_ = 0.0f, NDIlib_frame_format_type_e frame_format_type_ = NDIlib_frame_format_type_progressive, 
						 int64_t timecode_ = NDIlib_send_timecode_synthesize, uint8_t* p_data_ = NULL, int line_stride_in_bytes_ = 0);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

}	NDIlib_video_frame_t;

// This describes an audio frame
PROCESSINGNDILIB_DEPRECATED
typedef struct NDIlib_audio_frame_t
{	// The sample-rate of this buffer
	int sample_rate;

	// The number of audio channels
	int no_channels;

	// The number of audio samples per channel
	int no_samples;

	// The timecode of this frame in 100ns intervals
	int64_t timecode;

	// The audio data
	float* p_data;

	// The inter channel stride of the audio channels, in bytes
	int channel_stride_in_bytes;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_audio_frame_t(int sample_rate_ = 48000, int no_channels_ = 2, int no_samples_ = 0, int64_t timecode_ = NDIlib_send_timecode_synthesize,
						 float* p_data_ = NULL, int channel_stride_in_bytes_ = 0);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

}	NDIlib_audio_frame_t;