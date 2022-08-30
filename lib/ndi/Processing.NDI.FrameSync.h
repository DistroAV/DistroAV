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

// It is important when using video to realize that often you are using difference clocks for different parts
// of the signal chain. Within NDI, the sender can send at the clock rate that it wants and the receiver will
// receive it at that rate. The receiver however is very unlikely to share the exact same clock rate in many
// cases. For instance, bear in mind that computer clocks rely on crystals which while all rated for the same
// frequency are still not exact. If you sending computer has an audio clock that it "thinks" is 48000Hz, to
// the receiver computer that has a different audio clock this might be 48001Hz or 47998Hz. While these
// differences might appear small they accumulate over time and can cause audio to either slightly drift out
// of sync (it is receiving more audio sample than it needs to play back) or might cause audio glitches
// because it is not receiving enough audio samples. While we have described the situation for audio, the
// same exact problem occurs for video sources; it is commonly thought that this can be solved by simply
// having a "frame buffer" and that displaying the "most recently received video frame" will solve these
// timing discrepancies. Unfortunately this is not the case and when it is done because of the variance in
// clock timings, it is very common the video will appear the "jitter" when the sending and receiving clocks
// are almost in alignment. The solution to these problems is to implement a "time base corrector" for the
// video clock which is a device that uses hysteresis to know when the best time is to either drop or insert
// a video frame such that the video is most likely to play back smoothly, and audio should be dynamically
// audio sampled (with a high order resampling filter) to adaptively track any clocking differences.
// Implementing these components is very difficult to get entirely correct under all scenarios and this
// implementation is provided to facilitate this and help people who are building real time video
// applications to receive audio and video without needing to undertake the full complexity of implementing
// such clock devices.
//
// Another way to look at what this class does is that it transforms "push" sources (i.e. NDI sources in
// which the data is pushed from the sender to the receiver) into "pull" sources in which a host application
// is pulling the data down-stream. The frame-sync automatically tracks all clocks to achieve the best video
// performance doing this operation.
//
// In addition to time-base correction operations, these implementations also will automatically detect and
// correct timing jitter that might occur. This will internally correct for timing anomalies that might be
// caused by network, sender or receiver side timing errors caused by CPU limitations, network bandwidth
// fluctuations, etc...
//
// A very common use of a frame-synchronizer might be if you are displaying video on screen timed to the GPU
// v-sync, you should use such a device to convert from the incoming time-base into the time-base of the GPU.
//
// The following are common times that you want to use a frame-synchronizer
//     Video playback on screen : Yes, you want the clock to be synced with vertical refresh.
//     Audio playback through sound card : Yes you want the clock to be synced with your sound card clock.
//     Video mixing : Yes you want the input video clocks to all be synced to your output video clock.
//     Audio mixing : Yes, you want all input audio clocks to be brought into sync with your output
//                    audio clock.
//     Recording a single channel : No, you want to record the signal in it's raw form without
//                                  any re-clocking.
//     Recording multiple channels : Maybe. If you want to sync some input channels to match a master clock
//                                   so that they can be ISO edited, then you might want a frame-sync.

// The type instance for a frame-synchronizer.
struct NDIlib_framesync_instance_type;
typedef struct NDIlib_framesync_instance_type* NDIlib_framesync_instance_t;

// Create a frame synchronizer instance that can be used to get frames from a receiver. Once this receiver
// has been bound to a frame-sync then you should use it in order to receive video frames. You can continue
// to use the underlying receiver for other operations (tally, PTZ, etc...). Note that it remains your
// responsibility to destroy the receiver even when a frame-sync is using it. You should always destroy the
// receiver after the frame-sync has been destroyed.
//
PROCESSINGNDILIB_API
NDIlib_framesync_instance_t NDIlib_framesync_create(NDIlib_recv_instance_t p_receiver);

// Destroy a frame-sync implementation.
PROCESSINGNDILIB_API
void NDIlib_framesync_destroy(NDIlib_framesync_instance_t p_instance);

