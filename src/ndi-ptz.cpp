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

#include <obs.hpp>

#include <QCoreApplication>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#define NDI_PTZ_PRESET_SLOTS 100
#define NDI_PTZ_RECALL_ARRIVED_MS 3000

struct ndi_preset_t {
	int slot;
	std::string name;
	std::string thumbnail;
};

struct ndi_ptz_t {
	obs_source_t *source;

	// The command slot: any thread posts, the source thread drains
	std::mutex lock;
	std::array<NdiPtzCmd, (size_t)NdiPtzCmdKind::Count> slot;
	std::array<bool, (size_t)NdiPtzCmdKind::Count> present;
	std::atomic<bool> pending;

	// The receiver can do PTZ
	std::atomic<bool> connected{false};

	// The source's PTZ is switched on, by its "ndi_ptz" setting
	std::atomic<bool> enabled{false};
	// This device has said ptz_device_add and not ptz_device_remove; under live_lock
	bool announced = false;


	// Settings: speed caps and inversion for pan, tilt, zoom and focus
	std::atomic<float> speed_max[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	std::atomic<bool> invert[4] = {false, false, false, false};

	// Held while the presets change and while a signal is sent, so that the signals go one at a
	// time, in the order the changes happened. A listener may call back in from its own thread
	std::recursive_mutex api_lock;
	// In display order; under api_lock
	std::vector<ndi_preset_t> presets;
	bool presets_loaded = false;

	// A move or a newer recall ends the wait for the last recall to arrive
	std::atomic<unsigned> recall_generation{0};
	std::atomic<bool> af_enabled{false};
	std::atomic<bool> af_known{false};

	// The speed each axis was last asked to move at, in the API's terms, for a move that gives
	// one axis alone
	std::mutex move_lock;
	float speed_now[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	// The state as last announced, for ptz_get_state from any thread
	std::mutex state_lock;
	obs_data_t *state = nullptr;
	unsigned request_counter = 0;
};

static std::mutex live_lock;
static std::vector<ndi_ptz_t *> live;

static void rebuild_state(ndi_ptz_t *p);
static void announce_presence(ndi_ptz_t *p);
static void queue_announce(ndi_ptz_t *p);

ndi_ptz_t *ndi_ptz_create(obs_source_t *source)
{
	auto p = new ndi_ptz_t();
	p->source = source;
	p->present.fill(false);
	p->pending = false;
	ndi_ptz_register(p);
	{
		std::lock_guard<std::mutex> guard(live_lock);
		live.push_back(p);
	}
	rebuild_state(p);
	queue_announce(p);
	return p;
}

void ndi_ptz_destroy(ndi_ptz_t *p)
{
	{
		std::lock_guard<std::mutex> guard(live_lock);
		live.erase(std::remove(live.begin(), live.end(), p), live.end());
		// A device that is gone says so; OBS's own "destroy" is not a word it hears
		p->enabled = false;
		announce_presence(p);
	}
	if (p->state)
		obs_data_release(p->state);
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
	bool any = false;
	for (size_t i = 0; i < p->present.size(); i++) {
		const NdiPtzCmd &c = p->slot[i];
		bool is_stop = (c.kind == NdiPtzCmdKind::PanTiltSpeed || c.kind == NdiPtzCmdKind::ZoomSpeed ||
				c.kind == NdiPtzCmdKind::FocusSpeed) &&
			       c.a == 0.0f && c.b == 0.0f;
		// A move is not worth sending to a camera that is back later, but a stop is
		p->present[i] = p->present[i] && is_stop;
		any = any || p->present[i];
	}
	p->pending.store(any, std::memory_order_release);
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

//
// The PTZ Device API
//

// The device is off while its PTZ is switched off: it answers no version, its other procs do
// nothing and its signals say nothing, as the API has it of a device that turns its PTZ off
static bool off(const ndi_ptz_t *p)
{
	return !p->enabled.load(std::memory_order_relaxed);
}

static bool device_active(const ndi_ptz_t *p)
{
	return p->connected.load(std::memory_order_relaxed) && !off(p);
}

static void fill_features(obs_data_t *features, bool active)
{
	if (!active)
		return;
	for (const char *name : {"pan", "tilt", "zoom", "focus", "pan_abs", "tilt_abs", "zoom_abs", "focus_abs",
				 "autofocus", "wb_onepush", "presets"})
		obs_data_set_bool(features, name, true);
}

static void emit_signal(ndi_ptz_t *p, const char *name, const std::function<void(calldata_t *)> &fill)
{
	if (off(p))
		return;
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	calldata_t cd = {};
	calldata_set_ptr(&cd, "source", p->source);
	if (fill)
		fill(&cd);
	signal_handler_signal(obs_source_get_signal_handler(p->source), name, &cd);
	calldata_free(&cd);
}

// ptz_device_add and ptz_device_remove are on OBS's global handler, where a controller declares
// them; with none loaded they are not there, and a signal sent to them is not heard
static void emit_global(ndi_ptz_t *p, const char *name)
{
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	obs_source_t *source = p->source;
	calldata_t cd = {};
	calldata_set_ptr(&cd, "source", source);
	signal_handler_signal(obs_get_signal_handler(), name, &cd);
	calldata_free(&cd);
}

static void rebuild_state(ndi_ptz_t *p)
{
	obs_data_t *state = obs_data_create();
	bool connected = p->connected.load();
	obs_data_set_bool(state, "connected", connected);
	obs_data_set_string(state, "source", obs_source_get_name(p->source));
	OBSDataAutoRelease features = obs_data_create();
	fill_features(features, connected);
	obs_data_set_obj(state, "features", features);
	if (p->af_known.load())
		obs_data_set_bool(state, "focus_af_enabled", p->af_enabled.load());

	std::lock_guard<std::mutex> guard(p->state_lock);
	if (p->state)
		obs_data_release(p->state);
	p->state = state;
}

// The copy ptz_get_state gives is made before the signal, so it is never older than one
static void announce(ndi_ptz_t *p, obs_data_t *changed)
{
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	rebuild_state(p);
	emit_signal(p, "ptz_state_changed", [&](calldata_t *cd) { calldata_set_ptr(cd, "changed", changed); });
}

void ndi_ptz_set_connected(ndi_ptz_t *p, bool supported)
{
	if (p->connected.exchange(supported) == supported)
		return;
	// Under the live lock, so that the source is not destroyed while it is said
	std::lock_guard<std::mutex> guard(live_lock);
	if (std::find(live.begin(), live.end(), p) == live.end())
		return;
	bool connected = p->connected.load();
	OBSDataAutoRelease changed = obs_data_create();
	OBSDataAutoRelease features = obs_data_create();
	fill_features(features, connected);
	obs_data_set_obj(changed, "features", features);
	obs_data_set_bool(changed, "connected", connected);
	announce(p, changed);
}

void ndi_ptz_source_renamed(ndi_ptz_t *p)
{
	std::lock_guard<std::mutex> guard(live_lock);
	if (std::find(live.begin(), live.end(), p) == live.end())
		return;
	OBSDataAutoRelease changed = obs_data_create();
	obs_data_set_string(changed, "source", obs_source_get_name(p->source));
	announce(p, changed);
}

// A device says it is there when it is on, and that it is gone when it is not: OBS can take no proc
// back
static void announce_presence(ndi_ptz_t *p)
{
	bool present = !off(p);
	if (present == p->announced)
		return;
	p->announced = present;
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	if (present) {
		rebuild_state(p);
		emit_global(p, "ptz_device_add");
	} else {
		emit_global(p, "ptz_device_remove");
	}
}

static void queue_announce(ndi_ptz_t *p)
{
	// Left until the signal or the call that caused it is let go, which can be with OBS's locks
	// held, and until the source is made, so that it is ready to be asked
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[p]() {
			// Held while it is said: the source is destroyed on another thread, and
			// ndi_ptz_destroy() waits here before it deletes p
			std::lock_guard<std::mutex> guard(live_lock);
			if (std::find(live.begin(), live.end(), p) == live.end())
				return;
			announce_presence(p);
		},
		Qt::QueuedConnection);
}

static float clamp_to(double v, double lo, double hi)
{
	return (float)std::min(hi, std::max(lo, v));
}

enum { AXIS_PAN, AXIS_TILT, AXIS_ZOOM, AXIS_FOCUS };

// A speed asked for, as sent: capped, then reversed if the axis is
static float speed(const ndi_ptz_t *p, double v, int axis)
{
	float limit = p->speed_max[axis].load();
	float s = clamp_to(v, -(double)limit, (double)limit);
	// The API's speed is the way the position goes. NDI's pan speed is -1.0 for right, and
	// its focus speed is -1.0 for further, which is the way the position goes down
	if (axis == AXIS_PAN || axis == AXIS_FOCUS)
		s = -s;
	return p->invert[axis].load() ? -s : s;
}

static bool get_float(calldata_t *cd, const char *key, double *v)
{
	return calldata_get_float(cd, key, v);
}

static void proc_stop(void *data, calldata_t *)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	p->recall_generation++;
	{
		std::lock_guard<std::mutex> guard(p->move_lock);
		for (float &s : p->speed_now)
			s = 0.0f;
	}
	// Even with no PTZ yet: a stop is kept for a camera that is back (see ndi_ptz_clear())
	ndi_ptz_post(p, {NdiPtzCmdKind::Stop});
}

static void proc_move(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p) || !device_active(p))
		return;
	static const char *const keys[4] = {"pan", "tilt", "zoom", "focus"};
	std::lock_guard<std::mutex> guard(p->move_lock);
	bool given[4];
	bool moving = false;
	for (int i = 0; i < 4; i++) {
		double v;
		given[i] = get_float(cd, keys[i], &v);
		if (given[i]) {
			p->speed_now[i] = clamp_to(v, -1.0, 1.0);
			moving = moving || p->speed_now[i] != 0.0f;
		}
	}
	if (moving)
		p->recall_generation++;
	// NDI's one command for pan and tilt has both, so an axis left out is sent as it was
	if (given[AXIS_PAN] || given[AXIS_TILT])
		ndi_ptz_post(p, {NdiPtzCmdKind::PanTiltSpeed, speed(p, p->speed_now[AXIS_PAN], AXIS_PAN),
				 speed(p, p->speed_now[AXIS_TILT], AXIS_TILT)});
	if (given[AXIS_ZOOM])
		ndi_ptz_post(p, {NdiPtzCmdKind::ZoomSpeed, speed(p, p->speed_now[AXIS_ZOOM], AXIS_ZOOM)});
	if (given[AXIS_FOCUS])
		ndi_ptz_post(p, {NdiPtzCmdKind::FocusSpeed, speed(p, p->speed_now[AXIS_FOCUS], AXIS_FOCUS)});
}

