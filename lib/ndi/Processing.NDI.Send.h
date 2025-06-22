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

// Structures and type definitions required by NDI sending.
// The reference to an instance of the sender.
struct NDIlib_send_instance_type;
typedef struct NDIlib_send_instance_type* NDIlib_send_instance_t;

// The creation structure that is used when you are creating a sender.
typedef struct NDIlib_send_create_t {
	// The name of the NDI source to create. This is a NULL terminated UTF8 string.
	const char* p_ndi_name;

	// What groups should this source be part of. NULL means default.
	const char* p_groups;

	// Do you want audio and video to "clock" themselves. When they are clocked then by adding video frames,
	// they will be rate limited to match the current frame rate that you are submitting at. The same is true
	// for audio. In general if you are submitting video and audio off a single thread then you should only
	// clock one of them (video is probably the better of the two to clock off). If you are submitting audio
	// and video of separate threads then having both clocked can be useful.
	bool clock_video, clock_audio;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_send_create_t(
		const char* p_ndi_name_ = NULL,
		const char* p_groups_ = NULL,
		bool clock_video_ = true, bool clock_audio_ = true
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_send_create_t;

// Create a new sender instance. This will return NULL if it fails. If you specify leave p_create_settings
// null then the sender will be created with default settings.
PROCESSINGNDILIB_API
NDIlib_send_instance_t NDIlib_send_create(const NDIlib_send_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// This will destroy an existing finder instance.
PROCESSINGNDILIB_API
void NDIlib_send_destroy(NDIlib_send_instance_t p_instance);

// This will add a video frame.
PROCESSINGNDILIB_API
void NDIlib_send_send_video_v2(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

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
PROCESSINGNDILIB_API
void NDIlib_send_send_video_async_v2(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

// This will add an audio frame.
PROCESSINGNDILIB_API
void NDIlib_send_send_audio_v2(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);

// This will add an audio frame.
PROCESSINGNDILIB_API
void NDIlib_send_send_audio_v3(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);

// This will add a metadata frame.
PROCESSINGNDILIB_API
void NDIlib_send_send_metadata(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// This allows you to receive metadata from the other end of the connection.
PROCESSINGNDILIB_API
NDIlib_frame_type_e NDIlib_send_capture(
	NDIlib_send_instance_t p_instance,   // The instance data.
	NDIlib_metadata_frame_t* p_metadata, // The metadata received (can be NULL).
	uint32_t timeout_in_ms               // The amount of time in milliseconds to wait for data.
);

// Free the buffers returned by capture for metadata.
PROCESSINGNDILIB_API
void NDIlib_send_free_metadata(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// Determine the current tally sate. If you specify a timeout then it will wait until it has changed,
// otherwise it will simply poll it and return the current tally immediately. The return value is whether
// anything has actually change (true) or whether it timed out (false)
PROCESSINGNDILIB_API
bool NDIlib_send_get_tally(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms);

// Get the current number of receivers connected to this source. This can be used to avoid even rendering
// when nothing is connected to the video source. which can significantly improve the efficiency if you want
// to make a lot of sources available on the network. If you specify a timeout that is not 0 then it will
// wait until there are connections for this amount of time.
PROCESSINGNDILIB_API
int NDIlib_send_get_no_connections(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms);

// Connection based metadata is data that is sent automatically each time a new connection is received. You
// queue all of these up and they are sent on each connection. To reset them you need to clear them all and
// set them up again.
PROCESSINGNDILIB_API
void NDIlib_send_clear_connection_metadata(NDIlib_send_instance_t p_instance);

// Add a connection metadata string to the list of what is sent on each new connection. If someone is already
// connected then this string will be sent to them immediately.
PROCESSINGNDILIB_API
void NDIlib_send_add_connection_metadata(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// This will assign a new fail-over source for this video source. What this means is that if this video
// source was to fail any receivers would automatically switch over to use this source, unless this source
// then came back online. You can specify NULL to clear the source.
PROCESSINGNDILIB_API
void NDIlib_send_set_failover(NDIlib_send_instance_t p_instance, const NDIlib_source_t* p_failover_source);

// Retrieve the source information for the given sender instance.  This pointer is valid until NDIlib_send_destroy is called.
PROCESSINGNDILIB_API
const NDIlib_source_t* NDIlib_send_get_source_name(NDIlib_send_instance_t p_instance);
