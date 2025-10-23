/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
	Copyright (C) 2024 DistroAV <contact@distroav.org>

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
******************************************************************************/
#include "obs-source-preview.h"
#include "plugin-main.h"
#include <map>
#include <set>

/* Called when the source has been previewed */
void (*_preview)(void *data);
/* Called when the source has been removed or hidden from preview */
void (*_depreview)(void *data);

typedef struct source_preview_info_s {
	void *data;
	uint preview = 0;
} source_preview_info_t;

// Signal handler for the current preview scene
signal_handler_t *_sh = nullptr;

// Map of all sources boud to the preview tally system.
// Key is the obs_source_t pointer, value is the preview tally count for that source
// and a pointer to the source data
typedef std::map<const obs_source_t *, source_preview_info_t *> source_preview_map_t;
static source_preview_map_t _source_preview_map = {};

// Keep track of group signal handlers in the current preview scene we have connected to
std::vector<signal_handler_t *> _group_signal_handlers = {};

// Forward declaration for use in on_scene_reorder.
// This is so a scene reorder signal can refresh the _source_preview_map
void source_preview_map_refresh();

// Increment or decrement the preview count based on the visibility of the scene items for
// that source. Only invoke the preview or depreview callback if changing preview
// visibility of the source.
void update_preview_count(source_preview_info_t *source_preview_info, bool visible)
{
	uint old_preview = source_preview_info->preview;
	if (visible)
		source_preview_info->preview++;
	else if (source_preview_info->preview > 0)
		source_preview_info->preview--;

	if ((old_preview == 1) && (source_preview_info->preview == 0))
		_depreview(source_preview_info->data);
	else if ((old_preview == 0) && (source_preview_info->preview == 1))
		_preview(source_preview_info->data);
}

// Enumeration callback for obs_scene_enum_items and obs_sceneitem_group_enum_items
// to set the initial preview count from a full scene or group.
bool set_preview_for_item(obs_scene_t * /* scene */, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_t *this_group = item;
		obs_sceneitem_group_enum_items(item, set_preview_for_item, this_group);
		return true;
	}

	obs_sceneitem_t *group = (obs_sceneitem_t *)param;
	const obs_source_t *source = obs_sceneitem_get_source(item);
	if (source) {
		std::string id = obs_source_get_id(source);

		bool visible = (group == nullptr) ? obs_sceneitem_visible(item)
						  : obs_sceneitem_visible(group) && obs_sceneitem_visible(item);
		auto it = _source_preview_map.find(source);
		if (it != _source_preview_map.end()) {
			obs_log(LOG_DEBUG, "'%s' set_preview_for_item (%d)", obs_source_get_name(source), visible);
			update_preview_count(it->second, visible);
		}
	}
	return true;
}

// Called when a scene item is toggled on/off in the current preview scene.
void on_sceneitem_visible(void * /* param */, calldata_t *data)
{
	obs_sceneitem_t *scene_item = (obs_sceneitem_t *)calldata_ptr(data, "item");

	if (obs_sceneitem_is_group(scene_item)) {
		obs_sceneitem_t *group = scene_item;
		obs_sceneitem_group_enum_items(scene_item, set_preview_for_item, group);
		return;
	}

	bool scene_item_visible = calldata_bool(data, "visible");
	const obs_source_t *source = obs_sceneitem_get_source(scene_item);
	auto it = _source_preview_map.find(source);
	if (it != _source_preview_map.end()) {
		obs_log(LOG_DEBUG, "'%s' on_sceneitem_visible(%d)", obs_source_get_name(source), scene_item_visible);
		update_preview_count(it->second, scene_item_visible);
	}
}

// Enumeration callback for obs_scene_enum_items to set the item_visible signal for groups.
// Needed because sources in groups don't emit visible signals
bool set_visible_signal_for_group(obs_scene_t * /* scene */, obs_sceneitem_t *item, void * /* param */)
{
	if (obs_sceneitem_is_group(item)) {
		auto group_scene = obs_sceneitem_group_get_scene(item);

		auto sh = obs_source_get_signal_handler(obs_scene_get_source(group_scene));

		// Connect to the scene item visibility for the group so we can track visibility changes
		signal_handler_disconnect(sh, "item_visible", on_sceneitem_visible, nullptr);
		signal_handler_connect(sh, "item_visible", on_sceneitem_visible, nullptr);
		_group_signal_handlers.push_back(sh);
	}
	return true;
}

