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
#pragma once
#include "obs-frontend-api.h"

/* obs-source-tally
* 
* Provides a way for sources to be notified when they are in the preview scene so that the
* on_preview tally can be set on the NDI source.
* The latest version of OBS (31.1.2) does not provide this functionality in sources, so this
* libary implements it. It is only intended for use by the ndi-source and mirrors 
* how OBS implements the activated/deactivated callbacks for sources (which ndi-source also uses
* for on_program).
* If OBS does implement this functionality in the future, this library can be removed and should only need
* changes to ndi-source.cpp to use the OBS provided functionality.
* 
* In addition to OBS not providing this functionality, there are some other issues with OBS 
* which require workarounds in this library:
* 1. When a source is in a group, the item_visible doesn't get fired when it is toggled by the user
* 2. Items moved to a new group need to connect to the new group's item_visible signal
* 3. When a group is toggled, the item_visible doesn't get fired for the items in the group
* 
*/

/**
 * Callback information for source preview tally
 */
struct obs_source_tally_info {
	/** Called when the source has been previewed */
	void (*preview)(void *data);

	/* Called when the source has been removed or hidden from preview */
	void (*depreview)(void *data);
};

EXPORT void obs_source_tally_register_source(struct obs_source_tally_info *info);
EXPORT void obs_source_tally_bind_data(const obs_source_t *source, void *data);
EXPORT bool obs_source_tally_preview(const obs_source_t *source);
EXPORT void obs_source_tally_source_destroy(const obs_source_t *source);
EXPORT void obs_source_tally_destroy();
