#pragma once

//-----------------------------------------------------------------------------------------------------------
//
// Copyright (C)2014-2021, NewTek, inc.
//
// This file is part of the Advanced SDK and may not be distributed.
//
//-----------------------------------------------------------------------------------------------------------

#include "Processing.NDI.Lib.h"

#ifndef PROCESSINGNDILIB_ADVANCED_API
#define PROCESSINGNDILIB_ADVANCED_API PROCESSINGNDILIB_API
#endif

#include "Processing.NDI.AVSync.h"
#include "Processing.NDI.Genlock.h"

// Additional values for the NDIlib_FourCC_type_e type that are for usage with NDIlib_video_frame instances
typedef enum NDIlib_FourCC_video_type_ex_e
{	// SpeedHQ formats at the highest bandwidth
	NDIlib_FourCC_video_type_ex_SHQ0_highest_bandwidth = NDI_LIB_FOURCC('S', 'H', 'Q', '0'),		// SpeedHQ 4:2:0
	NDIlib_FourCC_type_SHQ0_highest_bandwidth = NDIlib_FourCC_video_type_ex_SHQ0_highest_bandwidth,	// Backwards compatibility

	NDIlib_FourCC_video_type_ex_SHQ2_highest_bandwidth = NDI_LIB_FOURCC('S', 'H', 'Q', '2'),		// SpeedHQ 4:2:2
	NDIlib_FourCC_type_SHQ2_highest_bandwidth = NDIlib_FourCC_video_type_ex_SHQ2_highest_bandwidth,	// Backwards compatibility

	NDIlib_FourCC_video_type_ex_SHQ7_highest_bandwidth = NDI_LIB_FOURCC('S', 'H', 'Q', '7'),		// SpeedHQ 4:2:2:4
	NDIlib_FourCC_type_SHQ7_highest_bandwidth = NDIlib_FourCC_video_type_ex_SHQ7_highest_bandwidth,	// Backwards compatibility

	// SpeedHQ formats at the lowest bandwidth
	NDIlib_FourCC_video_type_ex_SHQ0_lowest_bandwidth = NDI_LIB_FOURCC('s', 'h', 'q', '0'),			// SpeedHQ 4:2:0
	NDIlib_FourCC_type_SHQ0_lowest_bandwidth = NDIlib_FourCC_video_type_ex_SHQ0_lowest_bandwidth,	// Backwards compatibility

	NDIlib_FourCC_video_type_ex_SHQ2_lowest_bandwidth = NDI_LIB_FOURCC('s', 'h', 'q', '2'),			// SpeedHQ 4:2:2
	NDIlib_FourCC_type_SHQ2_lowest_bandwidth = NDIlib_FourCC_video_type_ex_SHQ2_lowest_bandwidth,	// Backwards compatibility

	NDIlib_FourCC_video_type_ex_SHQ7_lowest_bandwidth = NDI_LIB_FOURCC('s', 'h', 'q', '7'),			// SpeedHQ 4:2:2:4
	NDIlib_FourCC_type_SHQ7_lowest_bandwidth = NDIlib_FourCC_video_type_ex_SHQ7_lowest_bandwidth,	// Backwards compatibility

	// If SpeedHQ 4:4:4 / 4:4:4:4 formats are desired, please contact ndi@newtek.com.

	// H.264 video at the highest bandwidth -- the data field is expected to be prefixed with the
	// NDIlib_compressed_packet_t structure
	NDIlib_FourCC_video_type_ex_H264_highest_bandwidth = NDI_LIB_FOURCC('H', '2', '6', '4'),
	NDIlib_FourCC_type_H264_highest_bandwidth = NDIlib_FourCC_video_type_ex_H264_highest_bandwidth,	// Backwards compatibility

	// H.264 video at the lowest bandwidth -- the data field is expected to be prefixed with the
	// NDIlib_compressed_packet_t structure
	NDIlib_FourCC_video_type_ex_H264_lowest_bandwidth = NDI_LIB_FOURCC('h', '2', '6', '4'),
	NDIlib_FourCC_type_H264_lowest_bandwidth = NDIlib_FourCC_video_type_ex_H264_lowest_bandwidth,	// Backwards compatibility

	// H.265/HEVC video at the highest bandwidth -- the data field is expected to be prefixed with
	// the NDIlib_compressed_packet_t structure
	NDIlib_FourCC_video_type_ex_HEVC_highest_bandwidth = NDI_LIB_FOURCC('H', 'E', 'V', 'C'),
	NDIlib_FourCC_type_HEVC_highest_bandwidth = NDIlib_FourCC_video_type_ex_HEVC_highest_bandwidth,	// Backwards compatibility

	// H.265/HEVC video at the lowest bandwidth -- the data field is expected to be prefixed with
	// the NDIlib_compressed_packet_t structure
	NDIlib_FourCC_video_type_ex_HEVC_lowest_bandwidth = NDI_LIB_FOURCC('h', 'e', 'v', 'c'),
	NDIlib_FourCC_type_HEVC_lowest_bandwidth = NDIlib_FourCC_video_type_ex_HEVC_lowest_bandwidth,	// Backwards compatibility

	// H.264 video at the highest bandwidth -- the data field is expected to be prefixed with the
	// NDIlib_compressed_packet_t structure
	// This version is basically a frame of double the height where to top part is the color and the bottom
	// half has the alpha channel (against chroma being gray).
	NDIlib_FourCC_video_type_ex_H264_alpha_highest_bandwidth = NDI_LIB_FOURCC('A', '2', '6', '4'),
	NDIlib_FourCC_type_h264_alpha_highest_bandwidth = NDIlib_FourCC_video_type_ex_H264_alpha_highest_bandwidth,	// Backwards compatibility

	// H.264 video at the lowest bandwidth -- the data field is expected to be prefixed with the
	// NDIlib_compressed_packet_t structure.
	//
	// This version is basically a frame of double the height where to top part is the color and the bottom
	// half has the alpha channel (against chroma being gray).
	NDIlib_FourCC_video_type_ex_H264_alpha_lowest_bandwidth = NDI_LIB_FOURCC('a', '2', '6', '4'),
	NDIlib_FourCC_type_h264_alpha_lowest_bandwidth = NDIlib_FourCC_video_type_ex_H264_alpha_lowest_bandwidth,	// Backwards compatibility

	// H.265/HEVC video at the highest bandwidth -- the data field is expected to be prefixed with
	// the NDIlib_compressed_packet_t structure.
	//
	// This version is basically a frame of double the height where to top part is the color and the bottom
	// half has the alpha channel (against chroma being gray).
	NDIlib_FourCC_video_type_ex_HEVC_alpha_highest_bandwidth = NDI_LIB_FOURCC('A', 'E', 'V', 'C'),
	NDIlib_FourCC_type_HEVC_alpha_highest_bandwidth = NDIlib_FourCC_video_type_ex_HEVC_alpha_highest_bandwidth,	// Backwards compatibility

	// H.265/HEVC video at the lowest bandwidth -- the data field is expected to be prefixed with
	// the NDIlib_compressed_packet_t structure.
	//
	// This version is basically a frame of double the height where to top part is the color and the bottom
	// half has the alpha channel (against chroma being gray).
	NDIlib_FourCC_video_type_ex_HEVC_alpha_lowest_bandwidth = NDI_LIB_FOURCC('a', 'e', 'v', 'c'),
	NDIlib_FourCC_type_HEVC_alpha_lowest_bandwidth = NDIlib_FourCC_video_type_ex_HEVC_alpha_lowest_bandwidth,	// Backwards compatibility

	// Make sure this is a 32 bit enumeration
	NDIlib_FourCC_video_type_ex_max = 0x7fffffff
} NDIlib_FourCC_video_type_ex_e;