static void proc_move_abs(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p) || !device_active(p))
		return;
	p->recall_generation++;
	double pan = 0.0, tilt = 0.0, zoom, focus;
	bool has_pan = get_float(cd, "pan", &pan);
	bool has_tilt = get_float(cd, "tilt", &tilt);
	if (has_pan && has_tilt)
		ndi_ptz_post(p, {NdiPtzCmdKind::PanTiltAbs, clamp_to(pan, -1.0, 1.0), clamp_to(tilt, -1.0, 1.0)});
	else if (has_pan || has_tilt)
		// The other axis stays where it is, which the API has a device take from its state,
		// and NDI says nothing of where the camera is: neither moves
		obs_log(LOG_DEBUG, "ptz_move_abs: one of pan and tilt alone, with no position known for the other");
	// NDI's zoom 0.0 is zoomed in where the API's is wide, and its focus 0.0 is infinity
	// where the API's is near
	if (get_float(cd, "zoom", &zoom))
		ndi_ptz_post(p, {NdiPtzCmdKind::ZoomAbs, 1.0f - clamp_to(zoom, 0.0, 1.0)});
	if (get_float(cd, "focus", &focus))
		ndi_ptz_post(p, {NdiPtzCmdKind::FocusAbs, 1.0f - clamp_to(focus, 0.0, 1.0)});
}

