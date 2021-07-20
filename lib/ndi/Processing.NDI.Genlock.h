#pragma once

//-----------------------------------------------------------------------------------------------------------
//
// Copyright (C)2014-2021, NewTek, inc.
//
// This file is part of the NDI Advanced SDK and may not be distributed.
//
//-----------------------------------------------------------------------------------------------------------

// The type instance for a genlock object.
typedef void* NDIlib_genlock_instance_t;

// This will create a genlock object. You may specify the NDI source that you wish to lock the signal too,
// and the NDI JSON settings associated with it. It is important that you remember to fill out the vendor_id
// in the JSON or this sender will have the same limitation as NDI receivers. It is legal to create an NDI
// genlock that has a null source name and then later use the NDIlib_genlock_connect function to change the
// connection being used.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_genlock_instance_t NDIlib_genlock_create(const NDIlib_source_t* p_src_settings NDILIB_CPP_DEFAULT_VALUE(NULL), const char* p_config_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// This function will destroy
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_genlock_destroy(NDIlib_genlock_instance_t p_instance);

// If you wish to change the NDI source after having created it, it is possible to call this function and
// change the source that is connected too. This can be used to disconnect a genlock
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_genlock_connect(NDIlib_genlock_instance_t p_instance, const NDIlib_source_t* p_src_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// This function will tell you whether the signal is currently being genlocked too. If the NDI sender that is
// being used as the genlock source is not currently sending an active signal then this will return false.
// Note that the functions to actually perform the clocking operation return whether the genlock is active
// and so this function need not be polled within a sending loop and is provided as a convenience.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_genlock_is_active(NDIlib_genlock_instance_t p_instance);

// This function will wait for the time at which the next video frame is expected. A full frame structure is
// passed into the function, however only the frame-rate and field_type members of the structure are used and
// the rest need not be filled in. The return value tells you whether an NDI genlock signal is present (true)
// or if system clocking is being used to clock the signal (false). Note that audio and video clocks need not
// be in line with each other and can be operated from entirely separate threads.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_genlock_wait_video(NDIlib_genlock_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

// This function will wait for the time at which the next video frame is expected. A full frame structure is
// passed into the function, however only the no_samples and sample_rate members of the structure are used
// and the rest need not be filled in. The return value tells you whether an NDI genlock signal is present
// (true) or if system clocking is being used to clock the signal (false). Note that audio and video clocks
// need not be in line with each other and can be operated from entirely separate threads.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_genlock_wait_audio(NDIlib_genlock_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);