// Called when a scene item is added in the current preview scene.
void on_sceneitem_add(void * /* param */, calldata_t *data)
{
	obs_sceneitem_t *scene_item = (obs_sceneitem_t *)calldata_ptr(data, "item");
	const obs_source_t *source = obs_sceneitem_get_source(scene_item);
	auto it = _source_preview_map.find(source);
	if (it != _source_preview_map.end()) {
		obs_log(LOG_DEBUG, "'%s' on_sceneitem_add", obs_source_get_name(source));
		bool scene_item_visible = obs_sceneitem_visible(scene_item);
		update_preview_count(it->second, scene_item_visible);
	}
}

// Called when the preview scene is reordered, specifically when a source item moves to a group.
void on_scene_reorder(void * /* param */, calldata_t * /* data */)
{
	source_preview_map_refresh();
}

// Disconnect all signals we created
void disconnect_signals()
{
	if (_sh) {
		signal_handler_disconnect(_sh, "item_visible", on_sceneitem_visible, nullptr);
		signal_handler_disconnect(_sh, "item_add", on_sceneitem_add, nullptr);
		signal_handler_disconnect(_sh, "reorder", on_scene_reorder, nullptr);
		_sh = nullptr;
	}
	for (auto sh : _group_signal_handlers) {
		signal_handler_disconnect(sh, "item_visible", on_sceneitem_visible, nullptr);
	}
	_group_signal_handlers.clear();
}

// Refresh the source_preview_map to represent the current state of the current preview scene.
void source_preview_map_refresh()
{
	disconnect_signals();

	// Reset all preview counts to zero
	for (auto &pair : _source_preview_map) {
		update_preview_count(pair.second, false);
	}

	if (!obs_frontend_preview_program_mode_active()) {
		return;
	}

	obs_source_t *preview_source = obs_frontend_get_current_preview_scene();

	auto preview_scene = obs_scene_from_source(preview_source);

	_sh = obs_source_get_signal_handler(preview_source);
	obs_source_release(preview_source);

	// Connect to the scene item visibility/add and reorder signals so we can track changes
	signal_handler_connect(_sh, "item_visible", on_sceneitem_visible, nullptr);
	obs_scene_enum_items(preview_scene, set_visible_signal_for_group,
			     nullptr); // needed because sources in groups don't emit visible signals
	signal_handler_connect(_sh, "item_add", on_sceneitem_add, nullptr);
	signal_handler_connect(_sh, "reorder", on_scene_reorder, nullptr);

	obs_scene_enum_items(preview_scene, set_preview_for_item, nullptr);

	obs_log(LOG_DEBUG, "source_preview_map_refresh: Currently %zu sources in scene collection",
		_source_preview_map.size());
}

void on_frontend_event(enum obs_frontend_event event, void * /* param */)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		source_preview_map_refresh();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
		disconnect_signals();
		break;
	default:
		break;
	}
}

// Called by the source to determine if it is in the preview scene.
bool obs_source_preview(const obs_source_t *source)
{
	auto it = _source_preview_map.find(source);
	if (it != _source_preview_map.end()) {
		return it->second->preview > 0;
	}
	return false;
}

// Called by the source to register the preview and depreview callbacks for the source.
// This is implemented similar to how the activated/deactivated callbacks on sources are registered.
void obs_source_preview_register_source(struct obs_source_preview_info *info)
{
	_preview = info->preview;
	_depreview = info->depreview;
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
}

// Create the source_preview_map entry for this source and bind the source context (data)
// to it. Only one map entry for each unique source.
void obs_source_preview_bind_data(const obs_source_t *source, void *data)
{
	auto it = _source_preview_map.find(source);
	if (it != _source_preview_map.end()) {
		it->second->data = data;
	} else {
		source_preview_info_t *info = (source_preview_info_t *)bzalloc(sizeof(source_preview_info_t));
		info->data = data;
		info->preview = 0;
		_source_preview_map[source] = info;

		obs_log(LOG_DEBUG, "'%s' obs_source_preview_bind_data New source", obs_source_get_name(source));
	}
}

// Called when the source is being destroyed to remove it from the source_preview_map.
void obs_source_preview_source_destroy(const obs_source_t *source)
{
	auto it = _source_preview_map.find(source);
	if (it != _source_preview_map.end()) {
		bfree(it->second);
		_source_preview_map.erase(it);
	}
}

// Cleanup the preview tally system, disconnecting any signals and clearing the source_preview_map.
void obs_source_preview_destroy()
{
	disconnect_signals();
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	_source_preview_map.clear();
}
