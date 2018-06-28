#pragma once

// NOTE : The following MIT license applies to this file ONLY and not to the SDK as a whole. Please review the SDK documentation 
// for the description of the full license terms, which are also provided in the file "NDI License Agreement.pdf" within the SDK or 
// online at http://new.tk/ndisdk_license/. Your use of any part of this SDK is acknowledgment that you agree to the SDK license 
// terms. The full NDI SDK may be downloaded at https://www.newtek.com/ndi/sdk/
//
//***********************************************************************************************************************************************
// 
// Copyright(c) 2014-2018 NewTek, inc
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

//****************************************************************************************************************
typedef struct NDIlib_v3
{	// V1.5
	bool(*NDIlib_initialize)(void);
	void(*NDIlib_destroy)(void);
	const char* (*NDIlib_version)(void);
	bool(*NDIlib_is_supported_CPU)(void);
	PROCESSINGNDILIB_DEPRECATED NDIlib_find_instance_t(*NDIlib_find_create)(const NDIlib_find_create_t* p_create_settings);
	NDIlib_find_instance_t(*NDIlib_find_create_v2)(const NDIlib_find_create_t* p_create_settings);
	void(*NDIlib_find_destroy)(NDIlib_find_instance_t p_instance);
	const NDIlib_source_t* (*NDIlib_find_get_sources)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources, uint32_t timeout_in_ms);
	NDIlib_send_instance_t(*NDIlib_send_create)(const NDIlib_send_create_t* p_create_settings);
	void(*NDIlib_send_destroy)(NDIlib_send_instance_t p_instance);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_send_send_video)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_send_send_video_async)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_send_send_audio)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);
	void(*NDIlib_send_send_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	NDIlib_frame_type_e(*NDIlib_send_capture)(NDIlib_send_instance_t p_instance, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);
	void(*NDIlib_send_free_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	bool(*NDIlib_send_get_tally)(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms);
	int(*NDIlib_send_get_no_connections)(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms);
	void(*NDIlib_send_clear_connection_metadata)(NDIlib_send_instance_t p_instance);
	void(*NDIlib_send_add_connection_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	void(*NDIlib_send_set_failover)(NDIlib_send_instance_t p_instance, const NDIlib_source_t* p_failover_source);
	PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t(*NDIlib_recv_create_v2)(const NDIlib_recv_create_t* p_create_settings);
	PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t(*NDIlib_recv_create)(const NDIlib_recv_create_t* p_create_settings);
	void(*NDIlib_recv_destroy)(NDIlib_recv_instance_t p_instance);
	PROCESSINGNDILIB_DEPRECATED NDIlib_frame_type_e(*NDIlib_recv_capture)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_t* p_video_data, NDIlib_audio_frame_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_recv_free_video)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_recv_free_audio)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);
	void(*NDIlib_recv_free_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	bool(*NDIlib_recv_send_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	bool(*NDIlib_recv_set_tally)(NDIlib_recv_instance_t p_instance, const NDIlib_tally_t* p_tally);
	void(*NDIlib_recv_get_performance)(NDIlib_recv_instance_t p_instance, NDIlib_recv_performance_t* p_total, NDIlib_recv_performance_t* p_dropped);
	void(*NDIlib_recv_get_queue)(NDIlib_recv_instance_t p_instance, NDIlib_recv_queue_t* p_total);
	void(*NDIlib_recv_clear_connection_metadata)(NDIlib_recv_instance_t p_instance);
	void(*NDIlib_recv_add_connection_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	int(*NDIlib_recv_get_no_connections)(NDIlib_recv_instance_t p_instance);
	NDIlib_routing_instance_t(*NDIlib_routing_create)(const NDIlib_routing_create_t* p_create_settings);
	void(*NDIlib_routing_destroy)(NDIlib_routing_instance_t p_instance);
	bool(*NDIlib_routing_change)(NDIlib_routing_instance_t p_instance, const NDIlib_source_t* p_source);
	bool(*NDIlib_routing_clear)(NDIlib_routing_instance_t p_instance);
	void(*NDIlib_util_send_send_audio_interleaved_16s)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_16s_t* p_audio_data);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_util_audio_to_interleaved_16s)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_util_audio_from_interleaved_16s)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_t* p_dst);
	// V2
	bool(*NDIlib_find_wait_for_sources)(NDIlib_find_instance_t p_instance, uint32_t timeout_in_ms);
	const NDIlib_source_t* (*NDIlib_find_get_current_sources)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_util_audio_to_interleaved_32f)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
	PROCESSINGNDILIB_DEPRECATED void(*NDIlib_util_audio_from_interleaved_32f)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_t* p_dst);
	void(*NDIlib_util_send_send_audio_interleaved_32f)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_32f_t* p_audio_data);
	// V3
	void(*NDIlib_recv_free_video_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	void(*NDIlib_recv_free_audio_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);
	NDIlib_frame_type_e(*NDIlib_recv_capture_v2)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_audio_frame_v2_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.
	void(*NDIlib_send_send_video_v2)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	void(*NDIlib_send_send_video_async_v2)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	void(*NDIlib_send_send_audio_v2)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);
	void(*NDIlib_util_audio_to_interleaved_16s_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
	void(*NDIlib_util_audio_from_interleaved_16s_v2)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
	void(*NDIlib_util_audio_to_interleaved_32f_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
	void(*NDIlib_util_audio_from_interleaved_32f_v2)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
	// V3.01
	void(*NDIlib_recv_free_string)(NDIlib_recv_instance_t p_instance, const char* p_string);
	bool(*NDIlib_recv_ptz_is_supported)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_recording_is_supported)(NDIlib_recv_instance_t p_instance);
	const char*(*NDIlib_recv_get_web_control)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_zoom)(NDIlib_recv_instance_t p_instance, const float zoom_value);
	bool(*NDIlib_recv_ptz_zoom_speed)(NDIlib_recv_instance_t p_instance, const float zoom_speed);
	bool(*NDIlib_recv_ptz_pan_tilt)(NDIlib_recv_instance_t p_instance, const float pan_value, const float tilt_value);
	bool(*NDIlib_recv_ptz_pan_tilt_speed)(NDIlib_recv_instance_t p_instance, const float pan_speed, const float tilt_speed);
	bool(*NDIlib_recv_ptz_store_preset)(NDIlib_recv_instance_t p_instance, const int preset_no);
	bool(*NDIlib_recv_ptz_recall_preset)(NDIlib_recv_instance_t p_instance, const int preset_no, const float speed);
	bool(*NDIlib_recv_ptz_auto_focus)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_focus)(NDIlib_recv_instance_t p_instance, const float focus_value);
	bool(*NDIlib_recv_ptz_focus_speed)(NDIlib_recv_instance_t p_instance, const float focus_speed);
	bool(*NDIlib_recv_ptz_white_balance_auto)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_white_balance_indoor)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_white_balance_outdoor)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_white_balance_oneshot)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_white_balance_manual)(NDIlib_recv_instance_t p_instance, const float red, const float blue);
	bool(*NDIlib_recv_ptz_exposure_auto)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_ptz_exposure_manual)(NDIlib_recv_instance_t p_instance, const float exposure_level);
	bool(*NDIlib_recv_recording_start)(NDIlib_recv_instance_t p_instance, const char* p_filename_hint);
	bool(*NDIlib_recv_recording_stop)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_recording_set_audio_level)(NDIlib_recv_instance_t p_instance, const float level_dB);
	bool(*NDIlib_recv_recording_is_recording)(NDIlib_recv_instance_t p_instance);
	const char*(*NDIlib_recv_recording_get_filename)(NDIlib_recv_instance_t p_instance);
	const char*(*NDIlib_recv_recording_get_error)(NDIlib_recv_instance_t p_instance);
	bool(*NDIlib_recv_recording_get_times)(NDIlib_recv_instance_t p_instance, NDIlib_recv_recording_time_t* p_times);
	// V3.10
	NDIlib_recv_instance_t(*NDIlib_recv_create_v3)(const NDIlib_recv_create_v3_t* p_create_settings);
	// V3.5
	void(*NDIlib_recv_connect)(NDIlib_recv_instance_t p_instance, const NDIlib_source_t* p_src);

} NDIlib_v3;

typedef struct NDIlib_v3 NDIlib_v2;

// Load the library
PROCESSINGNDILIB_API
const NDIlib_v3* NDIlib_v3_load(void);

PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_v2* NDIlib_v2_load(void);
