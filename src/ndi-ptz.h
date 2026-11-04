/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>
	Copyright (C) 2026 Grant Likely <grant.likely@secretlab.ca>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.

	SPDX-License-Identifier: GPL-2.0-or-later
******************************************************************************/

#pragma once

#include "plugin-main.h"

// PTZ commands for an NDI source's receiver.
//
// Any thread posts a command; only the source thread, which owns the
// receiver, drains them and calls recv_ptz_*.
struct ndi_ptz_t;

ndi_ptz_t *ndi_ptz_create(obs_source_t *source);
void ndi_ptz_destroy(ndi_ptz_t *p);

enum class NdiPtzCmdKind {
	PanTiltSpeed,
	PanTiltAbs,
	ZoomSpeed,
	ZoomAbs,
	FocusSpeed,
	FocusAbs,
	AutoFocus,
	WbOnepush,
	PresetStore,
	PresetRecall,
	Stop,
	Count,
};

struct NdiPtzCmd {
	NdiPtzCmdKind kind;
	float a = 0.0f;
	float b = 0.0f;
	int slot = 0;
};

// Any thread. A later command of the same kind replaces an earlier one.
// Stop replaces the pending pan/tilt, zoom and focus speeds with zero.
void ndi_ptz_post(ndi_ptz_t *p, const NdiPtzCmd &cmd);

// Atomic read: is there anything for the source thread to send?
bool ndi_ptz_pending(const ndi_ptz_t *p);

// Drops what is pending, for a receiver that is gone or does not do PTZ.
void ndi_ptz_clear(ndi_ptz_t *p);

// Source thread only. Sends and clears what is pending; returns how many.
int ndi_ptz_drain(ndi_ptz_t *p, NDIlib_recv_instance_t recv);

// PTZ Device API (obs-ptz's ptz-device-api.md), on the source's own
// proc_handler and signal_handler.
#define NDI_PTZ_API_MAJOR 0
#define NDI_PTZ_API_MINOR 10

// Registers the procs and signals on the source. Call once, from create.
void ndi_ptz_register(ndi_ptz_t *p);

// From the source's update(): speed caps, inversion and the saved presets.
void ndi_ptz_update_settings(ndi_ptz_t *p, obs_data_t *settings);

// For get_defaults(): the settings of the device the source keeps.
void ndi_ptz_get_defaults(obs_data_t *settings);

// Source thread: whether the receiver can do PTZ has been seen again.
// Announces a change as ptz_state_changed.
void ndi_ptz_set_connected(ndi_ptz_t *p, bool supported);

// The source was renamed: its state's "source" follows it.
void ndi_ptz_source_renamed(ndi_ptz_t *p);