static void proc_trigger(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	const char *name = calldata_string(cd, "name");
	if (name && strcmp(name, "wb_onepush") == 0 && device_active(p))
		ndi_ptz_post(p, {NdiPtzCmdKind::WbOnepush});
	else
		obs_log(LOG_DEBUG, "ptz_trigger '%s' is not one this device does", name ? name : "");
}

static void proc_get_api_version(void *data, calldata_t *cd)
{
	// A device that is off gives no version a caller can use
	if (off(static_cast<ndi_ptz_t *>(data)))
		return;
	calldata_set_int(cd, "major", NDI_PTZ_API_MAJOR);
	calldata_set_int(cd, "minor", NDI_PTZ_API_MINOR);
}

static void proc_get_state(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	auto state = static_cast<obs_data_t *>(calldata_ptr(cd, "state"));
	if (!state)
		return;
	// From any thread: the state as last announced
	std::lock_guard<std::mutex> guard(p->state_lock);
	if (p->state)
		obs_data_apply(state, p->state);
}

static void proc_get_statistics(void *, calldata_t *)
{
	// NDI reports none
}

static void proc_get_parent_source(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	calldata_set_ptr(cd, "return", obs_source_get_ref(p->source));
}

static void proc_request_state(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	auto state = static_cast<obs_data_t *>(calldata_ptr(cd, "state"));
	if (!state || !obs_data_has_user_value(state, "focus_af_enabled") || !device_active(p))
		return;
	bool on = obs_data_get_bool(state, "focus_af_enabled");
	// NDI can only turn autofocus on: a manual focus command turns it off
	if (on)
		ndi_ptz_post(p, {NdiPtzCmdKind::AutoFocus});
	p->af_enabled = on;
	p->af_known = true;
	OBSDataAutoRelease changed = obs_data_create();
	obs_data_set_bool(changed, "focus_af_enabled", on);
	announce(p, changed);
}

