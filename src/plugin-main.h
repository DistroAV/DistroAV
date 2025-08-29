/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

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

#include "config.h"
#include "obs-support/qt_wrapper.hpp"
#include "plugin-support.h"

#include "obs-support/obs-app.hpp"

#include <Processing.NDI.Lib.h>

#define PLUGIN_MIN_QT_VERSION "6.0.0"
#define PLUGIN_MIN_OBS_VERSION "31.0.0"
#define PLUGIN_MIN_NDI_VERSION "6.0.0"

#define OBS_NDI_ALPHA_FILTER_ID "premultiplied_alpha_filter"

extern const NDIlib_v6 *ndiLib;

/*
The following accomplishes two goals:
1. Enable the use of a local emulator at 127.0.0.1 for [non-production] testing Update
   [and future Analytics/Telemetry]:
	The urls will always "look" official to the user (appear to point to non-local official
	NDI/DistroAV/etc urls).
	If `PLUGIN_WEB_HOST` is defined as "127.0.0.1" then the urls will still look like they point
	to the official NDI or DistroAV urls, but they will actually point to the local firebase
	emulator/server.
	If `PLUGIN_WEB_HOST` is not defined as "127.0.0.1" then the urls will point to what they say
	they point to... sortof: See #2.
	`rehostUrl` rewrites the urls to point to a local firebase emulator/server... or not.
	`makeLink` will call `rehostUrl` to rewrite any url... or not.
2. Semi-future proof urls so that they are [semi-]consistent if NDI ever changes their urls...again.
	If NDI ever changes their urls, updating and deploying the
	https://github.com/DistroAV/firebase/blob/main/firebase.json hosting urls will 
	allow the plugin to redirect to the new urls without needing to update the plugin.
	There is always the possibility that the user may **see** a "out of date" url, but when they
	click on it the distroav.org server will redirect them to the latest url.
*/
#define PLUGIN_WEB_HOST_PRODUCTION "distroav.org"
#define PLUGIN_WEB_HOST_LOCAL_EMULATOR "127.0.0.1"

QString rehostUrl(const char *url);
QString makeLink(const char *url, const char *text = nullptr);

#define PLUGIN_UPDATE_URL "https://distroav.org/api/update"

#define PLUGIN_REDIRECT_DISCORD_URL "https://distroav.org/discord"
#define PLUGIN_REDIRECT_DONATE_URL "https://distroav.org/donate"
#define PLUGIN_REDIRECT_REPORT_BUG_URL "https://distroav.org/report-bug"
#define PLUGIN_REDIRECT_TROUBLESHOOTING_URL "https://distroav.org/kb/troubleshooting"
#define PLUGIN_REDIRECT_UNINSTALL_URL "https://distroav.org/kb/uninstall"
#define PLUGIN_REDIRECT_UNINSTALL_OBSNDI_URL "https://distroav.org/kb/uninstall-obs-ndi"
#if defined(Q_OS_WIN)
// Windows
#define PLUGIN_REDIRECT_NDI_REDIST_URL "https://distroav.org/ndi/redist-windows"
#elif defined(Q_OS_MACOS)
// MacOS
#define PLUGIN_REDIRECT_NDI_REDIST_URL "https://distroav.org/ndi/redist-macos"
#else
// Linux
#define PLUGIN_REDIRECT_NDI_REDIST_URL "https://distroav.org/ndi/redist-linux"
#endif
#define PLUGIN_REDIRECT_NDI_SDK_CPU_REQUIREMENTS_URL "https://distroav.org/ndi/sdk-cpu-requirements"
#define PLUGIN_REDIRECT_NDI_TOOLS_URL "https://distroav.org/ndi/tools"
#define PLUGIN_REDIRECT_NDI_WEB_URL "https://distroav.org/ndi/web"

//
// `NDI_OFFICIAL_*` urls that should always be displayed cosmetically/literally to the user.
// The code will truly point to the `PLUGIN_*` urls above.
// If NDI ever changes any of these urls:
// 	1. First update them in https://github.com/DistroAV/firebase/blob/main/firebase.json hosting;
//	  	Clients will then auto-magically point to the new urls without needing to update the plugin,
//		even if they see the old urls; no one should notice/care [that the links look old].
// 	2. Then update them here and then release a new version of the plugin.
//		Clients will then both see and point to the new urls.
//
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#define NDI_OFFICIAL_REDIST_URL NDILIB_REDIST_URL
#define NDI_OFFICIAL_TOOLS_URL "https://ndi.video/tools/download"
#endif
#define NDI_OFFICIAL_CPU_REQUIREMENTS_URL "https://docs.ndi.video/docs/sdk/cpu-requirements"
// Required by NDI license:
// Per https://github.com/DistroAV/DistroAV/blob/master/lib/ndi/NDI%20SDK%20Documentation.pdf
// "3 Licensing"
// "Your application must provide a link to https://ndi.video/ in a location close to all locations
// where NDI is used / selected within the product, on your web site, and in its documentation."
#define NDI_OFFICIAL_WEB_URL "https://ndi.video"
// "Your application’s About Box and any other locations where trademark attribution is provided
// should also specifically indicate that “NDI® is a registered trademark of Vizrt NDI AB”."
#define NDI_IS_A_REGISTERED_TRADEMARK_TEXT "NDI® is a registered trademark of Vizrt NDI AB"
