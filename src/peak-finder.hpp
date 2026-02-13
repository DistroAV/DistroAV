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

#include <inttypes.h>

struct peak_finder {
	uint64_t cand_ts = 0;
	uint64_t last_ts = 0;

	float cand_score = 0.0f;
	float last_score = 0.0f;

	uint64_t dumping_range = 2000000000;

	float dumping(uint64_t ts_last, uint64_t ts_next) const
	{
		if (ts_next <= ts_last)
			return 1.0f;
		if (ts_next - ts_last > dumping_range)
			return 0.0f;
		float f = (float)(ts_next - ts_last) / dumping_range;
		return 1.0f - f * f;
	}

	bool append(float score, uint64_t ts, uint64_t wait_ts)
	{
		if (score > cand_score * dumping(cand_ts, ts)) {
			cand_ts = ts;
			cand_score = score;
			/* When updating candidate, which is the recent peak,
			 * there might be larger score coming next. */
			return false;
		}

		if (cand_ts + wait_ts > ts)
			return false;

		if (cand_ts > last_ts && cand_score > last_score * dumping(last_ts, cand_ts)) {
			last_ts = cand_ts;
			last_score = cand_score;
			return true;
		}

		return false;
	}
};
