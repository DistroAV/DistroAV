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

// The type instance for a receiver advertiser.
struct NDIlib_recv_advertiser_instance_type;
typedef struct NDIlib_recv_advertiser_instance_type* NDIlib_recv_advertiser_instance_t;

typedef struct NDIlib_recv_advertiser_create_t {
	// The URL address of the NDI Discovery Server to connect to. If NULL, then the default NDI discovery
	// server will be used. If there is no discovery server available, then the receiver advertiser will not
	// be able to be instantiated and the create function will return NULL. The format of this field is
	// expected to be the hostname or IP address, optionally followed by a colon and a port number. If the
	// port number is not specified, then port 5959 will be used. For example,
	//     127.0.0.1:5959
	//       or
	//     127.0.0.1
	//       or
	//     hostname:5959
	// This field can also specify multiple addresses separated by commas for redundancy support.
	const char* p_url_address;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_recv_advertiser_create_t(
		const char* p_url_address = NULL
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_recv_advertiser_create_t;

// Create an instance of the receiver advertiser. This will return NULL if it fails to create the advertiser.
PROCESSINGNDILIB_API
NDIlib_recv_advertiser_instance_t NDIlib_recv_advertiser_create(const NDIlib_recv_advertiser_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// Destroy an instance of the receiver advertiser.
PROCESSINGNDILIB_API
void NDIlib_recv_advertiser_destroy(NDIlib_recv_advertiser_instance_t p_instance);

// Add the receiver to the list of receivers that are being advertised. Returns false if the receiver has
// been previously registered.
PROCESSINGNDILIB_API
bool NDIlib_recv_advertiser_add_receiver(
	NDIlib_recv_advertiser_instance_t p_instance,
	NDIlib_recv_instance_t p_receiver,
	bool allow_controlling, bool allow_monitoring,
	const char* p_input_group_name NDILIB_CPP_DEFAULT_VALUE(NULL)
);

// Remove the receiver from the list of receivers that are being advertised. Returns false if the receiver
// was not previously registered.
PROCESSINGNDILIB_API
bool NDIlib_recv_advertiser_del_receiver(
	NDIlib_recv_advertiser_instance_t p_instance,
	NDIlib_recv_instance_t p_receiver
);
