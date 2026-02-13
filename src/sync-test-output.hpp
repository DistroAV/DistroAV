/******************************************************************************
	Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>
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

#include <obs-module.h>

#define SYNC_TEST_OUTPUT_ID "distroav_sync_test_output"

struct st_qr_data {
	uint32_t f = 0;
	uint32_t c = 0;
	uint32_t q_ms = 0;
	uint32_t index = -1;
	uint32_t index_max = 256;
	uint32_t type_flags = 0;
	bool valid = 0;

	bool _decode_kv(char *param)
	{
		char *saveptr;
		char *key = strtok_r(param, "=", &saveptr);
		if (!key || key[1] != 0)
			return false;

		char *val = strtok_r(NULL, "=", &saveptr);
		if (!val)
			return false;

		switch (key[0]) {
		case 'f':
			f = (uint32_t)atoi(val);
			return true;
		case 'c':
			c = (uint32_t)atoi(val);
			return true;
		case 'q':
			q_ms = (uint32_t)atoi(val);
			return true;
		case 'i':
			index = (uint32_t)atoi(val);
			return true;
		case 'I':
			index_max = (uint32_t)atoi(val);
			return true;
		case 't':
			type_flags = (uint32_t)atoi(val);
			return true;
		default:
			/* Ignored */
			return true;
		}

		return false;
	}

	bool check()
	{
		if (f < 10 || 32000 < f) {
			blog(LOG_WARNING, "f: out of range: %u", f);
			return false;
		}
		if (c < 1 || f < c) {
			blog(LOG_WARNING, "c: out of range: %u", c);
			return false;
		}
		if (q_ms < 1 || 1000 < q_ms) {
			blog(LOG_WARNING, "q: out of range: %u", q_ms);
			return false;
		}
		if (index & ~0xFF) {
			blog(LOG_WARNING, "i: out of range: %u", index);
			return false;
		}
		return true;
	}

	bool decode(char *payload)
	{
		valid = false;
		char *saveptr;
		char *param = strtok_r(payload, ",", &saveptr);
		while (param) {
			if (!_decode_kv(param))
				return false;
			param = strtok_r(NULL, ",", &saveptr);
		}
		if (!check())
			return false;
		valid = true;
		return true;
	}
};

struct video_marker_found_s {
	uint64_t timestamp;
	float score;
	struct st_qr_data qr_data;
};

struct audio_marker_found_s {
	uint64_t timestamp;
	int index;
	float score;
	uint32_t index_max;
};

struct sync_index {
	int index = -1;
	uint64_t video_ts = 0;
	uint64_t audio_ts = 0;
	uint32_t index_max = 256;
	bool matched = false;
};

struct frame_drop_event_s {
	uint64_t timestamp;
	int expected_index;
	int received_index;
	int dropped_count;
	uint64_t total_received;
	uint64_t total_dropped;
};

extern "C" void register_sync_test_output();