static void proc_get_camera_report(void *data, calldata_t *cd)
{
	if (off(static_cast<ndi_ptz_t *>(data)))
		return;
	calldata_set_string(cd, "report", "");
}

//
// Presets, all of the camera store: the slots NDI keeps, numbered from 0
//

static std::string preset_id(int slot)
{
	return "camera:" + std::to_string(slot);
}

// -1 for an id that is not of this device's store
static int preset_slot(const char *id)
{
	if (!id || strncmp(id, "camera:", 7) != 0)
		return -1;
	char *end = nullptr;
	long n = strtol(id + 7, &end, 10);
	if (!end || *end || n < 0 || n >= NDI_PTZ_PRESET_SLOTS)
		return -1;
	return (int)n;
}

static ndi_preset_t *find_preset(ndi_ptz_t *p, int slot)
{
	for (auto &pr : p->presets)
		if (pr.slot == slot)
			return &pr;
	return nullptr;
}

// The presets the source was saved with, read once, the first time they are wanted, with api_lock
// held: update() can come from other threads, and from the graphics thread for a video source
static void ensure_presets_loaded(ndi_ptz_t *p)
{
	if (p->presets_loaded)
		return;
	p->presets_loaded = true;
	OBSDataAutoRelease settings = obs_source_get_settings(p->source);
	OBSDataArrayAutoRelease saved = obs_data_get_array(settings, "presets");
	if (!saved) {
		p->presets.clear();
		return;
	}
	std::vector<ndi_preset_t> presets;
	size_t n = obs_data_array_count(saved);
	for (size_t i = 0; i < n; i++) {
		OBSDataAutoRelease item = obs_data_array_item(saved, i);
		int slot = preset_slot(obs_data_get_string(item, "id"));
		if (slot < 0)
			continue;
		presets.push_back({slot, obs_data_get_string(item, "name"), obs_data_get_string(item, "thumbnail")});
	}
	OBSDataArrayAutoRelease order = obs_data_get_array(settings, "preset_order");
	if (order) {
		std::vector<ndi_preset_t> ordered;
		size_t m = obs_data_array_count(order);
		for (size_t i = 0; i < m; i++) {
			OBSDataAutoRelease item = obs_data_array_item(order, i);
			int slot = preset_slot(obs_data_get_string(item, "id"));
			auto it = std::find_if(presets.begin(), presets.end(),
					       [slot](auto &pr) { return pr.slot == slot; });
			if (it != presets.end()) {
				ordered.push_back(*it);
				presets.erase(it);
			}
		}
		ordered.insert(ordered.end(), presets.begin(), presets.end());
		presets = std::move(ordered);
	}
	p->presets = std::move(presets);
}

