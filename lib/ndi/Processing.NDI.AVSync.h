#pragma once

//-----------------------------------------------------------------------------------------------------------
//
// Copyright (C)2014-2021, NewTek, inc.
//
// This file is part of the NDI Advanced SDK and may not be distributed.
//
//-----------------------------------------------------------------------------------------------------------

// The type instance for a AV synchronizer.
typedef void* NDIlib_avsync_instance_t;

// This will create an AV sync object. A receiver is passed into this object and audio will be taken from
// this receiver. One may then use the NDIlib_avsync_instance_t to pass in a video frame as a reference and
// the corresponding audio data received on this device will be returned that matches that video data as
// accurately as possible.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_avsync_instance_t NDIlib_avsync_create(NDIlib_recv_instance_t p_receiver);

// Destroy and AVSync object.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_avsync_destroy(NDIlib_avsync_instance_t p_avsync);

// The returned results from
typedef enum NDIlib_avsync_ret_e
{	// We recovered the audio that you asked for, if you requested an exact number of samples it was
	// returned. If you do not specify the number of audio samples that you want then this is always returned.
	NDIlib_avsync_ret_success = 1,

	// We recovered the audio, however the number of samples you asked for needed to be different to avoid
	// dropping audio data. This is because the remote source is not clocking audio and video sufficiently
	// accurately on the same clock and a different number of audio samples was needed in order to keep it
	// exactly in sync.
	NDIlib_avsync_ret_success_num_samples_not_matched = 2,

	// No audio could be captured that matched this video frame. This is because there is currently no audio
	// from this source.
	NDIlib_avsync_ret_no_audio_stream_received = -1,

	// No audio could be capture that matched this video frame. Audio is currently on this source, however
	// none could be found that matched the video frame. THis is likely because the sync, timestamps or
	// clocks on the remote source are so far away from the expectations. Or that the video has a timestamp
	// that is incorrect. This can also occur if the sender is putting audio and video into the stream in
	// such a way that they are out of sync.
	NDIlib_avsync_ret_no_samples_found = -2,

	// You specified an audio format, but it has changed. You specified an desired audio sample rate or no
	// audio channels however this is not what the audio settings currently are. NDIlib_audio_frame_v3_t has
	// been updated to correctly reflect the results but audio has not been filled in. Call again to get the
	// audio for this video frame.
	NDIlib_avsync_ret_format_changed = -3,

	// Some internal error occurred (e.g. p_avsync was NULL, p_audio_frame was NULL, etc...)
	NDIlib_avsync_ret_internal_error = -4
} NDIlib_avsync_ret_e;

// This is a complex function please review the Advanced SDK documentation for the full parameters around how
// it is used.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_avsync_ret_e NDIlib_avsync_synchronize(NDIlib_avsync_instance_t p_avsync, const NDIlib_video_frame_v2_t* p_video_frame, NDIlib_audio_frame_v3_t* p_audio_frame);

// Free buffers returned from NDIlib_avsync_synchronize.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_avsync_free_audio(NDIlib_avsync_instance_t p_avsync, NDIlib_audio_frame_v3_t* p_audio_frame);
