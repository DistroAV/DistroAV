/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/******************************************************************************
Original File: https://github.com/obsproject/obs-studio/blob/master/libobs/util/curl/curl-helper.h
******************************************************************************/

#pragma once

#include <curl/curl.h>

#if defined(_WIN32) && LIBCURL_VERSION_NUM >= 0x072c00

#ifdef CURLSSLOPT_REVOKE_BEST_EFFORT
#define CURL_OBS_REVOKE_SETTING CURLSSLOPT_REVOKE_BEST_EFFORT
#else
#define CURL_OBS_REVOKE_SETTING CURLSSLOPT_NO_REVOKE
#endif

#define curl_obs_set_revoke_setting(handle) curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, CURL_OBS_REVOKE_SETTING)

#else

#define curl_obs_set_revoke_setting(handle)

#endif
