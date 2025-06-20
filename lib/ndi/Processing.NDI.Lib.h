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

#ifdef PROCESSINGNDILIB_STATIC
#	ifdef __cplusplus
#		define PROCESSINGNDILIB_API extern "C"
#	else // __cplusplus
#		define PROCESSINGNDILIB_API
#	endif // __cplusplus
#else // PROCESSINGNDILIB_STATIC
#	ifdef _WIN32
#		ifdef PROCESSINGNDILIB_EXPORTS
#			ifdef __cplusplus
#				define PROCESSINGNDILIB_API extern "C" __declspec(dllexport)
#			else // __cplusplus
#				define PROCESSINGNDILIB_API __declspec(dllexport)
#			endif // __cplusplus
#		else // PROCESSINGNDILIB_EXPORTS
#			ifdef __cplusplus
#				define PROCESSINGNDILIB_API extern "C" __declspec(dllimport)
#			else // __cplusplus
#				define PROCESSINGNDILIB_API __declspec(dllimport)
#			endif // __cplusplus
#			ifdef _WIN64
#				define NDILIB_LIBRARY_NAME  "Processing.NDI.Lib.x64.dll"
#				define NDILIB_REDIST_FOLDER "NDI_RUNTIME_DIR_V6"
#				define NDILIB_REDIST_URL    "http://ndi.link/NDIRedistV6"
#			else // _WIN64
#				define NDILIB_LIBRARY_NAME  "Processing.NDI.Lib.x86.dll"
#				define NDILIB_REDIST_FOLDER "NDI_RUNTIME_DIR_V6"
#				define NDILIB_REDIST_URL    "http://ndi.link/NDIRedistV6"
#			endif // _WIN64
#		endif // PROCESSINGNDILIB_EXPORTS
#	else // _WIN32
#		ifdef __APPLE__
#			define NDILIB_LIBRARY_NAME  "libndi.dylib"
#			define NDILIB_REDIST_FOLDER "NDI_RUNTIME_DIR_V6"
#			define NDILIB_REDIST_URL    "http://ndi.link/NDIRedistV6Apple"
#		else // __APPLE__
#			define NDILIB_LIBRARY_NAME  "libndi.so.6"
#			define NDILIB_REDIST_FOLDER "NDI_RUNTIME_DIR_V6"
#			define NDILIB_REDIST_URL    ""
#		endif // __APPLE__
#		ifdef __cplusplus
#			define PROCESSINGNDILIB_API extern "C" __attribute((visibility("default")))
#		else // __cplusplus
#			define PROCESSINGNDILIB_API __attribute((visibility("default")))
#		endif // __cplusplus
#	endif // _WIN32
#endif	// PROCESSINGNDILIB_STATIC

#ifndef PROCESSINGNDILIB_DEPRECATED
#	ifdef _WIN32
#		ifdef _MSC_VER
#			define PROCESSINGNDILIB_DEPRECATED __declspec(deprecated)
#		else // _MSC_VER
#			define PROCESSINGNDILIB_DEPRECATED __attribute((deprecated))
#		endif // _MSC_VER
#	else // _WIN32
#		define PROCESSINGNDILIB_DEPRECATED
#	endif // _WIN32
#endif // PROCESSINGNDILIB_DEPRECATED

#ifndef NDILIB_CPP_DEFAULT_CONSTRUCTORS
#	ifdef __cplusplus
#		define NDILIB_CPP_DEFAULT_CONSTRUCTORS 1
#	else // __cplusplus
#		define NDILIB_CPP_DEFAULT_CONSTRUCTORS 0
#	endif // __cplusplus
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS

#ifndef NDILIB_CPP_DEFAULT_VALUE
#	ifdef __cplusplus
#		define NDILIB_CPP_DEFAULT_VALUE(a) =(a)
#	else // __cplusplus
#		define NDILIB_CPP_DEFAULT_VALUE(a)
#	endif // __cplusplus
#endif // NDILIB_CPP_DEFAULT_VALUE

// Data structures shared by multiple SDKs.
#include "Processing.NDI.compat.h"
#include "Processing.NDI.structs.h"

// This is not actually required, but will start and end the libraries which might get you slightly better
// performance in some cases. In general it is more "correct" to call these although it is not required.
// There is no way to call these that would have an adverse impact on anything (even calling destroy before
// you've deleted all your objects). This will return false if the CPU is not sufficiently capable to run
// NDILib currently NDILib requires SSE4.2 instructions (see documentation). You can verify a specific CPU
// against the library with a call to NDIlib_is_supported_CPU().
PROCESSINGNDILIB_API
bool NDIlib_initialize(void);

PROCESSINGNDILIB_API
void NDIlib_destroy(void);

PROCESSINGNDILIB_API
const char* NDIlib_version(void);

// Recover whether the current CPU in the system is capable of running NDILib.
PROCESSINGNDILIB_API
bool NDIlib_is_supported_CPU(void);

// The finding (discovery API).
#include "Processing.NDI.Find.h"

// The receiving video and audio API.
#include "Processing.NDI.Recv.h"

// Extensions to support PTZ control, etc...
#include "Processing.NDI.Recv.ex.h"

// The receiver advertiser API.
#include "Processing.NDI.RecvAdvertiser.h"

// The receiver listener API.
#include "Processing.NDI.RecvListener.h"

// The sending video API.
#include "Processing.NDI.Send.h"

// The routing of inputs API.
#include "Processing.NDI.Routing.h"

// Utility functions.
#include "Processing.NDI.utilities.h"

// Deprecated structures and functions.
#include "Processing.NDI.deprecated.h"

// The frame synchronizer.
#include "Processing.NDI.FrameSync.h"

// Dynamic loading used for OSS libraries.
#include "Processing.NDI.DynamicLoad.h"

// The C++ implementations.
#if NDILIB_CPP_DEFAULT_CONSTRUCTORS
#include "Processing.NDI.Lib.cplusplus.h"
#endif // NDILIB_CPP_DEFAULT_CONSTRUCTORS
