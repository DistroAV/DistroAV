#pragma once

//***********************************************************************************************************************************************
// 
// Copyright(c) 2014-2018 NewTek, inc
//
//***********************************************************************************************************************************************

// It is important when using video to realize that often you are using differenc clocks
// for different parts of the signal chain. Within NDI, the sender can send at the clock rate
// that it wants and the receiver will receive it at that rate. The receiver however is very
// unlikely to share the exact same clock rate in many cases. For instance, bear in mind that
// computer clocks rely on crystals which while all rated for the same frequency are still not
// exact. If you sending computer has an audio clock that it "thinks" is 48000Hz, to the receiver
// computer that has a different audio clock this might be 48001Hz or 47998Hz. While these 
// differences might appear small they accumulate over time and can cause audio to either 
// slightly drift out of sync (it is receiving more audio sample sthan it needs to play back)
// or might cause audio glitches because it is not receiving enough audio samples. While we
// have described the situation for audio, the same exact problem occurs for video sources;
// it is commonly thought that this can be solved by simply having a "frame buffer" and that
// displaying the "most recently received video frame" will solve these timing discrepencies.
// Unfortunately this is not the case and when it is done because of the variance in clock 
// timings, it is veyr common the the video will appear the "jitter" when the sending and 
// receiving closks are almost in alignment. The solution to these problems is to implement
// a "time base corrector" for the video clock which is a device that uses hysterisis to know
// when the best time is to either drop or insert a video frame such that the video is most
// likely to play back smoothly, and audio should be dynamically audio sampled (with a high
// order resampling filter) to adaptively track any clocking differences. Implementing these
// components is very difficult to get entirely correct under all scenarios and this 
// implementation is provided to facilitate this and help people who are building real time
// video applications to receive audio and video without needing to undertake the full 
// complexity of implementing such clock devices. 
//
// Another way to look at what this class does is that it transforms "push" sources (i.e.
// NDI sources in which the data is pushed from the sender to the receiver) into "pull" 
// sources in which a host application is pulling the data down-stream. The frame-sync
// automatically tracks all clocks to acheive the best video performance doing this 
// operation. 
//
// In addition to time-base correction operations, these implementations also will automatically
// detect and correct timing jitter that might occur. This will internally correct for timing
// anomolies that might be caused by network, sender or receiver side timing errors caused
// by CPU limitatoins, network bandwidth fluctuations, etc...
//
// A very common use of a frame-synchronizer might be if you are displaying video on screen 
// timed to the GPU v-sync, you should use sych a device to convert from the incoming time-base
// into the time-base of the GPU.
//
// The following are common times that you want to use a frame-synchronizer
//     Video playback on screen : Yes, you want the clock to be synced with vertical refresh.
//     Audio playback through sound card : Yes you want the clock to be synced with your sound card clock.
//     Video mixing : Yes you want the input video clocks to all be synced to your output video clock.
//     Audio mixing : Yes, you want all input audio clocks to be brough into sync with your output audio clock.
//     Recording a single channel : No, you want to record the signal in it's raw form without any reclocking.
//     Recording multiple channels : Maybe. If you want to sync some input channels to match a master clock so that
//                                   they can be ISO edited, then you might want a frame-sync.

// The type instance for a frame-synchronizer
typedef void* NDIlib_framesync_instance_t;

// Create a frame synchronizer instance that can be used to get frames
// from a receiver. Once this receiver has been bound to a frame-sync
// then you should use it in order to receover video frames. You can 
// continue to use the underlying receiver for other operations (tally,
// PTZ, etc...). Note that it remains your responsability to destroy the
// receiver even when a frame-sync is using it. You should always destroy
// the receiver after the framesync has been destroyed.
PROCESSINGNDILIB_API
NDIlib_framesync_instance_t NDIlib_framesync_create(NDIlib_recv_instance_t p_receiver);

// Destroy a frame-sync implemenration
PROCESSINGNDILIB_API
void NDIlib_framesync_destroy(NDIlib_framesync_instance_t p_instance);

// This function will pull audio samples from the frame-sync queue. This function
// will always return data immediately, inserting silence if no current audio
// data is present. You should call this at the rate that you want audio and it
// will automatically adapt the incoming audio signal to match the rate at which
// you are calling by using dynamic audio sampling. Note that you have no obligation
// that your requested sample rate, no channels and no samples match the incoming signal
// and all combinations of conversions are supported. If you specify a sample-rate=0
// or no_channels=0 then it will use the original sample rate of the buffer.
PROCESSINGNDILIB_API
void NDIlib_framesync_capture_audio(// The frame sync instance data
                                    NDIlib_framesync_instance_t p_instance,
                                    // The destination audio buffer that you wish to have filled in.
                                    NDIlib_audio_frame_v2_t* p_audio_data,
                                    // Your desired sample rate, number of channels and the number of desired samples.
                                    int sample_rate, int no_channels, int no_samples);

// Free audio returned by NDIlib_framesync_capture_audio
PROCESSINGNDILIB_API
void NDIlib_framesync_free_audio(// The frame sync instance data
                                 NDIlib_framesync_instance_t p_instance,
                                 // The destination audio buffer that you wish to have filled in.
                                 NDIlib_audio_frame_v2_t* p_audio_data);

// This function will pull video samples from the frame-sync queue. This function
// will always immediately return a video sample by using time-base correction. You can 
// specify the desired field type which is then used to return the best possible frame.
// Note that field based frame-synronization means that the frame-synchronizer attempts
// to match the fielded input phase with the frame requests so that you have the most
// correct possible field ordering on output. Note that the same frame can be returned
// multiple times.
//
// If no video frame has evern been received, this will return NDIlib_video_frame_v2_t as 
// an empty (all zero) structure. The reason for this is that it allows you to determine
// that there has not yet been any video and act accordingly. For instamce you might want
// to display a constant frame output at a particular video format, or black.
PROCESSINGNDILIB_API
void NDIlib_framesync_capture_video(// The frame sync instance data
                                    NDIlib_framesync_instance_t p_instance,
                                    // The destination audio buffer that you wish to have filled in.
                                    NDIlib_video_frame_v2_t* p_video_data,
                                    // The frame type that you would prefer, all effort is made to match these.
                                    NDIlib_frame_format_type_e field_type NDILIB_CPP_DEFAULT_VALUE(NDIlib_frame_format_type_progressive));

// Free audio returned by NDIlib_framesync_capture_video
PROCESSINGNDILIB_API
void NDIlib_framesync_free_video(// The frame sync instance data
                                 NDIlib_framesync_instance_t p_instance,
                                 // The destination audio buffer that you wish to have filled in.
                                 NDIlib_video_frame_v2_t* p_video_data);