// This function will pull audio samples from the frame-sync queue. This function will always return data
// immediately, inserting silence if no current audio data is present. You should call this at the rate that
// you want audio and it will automatically adapt the incoming audio signal to match the rate at which you
// are calling by using dynamic audio sampling. Note that you have no obligation that your requested sample
// rate, no channels and no samples match the incoming signal and all combinations of conversions
// are supported.
//
// If you wish to know what the current incoming audio format is, then you can make a call with the
// parameters set to zero and it will then return the associated settings. For instance a call as follows:
//
//     NDIlib_framesync_capture_audio(p_instance, p_audio_data, 0, 0, 0);
//
// will return in p_audio_data the current received audio format if there is one or sample_rate and
// no_channels equal to zero if there is not one. At any time you can specify sample_rate and no_channels as
// zero and it will return the current received audio format.
//
PROCESSINGNDILIB_API
void NDIlib_framesync_capture_audio(
	// The frame sync instance data.
	NDIlib_framesync_instance_t p_instance,
	// The destination audio buffer that you wish to have filled in.
	NDIlib_audio_frame_v2_t* p_audio_data,
	// Your desired sample rate, number of channels and the number of desired samples.
	int sample_rate, int no_channels, int no_samples
);
PROCESSINGNDILIB_API
void NDIlib_framesync_capture_audio_v2(
	// The frame sync instance data.
	NDIlib_framesync_instance_t p_instance,
	// The destination audio buffer that you wish to have filled in.
	NDIlib_audio_frame_v3_t* p_audio_data,
	// Your desired sample rate, number of channels and the number of desired samples.
	int sample_rate, int no_channels, int no_samples
);

// Free audio returned by NDIlib_framesync_capture_audio.
PROCESSINGNDILIB_API
void NDIlib_framesync_free_audio(
	// The frame sync instance data.
	NDIlib_framesync_instance_t p_instance,
	// The destination audio buffer that you wish to have filled in.
	NDIlib_audio_frame_v2_t* p_audio_data
);
PROCESSINGNDILIB_API
void NDIlib_framesync_free_audio_v2(
	// The frame sync instance data.
	NDIlib_framesync_instance_t p_instance,
	// The destination audio buffer that you wish to have filled in.
	NDIlib_audio_frame_v3_t* p_audio_data
);

// This function will tell you the approximate current depth of the audio queue to give you an indication
// of the number of audio samples you can request. Note that if you should treat the results of this function
// with some care because in reality the frame-sync API is meant to dynamically resample audio to match the
// rate that you are calling it. If you have an inaccurate clock then this function can be useful.
// for instance :
//
//  while(true)
//  {   int no_samples = NDIlib_framesync_audio_queue_depth(p_instance);
//      NDIlib_framesync_capture_audio( ... );
//      play_audio( ... )
//      NDIlib_framesync_free_audio( ... )
//      inaccurate_sleep( 33ms );
//  }
//
// Obviously because audio is being received in real-time there is no guarantee after the call that the
// number is correct since new samples might have been captured in that time. On synchronous use of this
// function however this will be the minimum number of samples in the queue at any later time until
// NDIlib_framesync_capture_audio is called.
//
PROCESSINGNDILIB_API
int NDIlib_framesync_audio_queue_depth(NDIlib_framesync_instance_t p_instance);

// This function will pull video samples from the frame-sync queue. This function will always immediately
// return a video sample by using time-base correction. You can specify the desired field type which is then
// used to return the best possible frame. Note that field based frame-synchronization means that the
// frame-synchronizer attempts to match the fielded input phase with the frame requests so that you have the
// most correct possible field ordering on output. Note that the same frame can be returned multiple times.
//
// If no video frame has ever been received, this will return NDIlib_video_frame_v2_t as an empty (all zero)
// structure. The reason for this is that it allows you to determine that there has not yet been any video
// and act accordingly. For instance you might want to display a constant frame output at a particular video
// format, or black.
//
PROCESSINGNDILIB_API
void NDIlib_framesync_capture_video(
	// The frame sync instance data.
	NDIlib_framesync_instance_t p_instance,
	// The destination video buffer that you wish to have filled in.
	NDIlib_video_frame_v2_t* p_video_data,
	// The frame type that you would prefer, all effort is made to match these.
	NDIlib_frame_format_type_e field_type NDILIB_CPP_DEFAULT_VALUE(NDIlib_frame_format_type_progressive)
);

// Free audio returned by NDIlib_framesync_capture_video.
//
PROCESSINGNDILIB_API
void NDIlib_framesync_free_video(
	// The frame sync instance data.
	NDIlib_framesync_instance_t p_instance,
	// The destination video buffer that you wish to have filled in.
	NDIlib_video_frame_v2_t* p_video_data
);
