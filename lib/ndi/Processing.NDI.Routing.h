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

// Structures and type definitions required by NDI routing.
// The reference to an instance of the router.
struct NDIlib_routing_instance_type;
typedef struct NDIlib_routing_instance_type* NDIlib_routing_instance_t;

// The creation structure that is used when you are creating a sender.
typedef struct NDIlib_routing_create_t
{
	// The name of the NDI source to create. This is a NULL terminated UTF8 string.
	const char* p_ndi_name;

	// What groups should this source be part of.
	const char* p_groups;

#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
	NDIlib_routing_create_t(const char* p_ndi_name_ = NULL, const char* p_groups_ = NULL);
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
} NDIlib_routing_create_t;

// Create an NDI routing source.
PROCESSINGNDILIB_API
NDIlib_routing_instance_t NDIlib_routing_create(const NDIlib_routing_create_t* p_create_settings);

// Destroy and NDI routing source.
PROCESSINGNDILIB_API
void NDIlib_routing_destroy(NDIlib_routing_instance_t p_instance);

// Change the routing of this source to another destination.
PROCESSINGNDILIB_API
bool NDIlib_routing_change(NDIlib_routing_instance_t p_instance, const NDIlib_source_t* p_source);

// Change the routing of this source to another destination.
PROCESSINGNDILIB_API
bool NDIlib_routing_clear(NDIlib_routing_instance_t p_instance);

// Get the current number of receivers connected to this source. This can be used to avoid even rendering
// when nothing is connected to the video source. which can significantly improve the efficiency if you want
// to make a lot of sources available on the network. If you specify a timeout that is not 0 then it will
// wait until there are connections for this amount of time.
PROCESSINGNDILIB_API
int NDIlib_routing_get_no_connections(NDIlib_routing_instance_t p_instance, uint32_t timeout_in_ms);

// Retrieve the source information for the given router instance.  This pointer is valid until
// NDIlib_routing_destroy is called.
PROCESSINGNDILIB_API
const NDIlib_source_t* NDIlib_routing_get_source_name(NDIlib_routing_instance_t p_instance);