static void persist_presets(ndi_ptz_t *p)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataArrayAutoRelease presets = obs_data_array_create();
	OBSDataArrayAutoRelease order = obs_data_array_create();
	for (auto &pr : p->presets) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "id", preset_id(pr.slot).c_str());
		obs_data_set_string(item, "name", pr.name.c_str());
		obs_data_set_string(item, "thumbnail", pr.thumbnail.c_str());
		obs_data_array_push_back(presets, item);
		OBSDataAutoRelease ordered = obs_data_create();
		obs_data_set_string(ordered, "id", preset_id(pr.slot).c_str());
		obs_data_array_push_back(order, ordered);
	}
	obs_data_set_array(settings, "presets", presets);
	obs_data_set_array(settings, "preset_order", order);
	obs_source_update(p->source, settings);
}

static void proc_preset_get_list(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	ensure_presets_loaded(p);
	obs_data_t *list = obs_data_create();
	OBSDataAutoRelease presets = obs_data_create();
	OBSDataArrayAutoRelease order = obs_data_array_create();
	for (auto &pr : p->presets) {
		std::string id = preset_id(pr.slot);
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "id", id.c_str());
		obs_data_set_string(item, "store", "camera");
		obs_data_set_string(item, "name", pr.name.c_str());
		obs_data_set_string(item, "camera_name", "");
		obs_data_set_string(item, "thumbnail", pr.thumbnail.c_str());
		obs_data_set_obj(presets, id.c_str(), item);
		OBSDataAutoRelease ordered = obs_data_create();
		obs_data_set_string(ordered, "id", id.c_str());
		obs_data_array_push_back(order, ordered);
	}
	obs_data_set_obj(list, "presets", presets);
	obs_data_set_array(list, "order", order);
	OBSDataAutoRelease stores = obs_data_create();
	obs_data_set_bool(stores, "camera", true);
	obs_data_set_obj(list, "stores", stores);
	obs_data_set_bool(list, "names_on_camera", false);
	obs_data_set_bool(list, "enumerable", false);
	OBSDataAutoRelease value_keys = obs_data_create();
	obs_data_set_obj(list, "value_keys", value_keys);
	calldata_set_ptr(cd, "return", list);
}

static void proc_preset_save(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	int slot = preset_slot(calldata_string(cd, "id"));
	if (slot < 0 || !device_active(p))
		return;
	ndi_ptz_post(p, {NdiPtzCmdKind::PresetStore, 0.0f, 0.0f, slot});
}

static void proc_preset_recall(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	int slot = preset_slot(calldata_string(cd, "id"));
	if (slot < 0 || !device_active(p))
		return;
	ndi_ptz_post(p, {NdiPtzCmdKind::PresetRecall, 0.0f, 0.0f, slot});

	// NDI says nothing of the camera arriving, so, as the API says of a device without
	// move_completion, it is taken to have after a fixed time, unless a newer recall or a
	// move came first
	unsigned generation = ++p->recall_generation;
	std::string id = preset_id(slot);
	QTimer::singleShot(NDI_PTZ_RECALL_ARRIVED_MS, QCoreApplication::instance(), [p, generation, id]() {
		std::lock_guard<std::mutex> guard(live_lock);
		if (std::find(live.begin(), live.end(), p) == live.end() || p->recall_generation.load() != generation)
			return;
		emit_signal(p, "ptz_preset_recalled", [&](calldata_t *c) { calldata_set_string(c, "id", id.c_str()); });
	});
}

