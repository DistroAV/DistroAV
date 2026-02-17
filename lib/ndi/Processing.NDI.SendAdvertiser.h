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
// 
// NOTE: It is very important to note the distinction between the automatic advertising done by the NDI
// sender versus the advertising done with this NDI sender advertiser. The NDI sender will always advertise
// via mDNS or the NDI Discovery Server, depending on how it has been configured. The advertising done within
// this NDI sender advertiser API is strictly through the NDI Discovery Server for monitoring purposes. The
// NDI finder or NDI receiver will not use the advertising from the NDI sender advertiser to locate NDI
// sources in order to know their network endpoints or other information.
// 
//***********************************************************************************************************

// The type instance for a sender advertiser.
struct NDIlib_send_advertiser_instance_type;
typedef struct NDIlib_send_advertiser_instance_type* NDIlib_send_advertiser_instance_t;

typedef struct NDIlib_send_advertiser_create_t {
	// The URL address of the NDI Discovery Server to connect to. If NULL, then the default NDI discovery
	// server will be used. If there is no discovery server available, then the sender advertiser will not
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
	NDIlib_send_advertiser_create_t(
		const char* p_url_address NDILIB_CPP_DEFAULT_VALUE(NULL)
	);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_send_advertiser_create_t;

// Create an instance of the sender advertiser. This will return NULL if it fails to create the advertiser.
PROCESSINGNDILIB_API
NDIlib_send_advertiser_instance_t NDIlib_send_advertiser_create(const NDIlib_send_advertiser_create_t* p_create_settings NDILIB_CPP_DEFAULT_VALUE(NULL));

// Destroy an instance of the sender advertiser.
PROCESSINGNDILIB_API
void NDIlib_send_advertiser_destroy(NDIlib_send_advertiser_instance_t p_instance);

// Add the sender to the list of senders that are being advertised. Returns false if the receiver has been
// previously registered.
PROCESSINGNDILIB_API
bool NDIlib_send_advertiser_add_sender(
	NDIlib_send_advertiser_instance_t p_instance,
	NDIlib_send_instance_t p_sender,
	bool allow_monitoring
);

// Remove the sender from the list of senders that are being advertised. Returns false if the sender was not
// previously registered.
PROCESSINGNDILIB_API
bool NDIlib_send_advertiser_del_sender(
	NDIlib_send_advertiser_instance_t p_instance,
	NDIlib_send_instance_t p_sender
);
