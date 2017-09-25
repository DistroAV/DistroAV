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

//**************************************************************************************************************************
// Structures and type definitions required by NDI finding
// The reference to an instance of the receiver
typedef void* NDIlib_recv_instance_t;

typedef enum NDIlib_recv_bandwidth_e
{
	NDIlib_recv_bandwidth_metadata_only = -10, // Receive metadata.
	NDIlib_recv_bandwidth_audio_only	=  10, // Receive metadata, audio.
	NDIlib_recv_bandwidth_lowest		=  0,  // Receive metadata, audio, video at a lower bandwidth and resolution.
	NDIlib_recv_bandwidth_highest		=  100 // Receive metadata, audio, video at full resolution.

}	NDIlib_recv_bandwidth_e;

typedef enum NDIlib_recv_color_format_e
{	
	NDIlib_recv_color_format_BGRX_BGRA = 0,	// No alpha channel: BGRX, Alpha channel: BGRA	
	NDIlib_recv_color_format_UYVY_BGRA = 1,	// No alpha channel: UYVY, Alpha channel: BGRA
	NDIlib_recv_color_format_RGBX_RGBA = 2,	// No alpha channel: RGBX, Alpha channel: RGBA
	NDIlib_recv_color_format_UYVY_RGBA = 3,	// No alpha channel: UYVY, Alpha channel: RGBA

	// Read the SDK documentation to understand the pros and cons of this format. 
	NDIlib_recv_color_format_fastest = 100,

	// Legacy definitions for backwards compatability
	NDIlib_recv_color_format_e_BGRX_BGRA = 0,
	NDIlib_recv_color_format_e_UYVY_BGRA = 1,
	NDIlib_recv_color_format_e_RGBX_RGBA = 2,
	NDIlib_recv_color_format_e_UYVY_RGBA = 3
	
}	NDIlib_recv_color_format_e;

// The creation structure that is used when you are creating a receiver
typedef struct NDIlib_recv_create_t
{	// The source that you wish to connect to.
	NDIlib_source_t source_to_connect_to;

	// Your preference of color space. See above.
	NDIlib_recv_color_format_e color_format;

	// The bandwidth setting that you wish to use for this video source. Bandwidth
	// controlled by changing both the compression level and the resolution of the source.
	// A good use for low bandwidth is working on WIFI connections. 
	NDIlib_recv_bandwidth_e bandwidth;

	// When this flag is FALSE, all video that you receive will be progressive. For sources
	// that provide fields, this is de-interlaced on the receiving side (because we cannot change
	// what the up-stream source was actually rendering. This is provided as a convenience to
	// down-stream sources that do not wish to understand fielded video. There is almost no 
	// performance impact of using this function.
	bool allow_video_fields;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_create_t(const NDIlib_source_t source_to_connect_to_ = NDIlib_source_t(), NDIlib_recv_color_format_e color_format_=NDIlib_recv_color_format_UYVY_BGRA, 
						 NDIlib_recv_bandwidth_e bandwidth_ = NDIlib_recv_bandwidth_highest, bool allow_video_fields_ = true);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

}	NDIlib_recv_create_t;

// This allows you determine the current performance levels of the receiving to be able to detect whether frames have been dropped
typedef struct NDIlib_recv_performance_t
{	// The number of video frames
	int64_t video_frames;

	// The number of audio frames
	int64_t audio_frames;

	// The number of metadata frames
	int64_t metadata_frames;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_performance_t(void);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

}	NDIlib_recv_performance_t;

// Get the current queue depths
typedef struct NDIlib_recv_queue_t
{	// The number of video frames
	int video_frames;

	// The number of audio frames
	int audio_frames;

	// The number of metadata frames
	int metadata_frames;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_queue_t(void);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

}	NDIlib_recv_queue_t;

//**************************************************************************************************************************
// Create a new receiver instance. This will return NULL if it fails.
PROCESSINGNDILIB_API
NDIlib_recv_instance_t NDIlib_recv_create_v2(const NDIlib_recv_create_t* p_create_settings);