// Additional values for the NDIlib_FourCC_type_e type that are for usage with NDIlib_audio_frame instances
typedef enum NDIlib_FourCC_audio_type_ex_e
{	// AAC audio -- the data field is expected to be prefixed with the NDIlib_compressed_packet_t structure
	NDIlib_FourCC_audio_type_ex_AAC = 0x00ff,

	// Multi channel Opus stream -- unlike other compressed formats, the data field is not expected to be
	// prefixed with the NDIlib_compressed_packet_t structure
	NDIlib_FourCC_audio_type_ex_OPUS = NDI_LIB_FOURCC('O', 'p', 'u', 's'),

	// Make sure this is a 32 bit enumeration
	NDIlib_FourCC_audio_type_ex_max = 0x7fffffff
} NDIlib_FourCC_audio_type_ex_e;

// Additional values for the NDIlib_FourCC_type_e type that are for usage with NDIlib_compressed_packet_t instances
typedef enum NDIlib_compressed_FourCC_type_e
{	// Used in the NDIlib_compressed_packet type to signify H.264 video data that is prefixed with the
	// NDIlib_compressed_packet_t structure
	NDIlib_compressed_FourCC_type_H264 = NDI_LIB_FOURCC('H', '2', '6', '4'),
	NDIlib_FourCC_type_H264 = NDIlib_compressed_FourCC_type_H264,

	// Used in the NDIlib_compressed_packet type to signify H.265/HEVC video data that is prefixed with the
	// NDIlib_compressed_packet_t structure
	NDIlib_compressed_FourCC_type_HEVC = NDI_LIB_FOURCC('H', 'E', 'V', 'C'),
	NDIlib_FourCC_type_HEVC = NDIlib_compressed_FourCC_type_HEVC,

	// Used in the NDIlib_compressed_packet type to signify AAC audio data that is prefixed with the
	// NDIlib_compressed_packet_t structure
	NDIlib_compressed_FourCC_type_AAC = 0x00ff,
	NDIlib_FourCC_type_AAC = NDIlib_compressed_FourCC_type_AAC,

	// Make sure the size is 32bits
	NDIlib_compressed_FourCC_type_max = 0x7fffffff
} NDIlib_compressed_FourCC_type_e;