static void proc_preset_capture_thumbnail(void *data, calldata_t *)
{
	// Thumbnails are not made by this device: its presets have none to replace
	(void)data;
	obs_log(LOG_DEBUG, "ptz_preset_capture_thumbnail: this device makes no thumbnails");
}

static void proc_preset_get_thumbnail(void *data, calldata_t *cd)
{
	if (off(static_cast<ndi_ptz_t *>(data)))
		return;
	// Its presets have none
	calldata_set_string(cd, "mime", "");
	calldata_set_string(cd, "data", "");
}

static void proc_preset_create(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	calldata_set_string(cd, "return", "");
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	ensure_presets_loaded(p);
	const char *store = calldata_string(cd, "store");
	if (!store || strcmp(store, "camera") != 0 || !device_active(p))
		return;
	int slot = 0;
	while (slot < NDI_PTZ_PRESET_SLOTS && find_preset(p, slot))
		slot++;
	if (slot == NDI_PTZ_PRESET_SLOTS)
		return;

	const char *name = calldata_string(cd, "name");
	p->presets.push_back({slot, name ? name : "", ""});
	ndi_ptz_post(p, {NdiPtzCmdKind::PresetStore, 0.0f, 0.0f, slot});
	persist_presets(p);

	std::string id = preset_id(slot);
	std::string request = "ndi-" + std::to_string(++p->request_counter);
	emit_signal(p, "ptz_preset_added", [&](calldata_t *c) { calldata_set_string(c, "id", id.c_str()); });
	// The request is over after this has returned
	QTimer::singleShot(0, QCoreApplication::instance(), [p, request, id]() {
		std::lock_guard<std::mutex> guard(live_lock);
		if (std::find(live.begin(), live.end(), p) == live.end())
			return;
		emit_signal(p, "ptz_preset_create_done", [&](calldata_t *c) {
			calldata_set_string(c, "request", request.c_str());
			calldata_set_string(c, "id", id.c_str());
		});
	});
	calldata_set_string(cd, "return", request.c_str());
}

static void proc_preset_delete(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	ensure_presets_loaded(p);
	const char *id = calldata_string(cd, "id");
	int slot = preset_slot(id);
	auto it = std::find_if(p->presets.begin(), p->presets.end(), [slot](auto &pr) { return pr.slot == slot; });
	if (it == p->presets.end())
		return;
	p->presets.erase(it);
	persist_presets(p);
	std::string removed = id;
	emit_signal(p, "ptz_preset_removed", [&](calldata_t *c) { calldata_set_string(c, "id", removed.c_str()); });
}

static void proc_preset_update(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	ensure_presets_loaded(p);
	const char *id = calldata_string(cd, "id");
	auto changes = static_cast<obs_data_t *>(calldata_ptr(cd, "changes"));
	ndi_preset_t *pr = find_preset(p, preset_slot(id));
	if (!pr || !changes)
		return;
	OBSDataAutoRelease changed = obs_data_create();
	if (obs_data_has_user_value(changes, "name")) {
		pr->name = obs_data_get_string(changes, "name");
		obs_data_set_string(changed, "name", pr->name.c_str());
	}
	if (obs_data_has_user_value(changes, "thumbnail")) {
		pr->thumbnail = obs_data_get_string(changes, "thumbnail");
		obs_data_set_string(changed, "thumbnail", pr->thumbnail.c_str());
	}
	persist_presets(p);
	std::string changed_id = id;
	emit_signal(p, "ptz_preset_changed", [&](calldata_t *c) {
		calldata_set_string(c, "id", changed_id.c_str());
		calldata_set_ptr(c, "changed", changed);
	});
}

