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

// The type instance for a sender listener.
struct NDIlib_send_listener_instance_type;
typedef struct NDIlib_send_listener_instance_type* NDIlib_send_listener_instance_t;

typedef struct NDIlib_send_listener_create_t {
	// The URL address of the NDI Discovery Server to connect to. If NULL, then the default NDI discovery
	// server will be used. If there is no discovery server available, then the sender listener will not
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
	NDIlib_send_listener_create_t(
		const char* p_url_address = NULL
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_send_listener_create_t;

// Create an instance of the sender listener. This will return NULL if it fails to create the listener.
PROCESSINGNDILIB_API
NDIlib_send_listener_instance_t NDIlib_send_listener_create(const NDIlib_send_listener_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// Destroy an instance of the sender listener.
PROCESSINGNDILIB_API
void NDIlib_send_listener_destroy(NDIlib_send_listener_instance_t p_instance);

// Returns true if the sender listener is actively connected to the configured NDI Discovery Server.
PROCESSINGNDILIB_API
bool NDIlib_send_listener_is_connected(NDIlib_send_listener_instance_t p_instance);

// Retrieve the URL address of the NDI Discovery Server that the sender listener is connected to. This can
// return NULL if the instance pointer is invalid.
PROCESSINGNDILIB_API
const char* NDIlib_send_listener_get_server_url(NDIlib_send_listener_instance_t p_instance);

// Describes a sender that has been discovered.
typedef struct NDIlib_sender_t {
	// The unique identifier for the sender on the network.
	const char* p_uuid;

	// The human-readable name of the sender.
	const char* p_name;

	// Source metadata for the sender.
	const char* p_metadata;

	// The known IP address of the sender.
	const char* p_address;

	// The port number of the sender.
	int port;

	// An array of strings, one for each group that the sender belongs to. The last entry in this list will
	// be NULL.
	const char** p_groups;

	// How many elements are in the p_groups array, excluding the NULL terminating entry.
	uint32_t num_groups;

	// Are we currently subscribed for sender events?
	bool events_subscribed;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_sender_t(void);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_sender_t;

// Retrieves the current list of advertised senders. The memory for the returned structure is only valid
// until the next call or when destroy is called. For a given NDIlib_send_listener_instance_t, do not call
// NDIlib_send_listener_get_senders asynchronously.
PROCESSINGNDILIB_API
const NDIlib_sender_t* NDIlib_send_listener_get_senders(NDIlib_send_listener_instance_t p_instance, uint32_t* p_num_senders);

// This will allow you to wait until the number of online sender has changed.
PROCESSINGNDILIB_API
bool NDIlib_send_listener_wait_for_senders(NDIlib_send_listener_instance_t p_instance, uint32_t timeout_in_ms);

// For backwards compatibility.
typedef NDIlib_listener_event NDIlib_send_listener_event;

// This will subscribe this listener instance to begin receiving events from the specified sender.
PROCESSINGNDILIB_API
void NDIlib_send_listener_subscribe_events(NDIlib_send_listener_instance_t p_instance, const char* p_sender_uuid);

// This will unsubscribe this listener instance from receiving events from the specified sender.
PROCESSINGNDILIB_API
void NDIlib_send_listener_unsubscribe_events(NDIlib_send_listener_instance_t p_instance, const char* p_sender_uuid);

// Returns a list of the currently pending events for the listener. The events are returned in the order that
// they were received. The timeout value is the amount of time in milliseconds that the function will wait
// for events to be received. If the timeout is 0, then the function will return immediately with any events
// that are currently pending. If the timeout is -1, then the function will wait indefinitely for events to
// be received. The function will return NULL if no events were received within the timeout period. The
// returned events should be freed using NDIlib_send_listener_free_events().
PROCESSINGNDILIB_API
const NDIlib_send_listener_event* NDIlib_send_listener_get_events(NDIlib_send_listener_instance_t p_instance, uint32_t* p_num_events, uint32_t timeout_in_ms);

// Frees the memory allocated for the events returned by NDIlib_send_listener_get_events().
PROCESSINGNDILIB_API
void NDIlib_send_listener_free_events(NDIlib_send_listener_instance_t p_instance, const NDIlib_send_listener_event* p_events);