// For legacy reasons I called this the wrong thing. For backwards compatability.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_recv_instance_t NDIlib_recv_create2(const NDIlib_recv_create_t* p_create_settings);

// This function is deprecated, please use NDIlib_recv_create_v2 if you can. Using this function will continue to work, and be
// supported for backwards compatibility. This version sets bandwidth to highest and allow fields to true.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_recv_instance_t NDIlib_recv_create(const NDIlib_recv_create_t* p_create_settings);

// This will destroy an existing receiver instance.
PROCESSINGNDILIB_API
void NDIlib_recv_destroy(NDIlib_recv_instance_t p_instance);

// This will allow you to receive video, audio and metadata frames.
// Any of the buffers can be NULL, in which case data of that type
// will not be captured in this call. This call can be called simultaneously
// on separate threads, so it is entirely possible to receive audio, video, metadata
// all on separate threads. This function will return NDIlib_frame_type_none if no
// data is received within the specified timeout and NDIlib_frame_type_error if the connection is lost.
// Buffers captured with this must be freed with the appropriate free function below.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
NDIlib_frame_type_e NDIlib_recv_capture(
	NDIlib_recv_instance_t p_instance,   // The library instance
	NDIlib_video_frame_t* p_video_data,  // The video data received (can be NULL)
	NDIlib_audio_frame_t* p_audio_data,  // The audio data received (can be NULL)
	NDIlib_metadata_frame_t* p_metadata, // The metadata received (can be NULL)
	uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.

PROCESSINGNDILIB_API
NDIlib_frame_type_e NDIlib_recv_capture_v2(
	NDIlib_recv_instance_t p_instance,   // The library instance
	NDIlib_video_frame_v2_t* p_video_data,  // The video data received (can be NULL)
	NDIlib_audio_frame_v2_t* p_audio_data,  // The audio data received (can be NULL)
	NDIlib_metadata_frame_t* p_metadata, // The metadata received (can be NULL)
	uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.

// Free the buffers returned by capture for video
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_recv_free_video(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);

PROCESSINGNDILIB_API
void NDIlib_recv_free_video_v2(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

// Free the buffers returned by capture for audio
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
void NDIlib_recv_free_audio(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);

PROCESSINGNDILIB_API 
void NDIlib_recv_free_audio_v2(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);

// Free the buffers returned by capture for metadata
PROCESSINGNDILIB_API
void NDIlib_recv_free_metadata(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// This will free a string that was allocated and returned by NDIlib_recv (for instance the NDIlib_recv_get_web_control) function.
PROCESSINGNDILIB_API
void NDIlib_recv_free_string(NDIlib_recv_instance_t p_instance, const char* p_string);

// This function will send a meta message to the source that we are connected too. This returns FALSE if we are
// not currently connected to anything.
PROCESSINGNDILIB_API
bool NDIlib_recv_send_metadata(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// Set the up-stream tally notifications. This returns FALSE if we are not currently connected to anything. That
// said, the moment that we do connect to something it will automatically be sent the tally state.
PROCESSINGNDILIB_API
bool NDIlib_recv_set_tally(NDIlib_recv_instance_t p_instance, const NDIlib_tally_t* p_tally);

// Get the current performance structures. This can be used to determine if you have been calling NDIlib_recv_capture fast
// enough, or if your processing of data is not keeping up with real-time. The total structure will give you the total frame
// counts received, the dropped structure will tell you how many frames have been dropped. Either of these could be NULL.
PROCESSINGNDILIB_API
void NDIlib_recv_get_performance(NDIlib_recv_instance_t p_instance, NDIlib_recv_performance_t* p_total, NDIlib_recv_performance_t* p_dropped);

// This will allow you to determine the current queue depth for all of the frame sources at any time. 
PROCESSINGNDILIB_API
void NDIlib_recv_get_queue(NDIlib_recv_instance_t p_instance, NDIlib_recv_queue_t* p_total);

// Connection based metadata is data that is sent automatically each time a new connection is received. You queue all of these
// up and they are sent on each connection. To reset them you need to clear them all and set them up again. 
PROCESSINGNDILIB_API
void NDIlib_recv_clear_connection_metadata(NDIlib_recv_instance_t p_instance);

// Add a connection metadata string to the list of what is sent on each new connection. If someone is already connected then
// this string will be sent to them immediately.
PROCESSINGNDILIB_API
void NDIlib_recv_add_connection_metadata(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// Is this receiver currently connected to a source on the other end, or has the source not yet been found or is no longe ronline.
// This will normally return 0 or 1
PROCESSINGNDILIB_API
int NDIlib_recv_get_no_connections(NDIlib_recv_instance_t p_instance);

// Has this receiver got PTZ control. Note that it might take a second or two after the connection for this value to be set.
// To avoid the need to poll this function, you can know when the value of this function might have changed when the 
// NDILib_recv_capture* call would return NDIlib_frame_type_status_change
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_is_supported(NDIlib_recv_instance_t p_instance);

// Has this receiver got recording control. Note that it might take a second or two after the connection for this value to be set.
// To avoid the need to poll this function, you can know when the value of this function might have changed when the 
// NDILib_recv_capture* call would return NDIlib_frame_type_status_change
PROCESSINGNDILIB_API
bool NDIlib_recv_recording_is_supported(NDIlib_recv_instance_t p_instance);

// Get the URL that might be used for configuration of this input. Note that it might take a second or two after the connection for 
// this value to be set. This function will return NULL if there is no web control user interface. You should call NDIlib_recv_free_string
// to free the string that is returned by this function. The returned value will be a fully formed URL, for instamce "http://10.28.1.192/configuration/"
// To avoid the need to poll this function, you can know when the value of this function might have changed when the 
// NDILib_recv_capture* call would return NDIlib_frame_type_status_change
PROCESSINGNDILIB_API
const char* NDIlib_recv_get_web_control(NDIlib_recv_instance_t p_instance);

// PTZ Controls
	// Zoom to an absolute value. 
	// zoom_value = 0.0 (zoomed in) ... 1.0 (zoomed out)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_zoom(NDIlib_recv_instance_t p_instance, const float zoom_value);

	// Zoom at a particular speed
	// zoom_speed = -1.0 (zoom outwards) ... +1.0 (zoom inwards)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_zoom_speed(NDIlib_recv_instance_t p_instance, const float zoom_speed);

	// Set the pan and tilt to an absolute value
	// pan_value  = -1.0 (left) ... 0.0 (centred) ... +1.0 (right)
	// tilt_value = -1.0 (bottom) ... 0.0 (centred) ... +1.0 (top)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_pan_tilt(NDIlib_recv_instance_t p_instance, const float pan_value, const float tilt_value);

	// Set the pan and tilt direction and speed
	// pan_speed = -1.0 (moving left) ... 0.0 (stopped) ... +1.0 (moving right)
	// tilt_speed = -1.0 (down) ... 0.0 (stopped) ... +1.0 (moving up)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_pan_tilt_speed(NDIlib_recv_instance_t p_instance, const float pan_speed, const float tilt_speed);

	// Store the current position, focus, etc... as a preset.
	// preset_no = 0 ... 99
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_store_preset(NDIlib_recv_instance_t p_instance, const int preset_no);

	// Recall a preset, including position, focus, etc...
	// preset_no = 0 ... 99
	// speed = 0.0(as slow as possible) ... 1.0(as fast as possible) The speed at which to move to the new preset
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_recall_preset(NDIlib_recv_instance_t p_instance, const int preset_no, const float speed);

	// Put the camera in auto-focus
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_auto_focus(NDIlib_recv_instance_t p_instance);

	// Focus to an absolute value. 
	// focus_value = 0.0 (focussed to infinity) ... 1.0 (focussed as close as possible)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_focus(NDIlib_recv_instance_t p_instance, const float focus_value);

	// Focus at a particular speed
	// focus_speed = -1.0 (focus outwards) ... +1.0 (focus inwards)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_focus_speed(NDIlib_recv_instance_t p_instance, const float focus_speed);

	// Put the camera in auto white balance moce
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_white_balance_auto(NDIlib_recv_instance_t p_instance);

	// Put the camera in indoor white balance
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_white_balance_indoor(NDIlib_recv_instance_t p_instance);

	// Put the camera in indoor white balance
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_white_balance_outdoor(NDIlib_recv_instance_t p_instance);

	// Use the current brightness to automatically set the current white balance
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_white_balance_oneshot(NDIlib_recv_instance_t p_instance);

	// Set the manual camera white balance using the R, B values
	// red = 0.0(not red) ... 1.0(very red)
	// blue = 0.0(not blue) ... 1.0(very blue)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_white_balance_manual(NDIlib_recv_instance_t p_instance, const float red, const float blue);

	// Put the camera in auto-exposure mode
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_exposure_auto(NDIlib_recv_instance_t p_instance);

	// Manually set the camera exposure
	// exposure_level = 0.0(dark) ... 1.0(light)
	PROCESSINGNDILIB_API
	bool NDIlib_recv_ptz_exposure_manual(NDIlib_recv_instance_t p_instance, const float exposure_level);

// Recording control
	// This will start recording.If the recorder was already recording then the message is ignored.A filename is passed in as a ‘hint’.Since the recorder might 
	// already be recording(or might not allow complete flexibility over its filename), the filename might or might not be used.If the filename is empty, or 
	// not present, a name will be chosen automatically. If you do not with to provide a filename hint you can simply pass NULL. 
	PROCESSINGNDILIB_API
	bool NDIlib_recv_recording_start(NDIlib_recv_instance_t p_instance, const char* p_filename_hint);

	// Stop recording.
	PROCESSINGNDILIB_API
	bool NDIlib_recv_recording_stop(NDIlib_recv_instance_t p_instance);

	// This will control the audio level for the recording.dB is specified in decibels relative to the reference level of the source. Not all recording sources support 
	// controlling audio levels.For instance, a digital audio device would not be able to avoid clipping on sources already at the wrong level, thus 
	// might not support this message.
	PROCESSINGNDILIB_API
	bool NDIlib_recv_recording_set_audio_level(NDIlib_recv_instance_t p_instance, const float level_dB);

	// This will determine if the source is currently recording. It will return true while recording is in progress and false when it is not. Because there is
	// one recorded and multiple people might be connected to it, there is a chance that it is recording which was initiated by someone else.
	PROCESSINGNDILIB_API
	bool NDIlib_recv_recording_is_recording(NDIlib_recv_instance_t p_instance);

	// Get the current filename for recording. When this is set it will return a non-NULL value which is owned by you and freed using NDIlib_recv_free_string. 
	// If a file was already being recorded by another client, the massage will contain the name of that file. The filename contains a UNC path (when one is available) 
	// to the recorded file, and can be used to access the file on your local machine for playback.  If a UNC path is not available, then this will represent the local 
	// filename. This will remain valid even after the file has stopped being recorded until the next file is started.
	PROCESSINGNDILIB_API
	const char* NDIlib_recv_recording_get_filename(NDIlib_recv_instance_t p_instance);

	// This will tell you whether there was a recording error and what that string is. When this is set it will return a non-NULL value which is owned by you and 
	// freed using NDIlib_recv_free_string. When there is no error it will return NULL.
	PROCESSINGNDILIB_API
	const char* NDIlib_recv_recording_get_error(NDIlib_recv_instance_t p_instance);

	// In order to get the duration 
	typedef struct NDIlib_recv_recording_time_t
	{	// The number of actual video frames recorded. 
		int64_t no_frames;
		
		// The starting time and current largest time of the record, in UTC time, at 100ns unit intervals. This allows you to know the record
		// time irrespective of frame-rate. For instance, last_time - start_time woudl give you the recording length in 100ns intervals.
		int64_t start_time, last_time;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
		NDIlib_recv_recording_time_t(void);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

	}	NDIlib_recv_recording_time_t;

	// Get the current recording times. These remain 
	PROCESSINGNDILIB_API
	bool NDIlib_recv_recording_get_times(NDIlib_recv_instance_t p_instance, NDIlib_recv_recording_time_t* p_times);