// Additional values for NDIlib_recv_color_format_e type
typedef enum NDIlib_recv_color_format_ex_e
{	// Request that compressed video data is the desired format.
	NDIlib_recv_color_format_ex_compressed = 300,
	NDIlib_recv_color_format_compressed = NDIlib_recv_color_format_ex_compressed,

	// Allow only compressed SpeedHQ frames to pass through.
	NDIlib_recv_color_format_ex_compressed_v1 = 300,
	NDIlib_recv_color_format_compressed_v1 = NDIlib_recv_color_format_ex_compressed_v1,

	// Allow SpeedHQ frames along with UYVY video to pass through. This would mean HX and H.264 sources can
	// be decompressed and returned back as UYVY.
	NDIlib_recv_color_format_ex_compressed_v2 = 301,
	NDIlib_recv_color_format_compressed_v2 = NDIlib_recv_color_format_ex_compressed_v2,

	// Allow SpeedHQ frames, compressed H.264 frames.
	NDIlib_recv_color_format_ex_compressed_v3 = 302,
	NDIlib_recv_color_format_compressed_v3 = NDIlib_recv_color_format_ex_compressed_v3,

	// Allow SpeedHQ frames, compressed H.264 frames, along with compressed audio frames.
	NDIlib_recv_color_format_ex_compressed_v3_with_audio = 304,
	NDIlib_recv_color_format_compressed_v3_with_audio = NDIlib_recv_color_format_ex_compressed_v3_with_audio,

	// Allow SpeedHQ frames, compressed H.264 frames, HEVC frames.
	NDIlib_recv_color_format_ex_compressed_v4 = 303,
	NDIlib_recv_color_format_compressed_v4 = NDIlib_recv_color_format_ex_compressed_v4,

	// Allow SpeedHQ frames, compressed H.264 frames, HEVC frames, along with compressed audio frames.
	NDIlib_recv_color_format_ex_compressed_v4_with_audio = 305,
	NDIlib_recv_color_format_compressed_v4_with_audio = NDIlib_recv_color_format_ex_compressed_v4_with_audio,

	// Allow SpeedHQ frames, compressed H.264 frames, HEVC frames and HEVC/H264 with alpha.
	NDIlib_recv_color_format_ex_compressed_v5 = 307,
	NDIlib_recv_color_format_compressed_v5 = NDIlib_recv_color_format_ex_compressed_v5,

	// Allow SpeedHQ frames, compressed H.264 frames, HEVC frames and HEVC/H264 with alpha, along with
	// compressed audio frames and OPUS support.
	NDIlib_recv_color_format_ex_compressed_v5_with_audio = 308,
	NDIlib_recv_color_format_compressed_v5_with_audio = NDIlib_recv_color_format_ex_compressed_v5_with_audio,

	// Ensure that this is 32 bits in size.
	NDIlib_recv_color_format_ex_max = 0x7fffffff
} NDIlib_recv_color_format_ex_e;