static void proc_preset_move(void *data, calldata_t *cd)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	std::lock_guard<std::recursive_mutex> guard(p->api_lock);
	ensure_presets_loaded(p);
	int slot = preset_slot(calldata_string(cd, "id"));
	auto it = std::find_if(p->presets.begin(), p->presets.end(), [slot](auto &pr) { return pr.slot == slot; });
	if (it == p->presets.end())
		return;
	ndi_preset_t moved = *it;
	p->presets.erase(it);
	long long index = calldata_int(cd, "index");
	index = std::min<long long>(std::max<long long>(index, 0), (long long)p->presets.size());
	p->presets.insert(p->presets.begin() + index, moved);
	persist_presets(p);
	emit_signal(p, "ptz_preset_order_changed", nullptr);
}

static void proc_preset_refresh(void *data, calldata_t *)
{
	auto p = static_cast<ndi_ptz_t *>(data);
	if (off(p))
		return;
	emit_signal(p, "ptz_preset_list_reset", nullptr);
}

struct proc_def_t {
	const char *decl;
	proc_handler_proc_t fn;
	const char *thread; // every proc is "any": the device does its own locking
	const char *access; // "read", "control" or "none"
};

static const std::vector<proc_def_t> &proc_defs();

static const char *const signal_decls[] = {
	"void ptz_state_changed(ptr source, ptr changed)",
	"void ptz_preset_added(ptr source, string id)",
	"void ptz_preset_removed(ptr source, string id)",
	"void ptz_preset_order_changed(ptr source)",
	"void ptz_preset_changed(ptr source, string id, ptr changed)",
	"void ptz_preset_recalled(ptr source, string id)",
	"void ptz_preset_list_reset(ptr source)",
	"void ptz_preset_create_done(ptr source, string request, string id)",
};

// "void ptz_preset_recall(string id)" is named "ptz_preset_recall"
static std::string decl_name(const std::string &decl)
{
	size_t start = decl.find("ptz_");
	size_t end = decl.find('(');
	return decl.substr(start, end - start);
}

static void proc_get_api_info(void *data, calldata_t *cd)
{
	if (off(static_cast<ndi_ptz_t *>(data)))
		return;
	obs_data_t *info = obs_data_create();
	OBSDataArrayAutoRelease procs = obs_data_array_create();
	for (auto &def : proc_defs()) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "name", decl_name(def.decl).c_str());
		obs_data_set_string(item, "decl", def.decl);
		obs_data_set_string(item, "thread", def.thread);
		obs_data_set_string(item, "access", def.access);
		obs_data_array_push_back(procs, item);
	}
	obs_data_set_array(info, "procs", procs);
	OBSDataArrayAutoRelease sigs = obs_data_array_create();
	for (const char *decl : signal_decls) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "name", decl_name(decl).c_str());
		obs_data_set_string(item, "decl", decl);
		obs_data_array_push_back(sigs, item);
	}
	obs_data_set_array(info, "signals", sigs);
	calldata_set_ptr(cd, "return", info);
}

