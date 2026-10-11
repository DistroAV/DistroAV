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

#include "ndi-ptz.h"

#include <array>
#include <atomic>
#include <mutex>

struct ndi_ptz_t {
	obs_source_t *source;

	std::mutex lock;
	std::array<NdiPtzCmd, (size_t)NdiPtzCmdKind::Count> slot;
	std::array<bool, (size_t)NdiPtzCmdKind::Count> present;
	std::atomic<bool> pending;
};

ndi_ptz_t *ndi_ptz_create(obs_source_t *source)
{
	auto p = new ndi_ptz_t();
	p->source = source;
	p->present.fill(false);
	p->pending = false;
	return p;
}

void ndi_ptz_destroy(ndi_ptz_t *p)
{
	delete p;
}

static void set_locked(ndi_ptz_t *p, const NdiPtzCmd &cmd)
{
	auto i = (size_t)cmd.kind;
	p->slot[i] = cmd;
	p->present[i] = true;
}

void ndi_ptz_post(ndi_ptz_t *p, const NdiPtzCmd &cmd)
{
	std::lock_guard<std::mutex> guard(p->lock);
	if (cmd.kind == NdiPtzCmdKind::Stop) {
		set_locked(p, {NdiPtzCmdKind::PanTiltSpeed});
		set_locked(p, {NdiPtzCmdKind::ZoomSpeed});
		set_locked(p, {NdiPtzCmdKind::FocusSpeed});
	} else {
		set_locked(p, cmd);
	}
	p->pending.store(true, std::memory_order_release);
}

bool ndi_ptz_pending(const ndi_ptz_t *p)
{
	return p->pending.load(std::memory_order_acquire);
}

void ndi_ptz_clear(ndi_ptz_t *p)
{
	std::lock_guard<std::mutex> guard(p->lock);
	p->present.fill(false);
	p->pending.store(false, std::memory_order_release);
}

int ndi_ptz_drain(ndi_ptz_t *p, NDIlib_recv_instance_t recv)
{
	std::array<NdiPtzCmd, (size_t)NdiPtzCmdKind::Count> cmds;
	std::array<bool, (size_t)NdiPtzCmdKind::Count> present;
	{
		std::lock_guard<std::mutex> guard(p->lock);
		cmds = p->slot;
		present = p->present;
		p->present.fill(false);
		p->pending.store(false, std::memory_order_release);
	}

	int sent = 0;
	for (size_t i = 0; i < cmds.size(); i++) {
		if (!present[i])
			continue;
		const NdiPtzCmd &c = cmds[i];
		switch (c.kind) {
		case NdiPtzCmdKind::PanTiltSpeed:
			ndiLib->recv_ptz_pan_tilt_speed(recv, c.a, c.b);
			break;
		case NdiPtzCmdKind::PanTiltAbs:
			ndiLib->recv_ptz_pan_tilt(recv, c.a, c.b);
			break;
		case NdiPtzCmdKind::ZoomSpeed:
			ndiLib->recv_ptz_zoom_speed(recv, c.a);
			break;
		case NdiPtzCmdKind::ZoomAbs:
			ndiLib->recv_ptz_zoom(recv, c.a);
			break;
		case NdiPtzCmdKind::FocusSpeed:
			ndiLib->recv_ptz_focus_speed(recv, c.a);
			break;
		case NdiPtzCmdKind::FocusAbs:
			ndiLib->recv_ptz_focus(recv, c.a);
			break;
		case NdiPtzCmdKind::AutoFocus:
			ndiLib->recv_ptz_auto_focus(recv);
			break;
		case NdiPtzCmdKind::WbOnepush:
			ndiLib->recv_ptz_white_balance_oneshot(recv);
			break;
		case NdiPtzCmdKind::PresetStore:
			ndiLib->recv_ptz_store_preset(recv, c.slot);
			break;
		case NdiPtzCmdKind::PresetRecall:
			ndiLib->recv_ptz_recall_preset(recv, c.slot, 1.0f);
			break;
		default:
			continue;
		}
		sent++;
	}
	return sent;
}