#pragma pack(push, 1)
static const uint32_t NDIlib_compressed_packet_flags_none = 0;
static const uint32_t NDIlib_compressed_packet_flags_keyframe = 1;
static const  int32_t NDIlib_compressed_packet_version_0 = 44;

typedef struct NDIlib_compressed_packet_t
{
	int32_t version;                         // Structure size (used to get version).
	NDIlib_compressed_FourCC_type_e fourCC;  // The FourCC of this format (e.g. "H264" or "AAC").
	int64_t pts;                             // The presentation time-stamp. In 10000000 intervals.
	int64_t dts;                             // The decode time-stamp. In 10000000 intervals.
	uint64_t reserved;                       // Reserved field.

#ifdef __cplusplus
	static const uint32_t flags_none = 0;
	static const uint32_t flags_keyframe = 1;
#endif

	uint32_t flags;                           // Flags associated with this frame.
	uint32_t data_size;                       // The size of the data that follows the header.
	uint32_t extra_data_size;                 // This is the ancillary data for the stream type. This should
											  // exist for all I-Frames and should be 0 for others.

	/* Data goes here ... If version expands the structure it gets pushed back.
	   uint8_t* p_frame_data = (uint8_t*)p_hdr + p_hdr->version;
	   uint8_t* p_ancilliary_data = p_frame_data + p_hdr->data_size;
	*/

#ifdef __cplusplus
	// Note this is the same on 32 and 64 bit architectures. Be careful that this might not always be the
	// case in the future if you add pointers to this class.
	static const int32_t version_0 = 44;
#endif

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_compressed_packet_t(void) : version(sizeof(NDIlib_compressed_packet_t)), fourCC(NDIlib_compressed_FourCC_type_H264),
		pts(0), dts(0), reserved(0), flags(flags_none), data_size(0), extra_data_size(0) { }
#endif
} NDIlib_compressed_packet_t;
#pragma pack(pop)

// This describes a scatter-gather list that can be used for audio or video data.
typedef struct NDIlib_frame_scatter_t
{	// List of pointers to scatter-gather data. The list should be terminated with a NULL pointer.
	const uint8_t* const* p_data_blocks;

	// A list of sizes for each data block in the scatter-gather list.
	const int* p_data_blocks_size;
} NDIlib_frame_scatter_t;

