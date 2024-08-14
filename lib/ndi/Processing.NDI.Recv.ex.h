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

// Has this receiver got PTZ control. Note that it might take a second or two after the connection for this
// value to be set. To avoid the need to poll this function, you can know when the value of this function
// might have changed when the NDILib_recv_capture* call would return NDIlib_frame_type_status_change.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_is_supported(NDIlib_recv_instance_t p_instance);

// Has this receiver got recording control. Note that it might take a second or two after the connection for
// this value to be set. To avoid the need to poll this function, you can know when the value of this
// function might have changed when the NDILib_recv_capture* call would return NDIlib_frame_type_status_change.
//
// Note on deprecation of this function:
//   NDI version 4 includes the native ability to record all NDI streams using an external application that
//   is provided with the SDK. This is better in many ways than the internal recording support which only
//   ever supported remotely recording systems and NDI|HX. This functionality will be supported in the SDK
//   for some time although we are recommending that you use the newer support which is more feature rich and
//   supports the recording of all stream types, does not take CPU time to record NDI sources (it does not
//   require any type of re-compression since it can just store the data in the file), it will synchronize
//   all recorders on a system (and cross systems if NTP clock locking is used).
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
bool NDIlib_recv_recording_is_supported(NDIlib_recv_instance_t p_instance);

// PTZ Controls.
// Zoom to an absolute value.
// zoom_value = 0.0 (zoomed in) ... 1.0 (zoomed out)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_zoom(NDIlib_recv_instance_t p_instance, const float zoom_value);

// Zoom at a particular speed.
// zoom_speed = -1.0 (zoom outwards) ... +1.0 (zoom inwards)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_zoom_speed(NDIlib_recv_instance_t p_instance, const float zoom_speed);

// Set the pan and tilt to an absolute value.
// pan_value  = -1.0 (left) ... 0.0 (centered) ... +1.0 (right)
// tilt_value = -1.0 (bottom) ... 0.0 (centered) ... +1.0 (top)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_pan_tilt(NDIlib_recv_instance_t p_instance, const float pan_value, const float tilt_value);

// Set the pan and tilt direction and speed.
// pan_speed = -1.0 (moving right) ... 0.0 (stopped) ... +1.0 (moving left)
// tilt_speed = -1.0 (down) ... 0.0 (stopped) ... +1.0 (moving up)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_pan_tilt_speed(NDIlib_recv_instance_t p_instance, const float pan_speed, const float tilt_speed);

// Store the current position, focus, etc... as a preset.
// preset_no = 0 ... 99
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_store_preset(NDIlib_recv_instance_t p_instance, const int preset_no);

// Recall a preset, including position, focus, etc...
// preset_no = 0 ... 99
// speed = 0.0(as slow as possible) ... 1.0(as fast as possible) The speed at which to move to the new preset.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_recall_preset(NDIlib_recv_instance_t p_instance, const int preset_no, const float speed);

// Put the camera in auto-focus.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_auto_focus(NDIlib_recv_instance_t p_instance);

// Focus to an absolute value.
// focus_value = 0.0 (focused to infinity) ... 1.0 (focused as close as possible)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_focus(NDIlib_recv_instance_t p_instance, const float focus_value);

// Focus at a particular speed.
// focus_speed = -1.0 (focus outwards) ... +1.0 (focus inwards)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_focus_speed(NDIlib_recv_instance_t p_instance, const float focus_speed);

// Put the camera in auto white balance mode.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_white_balance_auto(NDIlib_recv_instance_t p_instance);

// Put the camera in indoor white balance.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_white_balance_indoor(NDIlib_recv_instance_t p_instance);

// Put the camera in indoor white balance.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_white_balance_outdoor(NDIlib_recv_instance_t p_instance);

// Use the current brightness to automatically set the current white balance.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_white_balance_oneshot(NDIlib_recv_instance_t p_instance);

