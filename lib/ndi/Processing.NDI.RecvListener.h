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

// The type instance for a receiver listener.
struct NDIlib_recv_listener_instance_type;
typedef struct NDIlib_recv_listener_instance_type* NDIlib_recv_listener_instance_t;

typedef struct NDIlib_recv_listener_create_t {
	// The URL address of the NDI Discovery Server to connect to. If NULL, then the default NDI discovery
	// server will be used. If there is no discovery server available, then the receiver listener will not
	// be able to be instantiated and the create function will return NULL. The format of this field is
	// expected to be the hostname or IP address, optionally followed by a colon and a port number. If the
	// port number is not specified, then port 5959 will be used. For example,
	//     127.0.0.1:5959
	//       or
	//     127.0.0.1
	//       or
	//     hostname:5959
	// If this field is a comma-separated list, then only the first address will be used.
	const char* p_url_address;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_listener_create_t(
		const char* p_url_address = NULL
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_recv_listener_create_t;

// Create an instance of the receiver listener. This will return NULL if it fails to create the listener.
PROCESSINGNDILIB_API
NDIlib_recv_listener_instance_t NDIlib_recv_listener_create(const NDIlib_recv_listener_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// Destroy an instance of the receiver listener.
PROCESSINGNDILIB_API
void NDIlib_recv_listener_destroy(NDIlib_recv_listener_instance_t p_instance);

// Returns true if the receiver listener is actively connected to the configured NDI Discovery Server.
PROCESSINGNDILIB_API
bool NDIlib_recv_listener_is_connected(NDIlib_recv_listener_instance_t p_instance);

// Retrieve the URL address of the NDI Discovery Server that the receiver listener is connected to. This can
// return NULL if the instance pointer is invalid.
PROCESSINGNDILIB_API
const char* NDIlib_recv_listener_get_server_url(NDIlib_recv_listener_instance_t p_instance);

// The types of streams that a receiver can receive from the source it's connected to.
typedef enum NDIlib_receiver_type_e {
	NDIlib_receiver_type_none = 0,
	NDIlib_receiver_type_metadata = 1,
	NDIlib_receiver_type_video = 2,
	NDIlib_receiver_type_audio = 3,

	// Make sure this is a 32-bit enumeration.
	NDIlib_receiver_type_max = 0x7fffffff
} NDIlib_receiver_type_e;

// The types of commands that a receiver can process.
typedef enum NDIlib_receiver_command_e {
	NDIlib_receiver_command_none = 0,

	// A receiver can be told to connect to a specific source.
	NDIlib_receiver_command_connect = 1,

	// Make sure this is a 32-bit enumeration.
	NDIlib_receiver_command_max = 0x7fffffff
} NDIlib_receiver_command_e;

// Describes a receiver that has been discovered.
typedef struct NDIlib_receiver_t {
	// The unique identifier for the receiver on the network.
	const char* p_uuid;

	// The human-readable name of the receiver.
	const char* p_name;

	// The unique identifier for the input group that the receiver belongs to.
	const char* p_input_uuid;

	// The human-readable name of the input group that the receiver belongs to.
	const char* p_input_name;

	// The known IP address of the receiver.
	const char* p_address;

	// An array of streams that the receiver is set to receive. The last entry in this list will be
	// NDIlib_receiver_type_none.
	NDIlib_receiver_type_e* p_streams;

	// How many elements are in the p_streams array, excluding the NDIlib_receiver_type_none entry.
	uint32_t num_streams;

	// An array of commands that the receiver can process. The last entry in this list will be
	// NDIlib_receiver_command_none.
	NDIlib_receiver_command_e* p_commands;

	// How many elements are in the p_commands array, excluding the NDIlib_receiver_command_none entry.
	uint32_t num_commands;

	// Are we currently subscribed for receive events?
	bool events_subscribed;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_receiver_t(void);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_receiver_t;

// Retrieves the current list of advertised receivers. The memory for the returned structure is only valid
// until the next call or when destroy is called. For a given NDIlib_recv_listener_instance_t, do not call
// NDIlib_recv_listener_get_receivers asynchronously.
PROCESSINGNDILIB_API
const NDIlib_receiver_t* NDIlib_recv_listener_get_receivers(NDIlib_recv_listener_instance_t p_instance, uint32_t* p_num_receivers);

// This will allow you to wait until the number of online receivers has changed.
PROCESSINGNDILIB_API
bool NDIlib_recv_listener_wait_for_receivers(NDIlib_recv_listener_instance_t p_instance, uint32_t timeout_in_ms);