// This function will get the target frame size for this video. The video_frame structure does not need a
// valid data pointer but the rest of it is used to estimate the target frame size in bytes which you may use
// to estimate the current Q value.
PROCESSINGNDILIB_ADVANCED_API
int NDIlib_send_get_target_frame_size(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

// This is very similar to the function above, except that it returns the target bit-rate in bits per second.
// For I-Frame only schemes, it is better to target each frame size as an target but for more complex codecs
// it is more common to have the average data rate as a bit-rate.
PROCESSINGNDILIB_ADVANCED_API
int NDIlib_send_get_target_bit_rate(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

// For SpeedHQ this will return the target Q value for an NDI stream by using a combination of the resolution
// and the current content history. If doing interlaced video, please ensure that the quality for field 0 and
// field 1 are the same. Only apply a quality change on field 0, and reuse that quality for field 1. If the
// frame passed in is not SpeedHQ then this will return a value of -1.
PROCESSINGNDILIB_ADVANCED_API
int NDIlib_send_get_q_factor(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);

// Given the information in the video_data structure, check to see if the generation of a keyframe required
// by downstream clients. If the video frame is NULL, then keyframe information will be returned back
// regarding any stream bandwidth. If video_data is not NULL, then the video_data's FourCC will determine if
// a keyframe of a matching type is required. In this case, the data pointer is not required to be valid.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_send_is_keyframe_required(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// Returns true if there has been an update for the "keyframe requirement" since the last call to this
// function or if an update occurred during the timeout period. If no update has occurred, false will be
// returned. The NDIlib_send_is_keyframe_required function should be called afterwards to determine the
// precise status of a keyframe needing to be sent. If video_data is not NULL, then the video_data's FourCC
// will determine if a keyframe requirement update has occurred for that matching type, specifically the
// matching bandwidth. In this case, the data pointer is not required to be valid.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_send_wait_for_keyframe_request(NDIlib_send_instance_t p_instance, uint32_t timeout_in_ms, const NDIlib_video_frame_v2_t* p_video_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// Send a video frame synchronously from a scatter-gather list. If the p_video_scatter argument is NULL,
// then it would be as if the NDIlib_send_send_video_v2 were called instead. If the p_video_scatter argument
// is not NULL, then the p_data member of the p_video_data argument will be ignored along with its
// corresponding data_size_in_bytes value. The data represented by the p_video_scatter argument is expected
// to be that of a full video frame. If one were to copy each data segment into a contiguous block, it should
// be essentially a full and valid frame. This function should be used to send only compressed data. When
// sending a H.264 frame via this method, the NDIlib_compressed_packet_t structure is expected to be entirely
// within the memory of the first block. If it is not, the frame will be dropped. The scatter-gather list
// will be ignored for uncompressed frames.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_send_send_video_scatter(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data, const NDIlib_frame_scatter_t* p_video_scatter);

// Send a video frame asynchronously from a scatter-gather list. The rules of the asynchronous send follow
// the rules set by the NDIlib_send_send_video_async_v2 function. If the p_video_scatter argument is NULL,
// then it would be as if the NDIlib_send_send_video_async_v2 were called instead. If the p_video_scatter
// argument is not NULL, then the p_data member of the p_video_data argument will be ignored along with its
// corresponding data_size_in_bytes value. The data represented by the p_video_scatter argument is expected
// to be that of a full video frame. If one were to copy each data segment into a contiguous block, it should
// be essentially a full and valid frame. This function should be used to send only compressed data. When
// sending a H.264 frame via this method, the NDIlib_compressed_packet_t structure is expected to be entirely
// within the memory of the first block. If it is not, the frame will be dropped. The scatter-gather list
// will be ignored for uncompressed frames.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_send_send_video_scatter_async(NDIlib_send_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data, const NDIlib_frame_scatter_t* p_video_scatter);

// Send an audio frame synchronously from a scatter-gather list. If the p_audio_scatter argument is NULL,
// then it would be as if the NDIlib_send_send_audio_v3 were called instead. If the p_audio_scatter argument
// is not NULL, then the p_data member of the p_audio_data argument will be ignored along with its
// corresponding data_size_in_bytes value. The data represented by the p_audio_scatter argument is expected
// to be that of a full audio frame. If one were to copy each data segment into a contiguous block, it should
// be essentially a full and valid frame. This function should be used to send only compressed data. When
// sending an AAC frame via this method, the NDIlib_compressed_packet_t structure is expected to be entirely
// within the memory of the first block. If it is not, the frame will be dropped. The scatter-gather list
// will be ignored for uncompressed frames.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_send_send_audio_scatter(NDIlib_send_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data, const NDIlib_frame_scatter_t* p_audio_scatter);

// This is an extended function of the Advanced SDK that allows someone external to additionally control the
// tally state of an input. For instance it will allow you to specify that you already know that you are on
// air and it will be reflected down-stream to other listeners.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_send_set_tally(NDIlib_send_instance_t p_instance, const NDIlib_tally_t* p_tally);

// Add a connection metadata string to the list of what is sent on each new connection. If someone is already
// connected then this string will be sent to them immediately.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_routing_add_connection_metadata(NDIlib_routing_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

// Connection based metadata is data that is sent automatically each time a new connection is received. You
// queue all of these up and they are sent on each connection. To reset them you need to clear them all and
// set them up again.
PROCESSINGNDILIB_ADVANCED_API
void NDIlib_routing_clear_connection_metadata(NDIlib_routing_instance_t p_instance);

// Create a new finder instance. This will return NULL if it fails. If you specify p_create_settings to be
// NULL, then the finder will be created with default settings.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_find_instance_t NDIlib_find_create_v3(const NDIlib_find_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL), const char* p_config_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// Create a new receiver instance. This will return NULL if it fails. If you specify p_create_settings to be
// NULL, then the receiver will be created with default settings and will automatically determine a receiver
// name.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_recv_instance_t NDIlib_recv_create_v4(const NDIlib_recv_create_v3_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL), const char* p_config_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// Create a new sender instance. This will return NULL if it fails. If you specify p_create_settings to be
// NULL, then the sender will be created with default settings.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_send_instance_t NDIlib_send_create_v2(const NDIlib_send_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL), const char* p_config_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// Create an NDI routing source.
PROCESSINGNDILIB_ADVANCED_API
NDIlib_routing_instance_t NDIlib_routing_create_v2(const NDIlib_routing_create_t* p_create_settings, const char* p_config_data NDILIB_CPP_DEFAULT_VALUE(NULL));

// As of NDI version 5, we support custom memory allocators.
typedef bool (*NDIlib_video_alloc_t)(void* p_opaque, NDIlib_video_frame_v2_t* p_video_data);
typedef bool (*NDIlib_video_free_t)(void* p_opaque, const NDIlib_video_frame_v2_t* p_video_data);
typedef bool (*NDIlib_audio_alloc_t)(void* p_opaque, NDIlib_audio_frame_v3_t* p_audio_data);
typedef bool (*NDIlib_audio_free_t)(void* p_opaque, const NDIlib_audio_frame_v3_t* p_audio_data);

PROCESSINGNDILIB_ADVANCED_API
void NDIlib_recv_set_video_allocator(NDIlib_recv_instance_t p_instance, void* p_opaque, NDIlib_video_alloc_t p_allocator, NDIlib_video_free_t p_deallocator);

PROCESSINGNDILIB_ADVANCED_API
void NDIlib_recv_set_audio_allocator(NDIlib_recv_instance_t p_instance, void* p_opaque, NDIlib_audio_alloc_t p_allocator, NDIlib_audio_free_t p_deallocator);

// This will tell you whether the source that you are connected to supports.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_is_supported(NDIlib_recv_instance_t p_instance);

// When you are working with asynchronous buffers, by default a send operation from the previous send
// operation must have completed before the current one will complete. What this means is that if a sender is
// not able to achieve real-time for some reason (e.g. someone disconnected one a network cable when there
// are two connections connected to a sender) that you might not be able to continue to send video. By
// providing a completion handler on asynchronous sending which means you can always send and then are told
// when a send is fully complete to all senders.
typedef void (*NDIlib_video_send_async_completion_t)(void* p_opaque, const NDIlib_video_frame_v2_t* p_video_data);

PROCESSINGNDILIB_ADVANCED_API
void NDIlib_send_set_video_async_completion(NDIlib_send_instance_t p_instance, void* p_opaque, NDIlib_video_send_async_completion_t p_deallocator);

typedef struct NDIlib_source_v2_t
{	// A UTF8 string that provides a user readable name for this source. This can be used for serialization,
	// etc... and comprises the machine name and the source name on that machine. In the following form,
	//     MACHINE_NAME (NDI_SOURCE_NAME)
	// If you specify this parameter either as NULL, or an EMPTY string then the specific IP address and port
	// number from below is used.
	const char* p_ndi_name;

	// A UTF8 string that provides the actual network address and any parameters. This is not meant to be
	// application readable and might well change in the future. This can be NULL if you do not know it and
	// the API internally will instantiate a finder that is used to discover it even if it is not yet
	// available on the network.
	const char* p_url_address;

	// A UTF8 string that represents the metadata on this connection. This is currently only supported in
	// the Advanced SDK.
	const char* p_metadata;

} NDIlib_source_v2_t;

// This function will recover the current set of sources (i.e. the ones that exist right this second). The
// char* memory buffers returned in NDIlib_source_t are valid until the next call to
// NDIlib_find_get_current_sources_v2 or a call to NDIlib_find_destroy. For a given NDIlib_find_instance_t,
// do not call NDIlib_find_get_current_sources_v2 asynchronously.
PROCESSINGNDILIB_ADVANCED_API
const NDIlib_source_v2_t* NDIlib_find_get_current_sources_v2(NDIlib_find_instance_t p_instance, uint32_t* p_no_sources);