// Set the manual camera white balance using the R, B values.
// red = 0.0(not red) ... 1.0(very red)
// blue = 0.0(not blue) ... 1.0(very blue)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_white_balance_manual(NDIlib_recv_instance_t p_instance, const float red, const float blue);

// Put the camera in auto-exposure mode.
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_exposure_auto(NDIlib_recv_instance_t p_instance);

// Manually set the camera exposure iris.
// exposure_level = 0.0(dark) ... 1.0(light)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_exposure_manual(NDIlib_recv_instance_t p_instance, const float exposure_level);

// Manually set the camera exposure parameters.
// iris = 0.0(dark) ... 1.0(light)
// gain = 0.0(dark) ... 1.0(light)
// shutter_speed = 0.0(slow) ... 1.0(fast)
PROCESSINGNDILIB_API
bool NDIlib_recv_ptz_exposure_manual_v2(
	NDIlib_recv_instance_t p_instance,
	const float iris, const float gain, const float shutter_speed
);

// Recording control.
// This will start recording.If the recorder was already recording then the message is ignored.A filename is
// passed in as a "hint".Since the recorder might already be recording(or might not allow complete
// flexibility over its filename), the filename might or might not be used.If the filename is empty, or not
// present, a name will be chosen automatically. If you do not with to provide a filename hint you can simply
// pass NULL.
//
// See note above on depreciation and why this is, and how to replace this functionality.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
bool NDIlib_recv_recording_start(NDIlib_recv_instance_t p_instance, const char* p_filename_hint);

// Stop recording.
//
// See note above on depreciation and why this is, and how to replace this functionality.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
bool NDIlib_recv_recording_stop(NDIlib_recv_instance_t p_instance);

// This will control the audio level for the recording. dB is specified in decibels relative to the reference
// level of the source. Not all recording sources support controlling audio levels.For instance, a digital
// audio device would not be able to avoid clipping on sources already at the wrong level, thus might not
// support this message.
//
// See note above on depreciation and why this is, and how to replace this functionality.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
bool NDIlib_recv_recording_set_audio_level(NDIlib_recv_instance_t p_instance, const float level_dB);

// This will determine if the source is currently recording. It will return true while recording is in
// progress and false when it is not. Because there is one recorded and multiple people might be connected to
// it, there is a chance that it is recording which was initiated by someone else.
//
// See note above on depreciation and why this is, and how to replace this functionality.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
bool NDIlib_recv_recording_is_recording(NDIlib_recv_instance_t p_instance);

// Get the current filename for recording. When this is set it will return a non-NULL value which is owned by
// you and freed using NDIlib_recv_free_string. If a file was already being recorded by another client, the
// massage will contain the name of that file. The filename contains a UNC path (when one is available) to
// the recorded file, and can be used to access the file on your local machine for playback.  If a UNC path
// is not available, then this will represent the local filename. This will remain valid even after the file
// has stopped being recorded until the next file is started.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const char* NDIlib_recv_recording_get_filename(NDIlib_recv_instance_t p_instance);

// This will tell you whether there was a recording error and what that string is. When this is set it will
// return a non-NULL value which is owned by you and freed using NDIlib_recv_free_string. When there is no
// error it will return NULL.
//
// See note above on depreciation and why this is, and how to replace this functionality.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const char* NDIlib_recv_recording_get_error(NDIlib_recv_instance_t p_instance);

// In order to get the duration.
typedef struct NDIlib_recv_recording_time_t
{
	// The number of actual video frames recorded.
	int64_t no_frames;

	// The starting time and current largest time of the record, in UTC time, at 100-nanosecond unit
	// intervals. This allows you to know the record time irrespective of frame rate. For instance,
	// last_time - start_time would give you the recording length in 100-nanosecond intervals.
	int64_t start_time, last_time;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_recording_time_t(void);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_recv_recording_time_t;

// Get the current recording times.
//
// See note above on depreciation and why this is, and how to replace this functionality.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
bool NDIlib_recv_recording_get_times(NDIlib_recv_instance_t p_instance, NDIlib_recv_recording_time_t* p_times);
