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

typedef struct NDIlib_v6 {
	// v1.5
	union {
		bool (*initialize)(void);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_initialize)(void);
	};

	union {
		void (*destroy)(void);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_destroy)(void);
	};
	union {
		const char* (*version)(void);
		PROCESSINGNDILIB_DEPRECATED const char* (*NDIlib_version)(void);
	};

	union {
		bool (*is_supported_CPU)(void);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_is_supported_CPU)(void);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED NDIlib_find_instance_t (*find_create)(const NDIlib_find_create_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_find_instance_t (*NDIlib_find_create)(const NDIlib_find_create_t* p_create_settings);
	};

	union {
		NDIlib_find_instance_t (*find_create_v2)(const NDIlib_find_create_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_find_instance_t (*NDIlib_find_create_v2)(const NDIlib_find_create_t* p_create_settings);
	};

	union {
		void (*find_destroy)(NDIlib_find_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_find_destroy)(NDIlib_find_instance_t p_instance);
	};

	union {
		const NDIlib_source_t* (*find_get_sources)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED const NDIlib_source_t* (*NDIlib_find_get_sources)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources, uint32_t timeout_in_ms);
	};

	union {
		NDIlib_send_instance_t (*send_create)(const NDIlib_send_create_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_send_instance_t (*NDIlib_send_create)(const NDIlib_send_create_t* p_create_settings);
	};

	union {
		void (*send_destroy)(NDIlib_send_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_destroy)(NDIlib_send_instance_t p_instance);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*send_send_video)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_video)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*send_send_video_async)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_video_async)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*send_send_audio)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_audio)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);
	};

	union {
		void (*send_send_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	};

	union {
		NDIlib_frame_type_e (*send_capture)(NDIlib_send_instance_t p_instance, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED NDIlib_frame_type_e (*NDIlib_send_capture)(NDIlib_send_instance_t p_instance, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);
	};

	union {
		void (*send_free_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_free_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	};

	union {
		bool (*send_get_tally)(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_send_get_tally)(NDIlib_send_instance_t p_instance, NDIlib_tally_t* p_tally, uint32_t timeout_in_ms);
	};

	union {
		int (*send_get_no_connections)(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED int (*NDIlib_send_get_no_connections)(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms);
	};

	union {
		void (*send_clear_connection_metadata)(NDIlib_send_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_clear_connection_metadata)(NDIlib_send_instance_t p_instance);
	};

	union {
		void (*send_add_connection_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_add_connection_metadata)(NDIlib_send_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	};

	union {
		void (*send_set_failover)(NDIlib_send_instance_t p_instance, const NDIlib_source_t* p_failover_source);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_set_failover)(NDIlib_send_instance_t p_instance, const NDIlib_source_t* p_failover_source);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t (*recv_create_v2)(const NDIlib_recv_create_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t (*NDIlib_recv_create_v2)(const NDIlib_recv_create_t* p_create_settings);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t (*recv_create)(const NDIlib_recv_create_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t (*NDIlib_recv_create)(const NDIlib_recv_create_t* p_create_settings);
	};

	union {
		void (*recv_destroy)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_destroy)(NDIlib_recv_instance_t p_instance);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED NDIlib_frame_type_e (*recv_capture)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_t* p_video_data, NDIlib_audio_frame_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED NDIlib_frame_type_e (*NDIlib_recv_capture)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_t* p_video_data, NDIlib_audio_frame_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*recv_free_video)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_video)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_t* p_video_data);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*recv_free_audio)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_audio)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_t* p_audio_data);
	};

	union {
		void (*recv_free_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	};

	union {
		bool (*recv_send_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_send_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	};

	union {
		bool (*recv_set_tally)(NDIlib_recv_instance_t p_instance, const NDIlib_tally_t* p_tally);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_set_tally)(NDIlib_recv_instance_t p_instance, const NDIlib_tally_t* p_tally);
	};

	union {
		void (*recv_get_performance)(NDIlib_recv_instance_t p_instance, NDIlib_recv_performance_t* p_total, NDIlib_recv_performance_t* p_dropped);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_get_performance)(NDIlib_recv_instance_t p_instance, NDIlib_recv_performance_t* p_total, NDIlib_recv_performance_t* p_dropped);
	};

	union {
		void (*recv_get_queue)(NDIlib_recv_instance_t p_instance, NDIlib_recv_queue_t* p_total);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_get_queue)(NDIlib_recv_instance_t p_instance, NDIlib_recv_queue_t* p_total);
	};

	union {
		void (*recv_clear_connection_metadata)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_clear_connection_metadata)(NDIlib_recv_instance_t p_instance);
	};

	union {
		void (*recv_add_connection_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_add_connection_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);
	};

	union {
		int (*recv_get_no_connections)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED int (*NDIlib_recv_get_no_connections)(NDIlib_recv_instance_t p_instance);
	};

	union {
		NDIlib_routing_instance_t (*routing_create)(const NDIlib_routing_create_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_routing_instance_t (*NDIlib_routing_create)(const NDIlib_routing_create_t* p_create_settings);
	};

	union {
		void (*routing_destroy)(NDIlib_routing_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_routing_destroy)(NDIlib_routing_instance_t p_instance);
	};

	union {
		bool (*routing_change)(NDIlib_routing_instance_t p_instance, const NDIlib_source_t* p_source);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_routing_change)(NDIlib_routing_instance_t p_instance, const NDIlib_source_t* p_source);
	};

	union {
		bool (*routing_clear)(NDIlib_routing_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_routing_clear)(NDIlib_routing_instance_t p_instance);
	};

	union {
		void (*util_send_send_audio_interleaved_16s)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_16s_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_send_send_audio_interleaved_16s)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_16s_t* p_audio_data);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*util_audio_to_interleaved_16s)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_to_interleaved_16s)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*util_audio_from_interleaved_16s)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_from_interleaved_16s)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_t* p_dst);
	};

	// v2
	union {
		bool (*find_wait_for_sources)(NDIlib_find_instance_t p_instance, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_find_wait_for_sources)(NDIlib_find_instance_t p_instance, uint32_t timeout_in_ms);
	};

	union {
		const NDIlib_source_t* (*find_get_current_sources)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources);
		PROCESSINGNDILIB_DEPRECATED const NDIlib_source_t* (*NDIlib_find_get_current_sources)(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*util_audio_to_interleaved_32f)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_to_interleaved_32f)(const NDIlib_audio_frame_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
	};

	union {
		PROCESSINGNDILIB_DEPRECATED void (*util_audio_from_interleaved_32f)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_from_interleaved_32f)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_t* p_dst);
	};

	union {
		void (*util_send_send_audio_interleaved_32f)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_32f_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_send_send_audio_interleaved_32f)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_32f_t* p_audio_data);
	};

	// v3
	union {
		void (*recv_free_video_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_video_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	};

	union {
		void (*recv_free_audio_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_audio_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);
	};

	union {
		NDIlib_frame_type_e (*recv_capture_v2)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_audio_frame_v2_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.
		PROCESSINGNDILIB_DEPRECATED NDIlib_frame_type_e (*NDIlib_recv_capture_v2)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_audio_frame_v2_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.
	};

	union {
		void (*send_send_video_v2)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_video_v2)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	};

	union {
		void (*send_send_video_async_v2)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_video_async_v2)(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	};

	union {
		void (*send_send_audio_v2)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_audio_v2)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v2_t* p_audio_data);
	};

	union {
		void (*util_audio_to_interleaved_16s_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_to_interleaved_16s_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
	};

	union {
		void (*util_audio_from_interleaved_16s_v2)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_from_interleaved_16s_v2)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
	};

	union {
		void (*util_audio_to_interleaved_32f_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_to_interleaved_32f_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
	};

	union {
		void (*util_audio_from_interleaved_32f_v2)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_from_interleaved_32f_v2)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
	};

	// V3.01
	union {
		void (*recv_free_string)(NDIlib_recv_instance_t p_instance, const char* p_string);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_string)(NDIlib_recv_instance_t p_instance, const char* p_string);
	};

	union {
		bool (*recv_ptz_is_supported)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_is_supported)(NDIlib_recv_instance_t p_instance);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED bool (*recv_recording_is_supported)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_recording_is_supported)(NDIlib_recv_instance_t p_instance);
	};

	union {
		const char* (*recv_get_web_control)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED const char* (*NDIlib_recv_get_web_control)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_zoom)(NDIlib_recv_instance_t p_instance, const float zoom_value);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_zoom)(NDIlib_recv_instance_t p_instance, const float zoom_value);
	};

	union {
		bool (*recv_ptz_zoom_speed)(NDIlib_recv_instance_t p_instance, const float zoom_speed);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_zoom_speed)(NDIlib_recv_instance_t p_instance, const float zoom_speed);
	};

	union {
		bool (*recv_ptz_pan_tilt)(NDIlib_recv_instance_t p_instance, const float pan_value, const float tilt_value);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_pan_tilt)(NDIlib_recv_instance_t p_instance, const float pan_value, const float tilt_value);
	};

	union {
		bool (*recv_ptz_pan_tilt_speed)(NDIlib_recv_instance_t p_instance, const float pan_speed, const float tilt_speed);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_pan_tilt_speed)(NDIlib_recv_instance_t p_instance, const float pan_speed, const float tilt_speed);
	};

	union {
		bool (*recv_ptz_store_preset)(NDIlib_recv_instance_t p_instance, const int preset_no);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_store_preset)(NDIlib_recv_instance_t p_instance, const int preset_no);
	};

	union {
		bool (*recv_ptz_recall_preset)(NDIlib_recv_instance_t p_instance, const int preset_no, const float speed);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_recall_preset)(NDIlib_recv_instance_t p_instance, const int preset_no, const float speed);
	};

	union {
		bool (*recv_ptz_auto_focus)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_auto_focus)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_focus)(NDIlib_recv_instance_t p_instance, const float focus_value);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_focus)(NDIlib_recv_instance_t p_instance, const float focus_value);
	};

	union {
		bool (*recv_ptz_focus_speed)(NDIlib_recv_instance_t p_instance, const float focus_speed);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_focus_speed)(NDIlib_recv_instance_t p_instance, const float focus_speed);
	};

	union {
		bool (*recv_ptz_white_balance_auto)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_white_balance_auto)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_white_balance_indoor)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_white_balance_indoor)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_white_balance_outdoor)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_white_balance_outdoor)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_white_balance_oneshot)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_white_balance_oneshot)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_white_balance_manual)(NDIlib_recv_instance_t p_instance, const float red, const float blue);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_white_balance_manual)(NDIlib_recv_instance_t p_instance, const float red, const float blue);
	};

	union {
		bool (*recv_ptz_exposure_auto)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_exposure_auto)(NDIlib_recv_instance_t p_instance);
	};

	union {
		bool (*recv_ptz_exposure_manual)(NDIlib_recv_instance_t p_instance, const float exposure_level);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_exposure_manual)(NDIlib_recv_instance_t p_instance, const float exposure_level);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED bool (*recv_recording_start)(NDIlib_recv_instance_t p_instance, const char* p_filename_hint);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_recording_start)(NDIlib_recv_instance_t p_instance, const char* p_filename_hint);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED bool (*recv_recording_stop)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_recording_stop)(NDIlib_recv_instance_t p_instance);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED bool (*recv_recording_set_audio_level)(NDIlib_recv_instance_t p_instance, const float level_dB);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_recording_set_audio_level)(NDIlib_recv_instance_t p_instance, const float level_dB);
	};

	union {	// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED bool (*recv_recording_is_recording)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_recording_is_recording)(NDIlib_recv_instance_t p_instance);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED const char* (*recv_recording_get_filename)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED const char* (*NDIlib_recv_recording_get_filename)(NDIlib_recv_instance_t p_instance);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED const char* (*recv_recording_get_error)(NDIlib_recv_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED const char* (*NDIlib_recv_recording_get_error)(NDIlib_recv_instance_t p_instance);
	};

	union {
		// This functionality is now provided via external NDI recording, see SDK documentation.
		PROCESSINGNDILIB_DEPRECATED bool (*recv_recording_get_times)(NDIlib_recv_instance_t p_instance, NDIlib_recv_recording_time_t* p_times);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_recording_get_times)(NDIlib_recv_instance_t p_instance, NDIlib_recv_recording_time_t* p_times);
	};

	// v3.1
	union {
		NDIlib_recv_instance_t (*recv_create_v3)(const NDIlib_recv_create_v3_t* p_create_settings);
		PROCESSINGNDILIB_DEPRECATED NDIlib_recv_instance_t (*NDIlib_recv_create_v3)(const NDIlib_recv_create_v3_t* p_create_settings);
	};

	// v3.5
	union {
		void (*recv_connect)(NDIlib_recv_instance_t p_instance, const NDIlib_source_t* p_src);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_connect)(NDIlib_recv_instance_t p_instance, const NDIlib_source_t* p_src);
	};

	// v3.6
	union {
		NDIlib_framesync_instance_t (*framesync_create)(NDIlib_recv_instance_t p_receiver);
		PROCESSINGNDILIB_DEPRECATED NDIlib_framesync_instance_t (*NDIlib_framesync_create)(NDIlib_recv_instance_t p_receiver);
	};

	union {
		void (*framesync_destroy)(NDIlib_framesync_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_destroy)(NDIlib_framesync_instance_t p_instance);
	};

	union {
		void (*framesync_capture_audio)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v2_t* p_audio_data, int sample_rate, int no_channels, int no_samples);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_capture_audio)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v2_t* p_audio_data, int sample_rate, int no_channels, int no_samples);
	};

	union {
		void (*framesync_free_audio)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v2_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_free_audio)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v2_t* p_audio_data);
	};

	union {
		void (*framesync_capture_video)(NDIlib_framesync_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_frame_format_type_e field_type);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_capture_video)(NDIlib_framesync_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_frame_format_type_e field_type);
	};

	union {
		void (*framesync_free_video)(NDIlib_framesync_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_free_video)(NDIlib_framesync_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data);
	};

	union {
		void (*util_send_send_audio_interleaved_32s)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_32s_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_send_send_audio_interleaved_32s)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_interleaved_32s_t* p_audio_data);
	};

	union {
		void (*util_audio_to_interleaved_32s_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_32s_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_to_interleaved_32s_v2)(const NDIlib_audio_frame_v2_t* p_src, NDIlib_audio_frame_interleaved_32s_t* p_dst);
	};

	union {
		void (*util_audio_from_interleaved_32s_v2)(const NDIlib_audio_frame_interleaved_32s_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_audio_from_interleaved_32s_v2)(const NDIlib_audio_frame_interleaved_32s_t* p_src, NDIlib_audio_frame_v2_t* p_dst);
	};

	// v3.8
	union {
		const NDIlib_source_t* (*send_get_source_name)(NDIlib_send_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED const NDIlib_source_t* (*NDIlib_send_get_source_name)(NDIlib_send_instance_t p_instance);
	};

	// v4.0
	union {
		void (*send_send_audio_v3)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_send_send_audio_v3)(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);
	};

	union {
		void (*util_V210_to_P216)(const NDIlib_video_frame_v2_t* p_src_v210, NDIlib_video_frame_v2_t* p_dst_p216);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_V210_to_P216)(const NDIlib_video_frame_v2_t* p_src_v210, NDIlib_video_frame_v2_t* p_dst_p216);
	};

	union {
		void (*util_P216_to_V210)(const NDIlib_video_frame_v2_t* p_src_p216, NDIlib_video_frame_v2_t* p_dst_v210);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_util_P216_to_V210)(const NDIlib_video_frame_v2_t* p_src_p216, NDIlib_video_frame_v2_t* p_dst_v210);
	};

	// v4.1
	union {
		int (*routing_get_no_connections)(NDIlib_routing_instance_t p_instance, uint32_t timeout_in_ms);
		PROCESSINGNDILIB_DEPRECATED int (*NDIlib_routing_get_no_connections)(NDIlib_routing_instance_t p_instance, uint32_t timeout_in_ms);
	};

	union {
		const NDIlib_source_t* (*routing_get_source_name)(NDIlib_routing_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED const NDIlib_source_t* (*NDIlib_routing_get_source_name)(NDIlib_routing_instance_t p_instance);
	};

	union {
		NDIlib_frame_type_e (*recv_capture_v3)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_audio_frame_v3_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.
		PROCESSINGNDILIB_DEPRECATED NDIlib_frame_type_e (*NDIlib_recv_capture_v3)(NDIlib_recv_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data, NDIlib_audio_frame_v3_t* p_audio_data, NDIlib_metadata_frame_t* p_metadata, uint32_t timeout_in_ms);             // The amount of time in milliseconds to wait for data.
	};

	union {
		void (*recv_free_audio_v3)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_recv_free_audio_v3)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);
	};

	union {
		void (*framesync_capture_audio_v2)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v3_t* p_audio_data, int sample_rate, int no_channels, int no_samples);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_capture_audio_v2)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v3_t* p_audio_data, int sample_rate, int no_channels, int no_samples);
	};

	union {
		void (*framesync_free_audio_v2)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v3_t* p_audio_data);
		PROCESSINGNDILIB_DEPRECATED void (*NDIlib_framesync_free_audio_v2)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v3_t* p_audio_data);
	};

	union {
		int (*framesync_audio_queue_depth)(NDIlib_framesync_instance_t p_instance);
		PROCESSINGNDILIB_DEPRECATED int (*NDIlib_framesync_audio_queue_depth)(NDIlib_framesync_instance_t p_instance);
	};

	// v5
	union {
		bool (*recv_ptz_exposure_manual_v2)(NDIlib_recv_instance_t p_instance, const float iris, const float gain, const float shutter_speed);
		PROCESSINGNDILIB_DEPRECATED bool (*NDIlib_recv_ptz_exposure_manual_v2)(NDIlib_recv_instance_t p_instance, const float iris, const float gain, const float shutter_speed);
	};

	// v6.1
	bool (*util_audio_to_interleaved_16s_v3)(const NDIlib_audio_frame_v3_t* p_src, NDIlib_audio_frame_interleaved_16s_t* p_dst);
	bool (*util_audio_from_interleaved_16s_v3)(const NDIlib_audio_frame_interleaved_16s_t* p_src, NDIlib_audio_frame_v3_t* p_dst);
	bool (*util_audio_to_interleaved_32s_v3)(const NDIlib_audio_frame_v3_t* p_src, NDIlib_audio_frame_interleaved_32s_t* p_dst);
	bool (*util_audio_from_interleaved_32s_v3)(const NDIlib_audio_frame_interleaved_32s_t* p_src, NDIlib_audio_frame_v3_t* p_dst);
	bool (*util_audio_to_interleaved_32f_v3)(const NDIlib_audio_frame_v3_t* p_src, NDIlib_audio_frame_interleaved_32f_t* p_dst);
	bool (*util_audio_from_interleaved_32f_v3)(const NDIlib_audio_frame_interleaved_32f_t* p_src, NDIlib_audio_frame_v3_t* p_dst);

	// v6.2
	bool (*recv_get_source_name)(NDIlib_recv_instance_t p_instance, const char** p_source_name, uint32_t timeout_in_ms);

	NDIlib_recv_advertiser_instance_t (*recv_advertiser_create)(const NDIlib_recv_advertiser_create_t* p_create_settings);
	void (*recv_advertiser_destroy)(NDIlib_recv_advertiser_instance_t p_instance);
	bool (*recv_advertiser_add_receiver)(NDIlib_recv_advertiser_instance_t p_instance, NDIlib_recv_instance_t p_receiver, bool allow_controlling, bool allow_monitoring, const char* p_input_group_name);
	bool (*recv_advertiser_del_receiver)(NDIlib_recv_advertiser_instance_t p_instance, NDIlib_recv_instance_t p_receiver);

	NDIlib_recv_listener_instance_t (*recv_listener_create)(const NDIlib_recv_listener_create_t* p_create_settings);
	void (*recv_listener_destroy)(NDIlib_recv_listener_instance_t p_instance);
	bool (*recv_listener_is_connected)(NDIlib_recv_listener_instance_t p_instance);
	const char* (*recv_listener_get_server_url)(NDIlib_recv_listener_instance_t p_instance);
	const NDIlib_receiver_t* (*recv_listener_get_receivers)(NDIlib_recv_listener_instance_t p_instance, uint32_t* p_num_receivers);
	bool (*recv_listener_wait_for_receivers)(NDIlib_recv_listener_instance_t p_instance, uint32_t timeout_in_ms);
} NDIlib_v6;

typedef struct NDIlib_v6 NDIlib_v5;
typedef struct NDIlib_v6 NDIlib_v4_5;
typedef struct NDIlib_v6 NDIlib_v4;
typedef struct NDIlib_v6 NDIlib_v3;
typedef struct NDIlib_v6 NDIlib_v2;

// Load the library.
PROCESSINGNDILIB_API
const NDIlib_v6* NDIlib_v6_load(void);

// Load the library.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_v5* NDIlib_v5_load(void);

// Load the library.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_v4_5* NDIlib_v4_5_load(void);

// Load the library.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_v4* NDIlib_v4_load(void);

// Load the library.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_v3* NDIlib_v3_load(void);

// Load the library.
PROCESSINGNDILIB_API PROCESSINGNDILIB_DEPRECATED
const NDIlib_v2* NDIlib_v2_load(void);