// What a device has: it has no home, relative moves or thumbnails to make, and so no procs for them
static const std::vector<proc_def_t> &proc_defs()
{
	static const std::vector<proc_def_t> defs = {
		{"void ptz_get_api_version(out int major, out int minor)", proc_get_api_version, "any", "read"},
		{"ptr ptz_get_api_info()", proc_get_api_info, "any", "read"},
		{"void ptz_stop()", proc_stop, "any", "control"},
		{"void ptz_move(float pan, float tilt, float zoom, float focus)", proc_move, "any", "control"},
		{"void ptz_move_abs(float pan, float tilt, float zoom, float focus)", proc_move_abs, "any", "control"},
		{"ptr ptz_get_state(ptr state)", proc_get_state, "any", "read"},
		{"ptr ptz_get_statistics(ptr statistics)", proc_get_statistics, "any", "read"},
		{"ptr ptz_get_parent_source()", proc_get_parent_source, "any", "none"},
		{"void ptz_request_state(ptr state)", proc_request_state, "any", "control"},
		{"void ptz_trigger(string name)", proc_trigger, "any", "control"},
		{"void ptz_get_camera_report(out string report)", proc_get_camera_report, "any", "read"},
		{"ptr ptz_preset_get_list()", proc_preset_get_list, "any", "read"},
		{"void ptz_preset_save(string id)", proc_preset_save, "any", "control"},
		{"void ptz_preset_recall(string id)", proc_preset_recall, "any", "control"},
		{"void ptz_preset_get_thumbnail(string id, out string mime, out string data)",
		 proc_preset_get_thumbnail, "any", "read"},
		{"void ptz_preset_capture_thumbnail(string id, bool if_arrived)", proc_preset_capture_thumbnail, "any",
		 "control"},
		{"string ptz_preset_create(string name, string store)", proc_preset_create, "any", "control"},
		{"void ptz_preset_delete(string id)", proc_preset_delete, "any", "control"},
		{"void ptz_preset_update(string id, ptr changes)", proc_preset_update, "any", "control"},
		{"void ptz_preset_move(string id, int index)", proc_preset_move, "any", "control"},
		{"void ptz_preset_refresh()", proc_preset_refresh, "any", "control"},
	};
	return defs;
}

void ndi_ptz_register(ndi_ptz_t *p)
{
	proc_handler_t *ph = obs_source_get_proc_handler(p->source);
	signal_handler_t *sh = obs_source_get_signal_handler(p->source);
	if (!ph || !sh) {
		obs_log(LOG_ERROR, "could not register the PTZ device API: no proc or signal handler");
		return;
	}

	for (auto &def : proc_defs()) {
		proc_handler_add(ph, def.decl, def.fn, p);
		obs_log(LOG_DEBUG, "ptz device proc: %s", def.decl);
	}
	for (const char *decl : signal_decls) {
		signal_handler_add(sh, decl);
		obs_log(LOG_DEBUG, "ptz device signal: %s", decl);
	}
}

void ndi_ptz_get_defaults(obs_data_t *settings)
{
	for (const char *key : {"pan_speed_max", "tilt_speed_max", "zoom_speed_max", "focus_speed_max"})
		obs_data_set_default_double(settings, key, 1.0);
	for (const char *key : {"pan_invert", "tilt_invert", "zoom_invert", "focus_invert"})
		obs_data_set_default_bool(settings, key, false);
	obs_data_set_default_string(settings, "type", "ndi");
}

void ndi_ptz_update_settings(ndi_ptz_t *p, obs_data_t *settings)
{
	const char *caps[4] = {"pan_speed_max", "tilt_speed_max", "zoom_speed_max", "focus_speed_max"};
	for (int i = 0; i < 4; i++) {
		double cap = obs_data_get_double(settings, caps[i]);
		// The one cap of pan and tilt of an older version is both caps, when neither is set
		if ((i == AXIS_PAN || i == AXIS_TILT) && obs_data_has_user_value(settings, "pantilt_speed_max") &&
		    !obs_data_has_user_value(settings, caps[i]))
			cap = obs_data_get_double(settings, "pantilt_speed_max");
		p->speed_max[i] = clamp_to(cap, 0.1, 1.0);
	}
	const char *inverts[4] = {"pan_invert", "tilt_invert", "zoom_invert", "focus_invert"};
	for (int i = 0; i < 4; i++)
		p->invert[i] = obs_data_get_bool(settings, inverts[i]);

	bool enabled = obs_data_get_bool(settings, "ndi_ptz");
	if (p->enabled.exchange(enabled) != enabled) {
		// What was moving is stopped, so the camera does not drift
		if (!enabled)
			ndi_ptz_post(p, {NdiPtzCmdKind::Stop});
		queue_announce(p);
	}
}
