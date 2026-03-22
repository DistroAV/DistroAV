#pragma once

//-----------------------------------------------------------------------------------------------------------
//
// Copyright (C) 2023-2026 Vizrt NDI AB. All rights reserved.
//
// This file is part of the NDI Advanced SDK and may not be distributed.
//
//-----------------------------------------------------------------------------------------------------------

// This function will tell you whether the current source that this receive is connected to is able to be KVM
// controlled. The value returned by this function might change based on whether the remote source currently
// has KVM enabled or not. If you are receiving data from the video source using NDIlib_recv_capture_v2, then
// you will be notified of a potential change of KVM status when that function
// returns NDIlib_frame_type_status_change.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_is_supported(NDIlib_recv_instance_t p_instance);

// When you want to send a mouse down (i.e. a mouse click has been started) message to the source that this
// receiver is connected too then you would call this function. There are individual functions in order to
// allow you to send a mouse click message for the left, middle and right mouse buttons. In order to simulate
// a double-click operation you would send three messages; the first two would be mouse click messages with
// the third being the mouse release message. It is important that for each click message there must be at
// least one mouse release message or the operating system that you are connected too will continue to
// believe that the mouse button remains pressed forever.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_left_mouse_click(NDIlib_recv_instance_t p_instance);

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_middle_mouse_click(NDIlib_recv_instance_t p_instance);

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_right_mouse_click(NDIlib_recv_instance_t p_instance);

// When you wish to send a mouse release message to a source, then you will call one of these functions. They
// simulate the release of a mouse message and would always be sent after a mouse click message has been sent.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_left_mouse_release(NDIlib_recv_instance_t p_instance);

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_middle_mouse_release(NDIlib_recv_instance_t p_instance);

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_right_mouse_release(NDIlib_recv_instance_t p_instance);

// To simulate a mouse-wheel update you will call this function. You may simulate either a vertical or a
// horizontal wheel update which are separate controls on most platforms. The floating point unit represents
// the number of "units" that you wish the mouse-wheel to be moved, with 1.0 representing a downwards or
// right-hand single unit 

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_vertical_mouse_wheel(NDIlib_recv_instance_t p_instance, float no_units);

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_horizontal_mouse_wheel(NDIlib_recv_instance_t p_instance, float no_units);

// To set the mouse cursor position you will call this function. The coordinates are specified in resolution
// independent settings in the range 0.0 - 1.0 for the current display that you are connected too. The
// resolution of the screen can be known if you are receiving the video source from that device and this
// might change on the fly of course. Position (0,0) is the top left of the screen. posn[0] is the
// x-coordinate of the mouse cursor, posn[1] is the x-coordinate of the mouse cursor.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_mouse_position(NDIlib_recv_instance_t p_instance, const float posn[2]);

// In order to send a new "clipboard buffer" to the destination one would call this function and pass a null
// terminated string that represents the text to be placed on the destination machine buffer. 
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_clipboard_contents(NDIlib_recv_instance_t p_instance, const char* p_clipboard_contents);

// This function will send a touch event to the source that this receiver is connected too. There can be any
// number of simultaneous touch points in an array that is transmitted. The value of no_posns represents how
// many current touch points are used, and the array of posns[] is a list of the [x,y] coordinates in
// floating point of the touch positions. These positions are processed at a higher precision that the screen
// resolution on many systems, with (0,0) being the top-left corner of the screen and (1,1) being the
// bottom-right corner of the screen.
// As an example, if there are two touch positions, then posns[0] would be the x-coordinate of the first
// position, posns[1] would be the y-coordinate of the first position, posns[2] would be the x-coordinate of
// the second position, posns[2] would be the y-coordinate of the second position.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_touch_positions(NDIlib_recv_instance_t p_instance, int no_posns, const float posns[]);

// In order to flush the touch positions (i.e. there are no longer any fingers on the device, then you may
// simply call this function to indicate this to the connected sender.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_touch_finish(NDIlib_recv_instance_t p_instance);

// In order to send a keyboard press event, this function is called. Keyboard messages use the standard
// X-Windows key-sym values. A copy of the standard define that is used for these is included in the NDI SDK
// for your convenience with the filename "Processing.NDI.KVM.keysymdef.h". Since this file includes a large
// number of #defines, it is not included by default when you simply include the NDI SDK files, if you wish
// it to be included you may #define the value NDI_KVM_INCLUDE_KEYSYM or simply include this file into your
// application manually. For every key press event it is important to also send a keyboard release event or
// the destination will believe that there is a "stuck key"! Additional information about key-sym values may
// easily be located online, for instance https://www.tcl.tk/man/tcl/TkCmd/keysyms.html
#ifdef	NDI_KVM_INCLUDE_KEYSYM
#include "Processing.NDI.KVM.keysymdef.h"
#endif	// NDI_KVM_INCLUDE_KEYSYM

PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_keyboard_press(NDIlib_recv_instance_t p_instance, int key_sym_value);

// Once you wish a keyboard value to be "release" then one simply sends the matching keyboard release message
// with the associated key-sym value.
PROCESSINGNDILIB_ADVANCED_API
bool NDIlib_recv_kvm_send_keyboard_release(NDIlib_recv_instance_t p_instance, int key_sym_value);
