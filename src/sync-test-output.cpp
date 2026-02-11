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

#include <obs-module.h>
#include <util/platform.h>
#include <inttypes.h>
#include <deque>
#include <list>
#include <stdlib.h>
#include <algorithm>
#include <mutex>
#include <complex>
#include "quirc.h"
#include "sync-test-output.hpp"
#include "peak-finder.hpp"

#define N_CORNERS 4

#define N_AUDIO_SYMBOLS 16
#define N_SYMBOL_BUFFER 20

/* There are several reason to limit the width and the height.
 * - Since a square of 3/8 QR-code-length is calculated using uint32_t,
 *   the 3/8 of width or height cannot exceed the square root of uint32_t max.
 * - Since a sum of the pixels in a line is accumurated on uint32_t,
 *   the width must be less than 1/255 of uint32_t max.
 *   */
#define MAX_WIDTH_HEIGHT 87378u

struct st_audio_buffer
{
	std::deque<std::pair<int32_t, int32_t>> buffer;

	void push_back(int16_t xr, int16_t xi, size_t length)
	{
		int32_t vr = xr, vi = xi;
		if (buffer.size()) {
			vr += buffer.back().first;
			vi += buffer.back().second;
		}
		buffer.push_back(std::make_pair(vr, vi));

		if (buffer.size() <= length)
			return;

		buffer.pop_front();
	};

	std::pair<int32_t, int32_t> sum(size_t n_from_last)
	{
		if (buffer.size() <= 0)
			return std::make_pair(0, 0);
		if (n_from_last >= buffer.size())
			return buffer[0];
		return buffer[buffer.size() - n_from_last - 1];
	}
};

std::pair<int32_t, int32_t> operator-(std::pair<int32_t, int32_t> a, std::pair<int32_t, int32_t> b)
{
	return std::make_pair(a.first - b.first, a.second - b.second);
}

std::complex<float> int16_to_complex(std::pair<int32_t, int32_t> x)
{
	return std::complex<float>((float)x.first / 32768.0f, (float)x.second / 32768.0f);
}

struct corner_type
{
	uint32_t x, y;
	uint32_t r = 0;
};

struct sync_test_output
{
	obs_output_t *context;

	/* Configuration from OBS output context */
	uint32_t video_width = 0, video_height = 0;
	uint32_t video_pixelsize = 0;
	uint32_t video_pixeloffset = 0;
	uint8_t (*video_get_intensity)(const uint8_t *data) = nullptr;

	uint32_t audio_sample_rate = 0;
	size_t audio_channels = 0;

	/* Sync pattern detection from video */
	uint64_t start_ts = 0;

	struct quirc *qr = nullptr;
	uint32_t qr_step;
	struct corner_type qr_corners[N_CORNERS];
	st_qr_data qr_data;

	int64_t video_level_prev = 0;
	uint64_t video_level_prev_ts = 0;
	uint64_t video_marker_max_ts = 0;

	/* Sync pattern detection from audio */
	struct st_audio_buffer audio_buffer;
	struct peak_finder audio_marker_finder;
	uint32_t last_audio_index_max = 256;

	/* Multiplex sync pattern detection result */
	std::list<struct sync_index> sync_indices;

	std::mutex mutex;

	/* Audio pattern information from video to audio */
	uint32_t f = 0;
	uint32_t c = 0;
	uint32_t q_ms = 0;

	uint32_t f_last = 0;
	uint32_t c_last = 0;

	/* Sync latency baseline for stale match rejection */
	int64_t last_sync_latency_ns = 0;
	bool has_sync_baseline = false;

	/* Frame-drop detection state */
	int32_t last_qr_index = -1;
	uint32_t last_qr_index_max = 256;
	uint64_t total_frames_received = 0;
	uint64_t total_frames_dropped = 0;
	uint64_t frames_without_qr = 0;

	~sync_test_output()
	{
		if (qr)
			quirc_destroy(qr);
	}
};

static void video_marker_found(struct sync_test_output *st, uint64_t timestamp, float score);

static const char *st_get_name(void *)
{
	return "sync-test-output";
}

static void *st_create(obs_data_t *, obs_output_t *output)
{
	static const char *signals[] = {
		"void video_marker_found(ptr data)",
		"void audio_marker_found(ptr data)",
		"void qrcode_found(int timestamp, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)",
		"void sync_found(ptr data)",
		"void frame_drop_detected(ptr data)",
		"void render_timing(int frame_ts, int rendered_ns)",
		NULL,
	};
	signal_handler_add_array(obs_output_get_signal_handler(output), signals);

	auto *st = new sync_test_output;
	st->context = output;

	return st;
}

static void st_destroy(void *data)
{
	auto *st = (struct sync_test_output *)data;
	delete st;
}

static uint8_t get_intensity_10le(const uint8_t *data)
{
	uint16_t v = (data[0] >> 2) | (data[1] << 6);
	return (uint8_t)std::min<uint16_t>(v, 0xFF);
}

static bool st_start(void *data)
{
	auto *st = (struct sync_test_output *)data;

	const video_t *video = obs_output_video(st->context);
	if (!video) {
		blog(LOG_ERROR, "no video");
		return false;
	}
	const audio_t *audio = obs_output_audio(st->context);
	if (!audio) {
		blog(LOG_ERROR, "no audio");
		return false;
	}

	st->video_width = video_output_get_width(video);
	st->video_height = video_output_get_height(video);
	if (st->video_width > MAX_WIDTH_HEIGHT || st->video_height > MAX_WIDTH_HEIGHT) {
		blog(LOG_ERROR, "Requested size %ux%u exceeds maximum size %ux%u", st->video_width, st->video_height,
		     MAX_WIDTH_HEIGHT, MAX_WIDTH_HEIGHT);
		return false;
	}

	enum video_format video_format = video_output_get_format(video);
	switch (video_format) {
	case VIDEO_FORMAT_I420:
	case VIDEO_FORMAT_NV12:
	case VIDEO_FORMAT_I444:
	case VIDEO_FORMAT_I422:
	case VIDEO_FORMAT_I40A:
	case VIDEO_FORMAT_I42A:
	case VIDEO_FORMAT_YUVA:
		st->video_pixelsize = 1;
		st->video_pixeloffset = 0;
		st->video_get_intensity = nullptr;
		break;
	case VIDEO_FORMAT_I010:
		st->video_pixelsize = 2;
		st->video_pixeloffset = 0;
		st->video_get_intensity = get_intensity_10le;
		break;
	case VIDEO_FORMAT_P010:
		st->video_pixelsize = 2;
		st->video_pixeloffset = 1;
		st->video_get_intensity = nullptr;
		break;
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 1, 0)
	case VIDEO_FORMAT_P216:
	case VIDEO_FORMAT_P416:
		st->video_pixelsize = 2;
		st->video_pixeloffset = 1; // little endian
		st->video_get_intensity = nullptr;
		break;
#endif
	case VIDEO_FORMAT_RGBA:
	case VIDEO_FORMAT_BGRA:
	case VIDEO_FORMAT_BGRX:
		st->video_pixelsize = 4;
		st->video_pixeloffset = 1; // green channel
		st->video_get_intensity = nullptr;
		break;
	default:
		blog(LOG_ERROR, "unsupported pixel format %d", video_format);
		return false;
	}

	uint32_t qr_width = st->video_width;
	uint32_t qr_height = st->video_height;
	st->qr_step = 1;
	while (qr_width * qr_height > 640 * 480) {
		qr_width /= 2;
		qr_height /= 2;
		st->qr_step *= 2;
	}
	if (!st->qr)
		st->qr = quirc_new();
	if (!st->qr) {
		blog(LOG_ERROR, "failed to create QR code encoding context");
		return false;
	}
	if (quirc_resize(st->qr, qr_width, qr_height) < 0) {
		blog(LOG_ERROR, "failed to set-up QR code encoding context");
		return false;
	}

	st->audio_sample_rate = audio_output_get_sample_rate(audio);
	st->audio_channels = audio_output_get_channels(audio);

	st->has_sync_baseline = false;
	st->last_sync_latency_ns = 0;
	st->sync_indices.clear();

	obs_output_begin_data_capture(st->context, OBS_OUTPUT_VIDEO | OBS_OUTPUT_AUDIO);

	return true;
}

static void st_stop(void *data, uint64_t)
{
	auto *st = (struct sync_test_output *)data;

	obs_output_end_data_capture(st->context);
}

template<typename T> T sq(T x)
{
	return x * x;
}

static inline uint32_t diff_u32(uint32_t x, uint32_t y)
{
	if (x < y)
		return y - x;
	else
		return x - y;
}

static inline uint32_t sqrt_u32(uint32_t x)
{
	uint32_t r = 0;
	for (uint32_t b = 1 << 15; b; b >>= 1) {
		if (sq(r | b) <= x)
			r |= b;
	}
	return r;
}

static inline int qrcode_length(const struct corner_type *cc)
{
	auto l02 = hypotf((float)((int)cc[0].x - (int)cc[2].x), (float)((int)cc[0].y - (int)cc[2].y));
	auto l13 = hypotf((float)((int)cc[1].x - (int)cc[3].x), (float)((int)cc[1].y - (int)cc[3].y));
	return (int)((l02 + l13) * (float)(M_SQRT1_2 / 2.0f));
}

static inline void adjust_corners(struct corner_type *cc)
{
	int cx = 0, cy = 0;
	for (int i = 0; i < 4; i++) {
		cx += cc[i].x;
		cy += cc[i].y;
	}

	cx /= 4;
	cy /= 4;
	int r = qrcode_length(cc) / 4;

	// Move (x, y) to center side so that the circles will cover the pattern.
	for (int i = 0; i < 4; i++) {
		cc[i].x = (cc[i].x * 15 + cx * 9) / 24;
		cc[i].y = (cc[i].y * 15 + cy * 9) / 24;
		cc[i].r = r;
	}
}

static void signal_qrcode_found(obs_output_t *ctx, uint64_t timestamp, const struct corner_type *corners)
{
	uint8_t stack[384];
	struct calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	auto *sh = obs_output_get_signal_handler(ctx);

	calldata_set_int(&cd, "timestamp", timestamp);
	calldata_set_int(&cd, "x0", corners[0].x);
	calldata_set_int(&cd, "y0", corners[0].y);
	calldata_set_int(&cd, "x1", corners[1].x);
	calldata_set_int(&cd, "y1", corners[1].y);
	calldata_set_int(&cd, "x2", corners[2].x);
	calldata_set_int(&cd, "y2", corners[2].y);
	calldata_set_int(&cd, "x3", corners[3].x);
	calldata_set_int(&cd, "y3", corners[3].y);
	signal_handler_signal(sh, "qrcode_found", &cd);
}

static void st_raw_video_qrcode_decode(struct sync_test_output *st, struct video_data *frame)
{
	int w, h;
	auto qr = st->qr;
	uint8_t *qrbuf = quirc_begin(qr, &w, &h);

	const auto qr_step = st->qr_step;
	const auto pixelsize = st->video_pixelsize * qr_step;
	const uint8_t *linedata = frame->data[0] + frame->linesize[0] * (qr_step / 2);
	auto *ptr = qrbuf;
	for (int y = 0; y < h; y++) {
		const uint8_t *data = linedata + st->video_pixeloffset + st->video_pixelsize * (qr_step / 2);
		if (!st->video_get_intensity) {
			for (int x = 0; x < w; x++) {
				*ptr++ = *data;
				data += pixelsize;
			}
		}
		else {
			for (int x = 0; x < w; x++) {
				*ptr++ = st->video_get_intensity(data);
				data += pixelsize;
			}
		}

		linedata += frame->linesize[0] * qr_step;
	}

	quirc_end(qr);

	int num_codes = quirc_count(qr);

	for (int i = 0; i < num_codes; i++) {
		// (x0, y0): top left
		// (x1, y1): top right
		// (x2, y2): bottom right
		// (x3, y3): bottom left

		struct quirc_code code;
		struct quirc_data data;
		quirc_extract(qr, i, &code);
		auto err = quirc_decode(&code, &data);
		if (err == QUIRC_ERROR_DATA_ECC) {
			quirc_flip(&code);
			err = quirc_decode(&code, &data);
		}

		if (err)
			continue;

		data.payload[QUIRC_MAX_PAYLOAD - 1] = 0;
		if (!st->qr_data.decode((char *)data.payload))
			continue;

		for (int j = 0; j < 4; j++) {
			st->qr_corners[j].x = code.corners[j].x * st->qr_step;
			st->qr_corners[j].y = code.corners[j].y * st->qr_step;
		}

		signal_qrcode_found(st->context, frame->timestamp - st->start_ts, st->qr_corners);

		adjust_corners(st->qr_corners);

		if (st->qr_data.f > 0 && st->qr_data.c > 0) {
			std::unique_lock<std::mutex> lock(st->mutex);
			st->f = st->qr_data.f;
			st->c = st->qr_data.c;
			st->q_ms = st->qr_data.q_ms;
		}

		/* Frame-drop detection */
		{
			int32_t cur_index = (int32_t)st->qr_data.index;
			uint32_t index_max = st->qr_data.index_max;
			st->last_qr_index_max = index_max;
			st->total_frames_received++;
			st->frames_without_qr = 0;

			if (st->last_qr_index >= 0) {
				int32_t expected = (st->last_qr_index + 1) % (int32_t)index_max;
				if (cur_index != expected) {
					int dropped = ((int32_t)index_max + cur_index - expected) % (int32_t)index_max;
					if (dropped > 0 && (uint32_t)dropped < index_max / 2) {
						st->total_frames_dropped += dropped;

						struct frame_drop_event_s drop_data;
						drop_data.timestamp = frame->timestamp - st->start_ts;
						drop_data.expected_index = expected;
						drop_data.received_index = cur_index;
						drop_data.dropped_count = dropped;
						drop_data.total_received = st->total_frames_received;
						drop_data.total_dropped = st->total_frames_dropped;

						uint8_t stack[128];
						struct calldata cd;
						calldata_init_fixed(&cd, stack, sizeof(stack));
						auto *sh = obs_output_get_signal_handler(st->context);
						calldata_set_ptr(&cd, "data", &drop_data);
						signal_handler_signal(sh, "frame_drop_detected", &cd);

	}
				}
			}
			st->last_qr_index = cur_index;
		}

		/* For single-frame QR patterns (q_ms <= 34), fire video_marker_found
		 * directly since there are no checkerboard frames for zero-crossing. */
		if (st->qr_data.q_ms <= 34)
			video_marker_found(st, frame->timestamp, 1.0f);

		st->video_marker_max_ts = frame->timestamp + st->qr_data.q_ms * 3 * 1000000;
		st->video_level_prev = 0;
	}

	/* Track frames without QR for diagnostics */
	if (num_codes == 0 && st->last_qr_index >= 0)
		st->frames_without_qr++;
}

static void st_raw_video_find_marker(struct sync_test_output *st, struct video_data *frame)
{
	int64_t sum = 0;

	if (frame->timestamp > st->video_marker_max_ts) {
		st->video_level_prev = 0;
		return;
	}

	const uint8_t *linedata = frame->data[0];
	const uint32_t pixelsize = st->video_pixelsize;

	for (size_t i = 0; i < N_CORNERS; i++) {
		const struct corner_type c = st->qr_corners[i];
		if (c.r == 0)
			return;
		uint32_t y0 = c.y > c.r ? c.y - c.r : 0;
		uint32_t y1 = std::min(c.y + c.r, st->video_height);
		uint32_t sq_r = sq(c.r);

		for (uint32_t y = y0; y < y1; y++) {
			uint32_t dx = sqrt_u32(sq_r - sq(diff_u32(y, c.y)));
			uint32_t x0 = c.x > dx ? c.x - dx : 0;
			uint32_t x1 = std::min(c.x + dx, st->video_width);

			const uint8_t *data =
				linedata + frame->linesize[0] * y + st->video_pixeloffset + st->video_pixelsize * x0;

			uint32_t line_sum = 0;

			if (!st->video_get_intensity) {
				for (uint32_t x = x0; x < x1; x++) {
					line_sum += *data;
					data += pixelsize;
				}
			}
			else {
				for (uint32_t x = x0; x < x1; x++) {
					line_sum += st->video_get_intensity(data);
					data += pixelsize;
				}
			}

			if (i & 1)
				sum += line_sum;
			else
				sum -= line_sum;
		}
	}

	// blog(LOG_INFO, "st_raw_video-plot: %.03f %f", (frame->timestamp - st->start_ts) * 1e-9, (double)sum / (255.0 * M_PI * sq(st->qr_corners[0].r)));

	if (st->qr_data.valid && st->video_level_prev < 0 && sum >= 0) {
		/* Calculate the time half frame later than the zero-cross of `sum`. */
		uint64_t t = frame->timestamp - st->video_level_prev_ts;
		uint64_t add = util_mul_div64(t, sum - st->video_level_prev * 3, (sum - st->video_level_prev) * 2);
		video_marker_found(st, st->video_level_prev_ts + add, (float)(sum - st->video_level_prev));
	}
	st->video_level_prev = sum;
	st->video_level_prev_ts = frame->timestamp;
}

static bool is_overlapped(uint32_t index, uint32_t index_max, uint32_t next_index)
{
	return index_max && ((index_max + next_index - index) % index_max) > index_max / 2;
}

static void signal_sync_found(obs_output_t *ctx, const struct sync_index *si)
{
	uint8_t stack[64];
	struct calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	auto *sh = obs_output_get_signal_handler(ctx);

	calldata_set_ptr(&cd, "data", const_cast<sync_index *>(si));
	signal_handler_signal(sh, "sync_found", &cd);
}

static void sync_index_found(struct sync_test_output *st, int index, uint64_t ts, bool is_video, uint32_t index_max)
{
	std::unique_lock<std::mutex> lock(st->mutex);

	for (auto it = st->sync_indices.begin(); it != st->sync_indices.end();) {
		if ((it->video_ts && is_video) || (it->audio_ts && !is_video)) {
			if (is_overlapped(it->index, it->index_max, index)) {
				st->sync_indices.erase(it++);
				continue;
			}
		}

		if (it->index != index) {
			it++;
			continue;
		}

		/* Skip already-matched entries (kept for identify_audio_index_max) */
		if (it->matched) {
			it++;
			continue;
		}

		if ((it->video_ts && !is_video) || (it->audio_ts && is_video)) {
			(is_video ? it->video_ts : it->audio_ts) = ts;
			if (is_video)
				it->index_max = index_max;

			/* Reject stale matches: after video dropout, old audio entries
			 * can match new video events producing wrong latency. Check that
			 * the new latency doesn't deviate too far from the baseline. */
			int64_t latency_ns = (int64_t)it->audio_ts - (int64_t)it->video_ts;
			if (st->has_sync_baseline) {
				int64_t deviation = latency_ns - st->last_sync_latency_ns;
				if (deviation > 1000000000LL || deviation < -1000000000LL) {
					st->sync_indices.erase(it);
					break;
				}
			}

			st->last_sync_latency_ns = latency_ns;
			st->has_sync_baseline = true;
			it->matched = true;

			signal_sync_found(st->context, &*it);

			/* Do not erase `it` so that `identify_audio_index_max` can refer the last found pattern.
			 * Current `it` will be erased at the next call of this function. */
			return;
		}

		/* Remove the old one. Later, insert the new one to the end */
		st->sync_indices.erase(it);
		break;
	}

	while (st->sync_indices.size() >= 128)
		st->sync_indices.erase(st->sync_indices.begin());

	auto &ref = st->sync_indices.emplace_back();
	ref.index = index;
	(is_video ? ref.video_ts : ref.audio_ts) = ts;
	ref.index_max = index_max;
}

static void video_marker_found(struct sync_test_output *st, uint64_t timestamp, float score)
{
	uint8_t stack[64];
	struct calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	auto *sh = obs_output_get_signal_handler(st->context);

	struct video_marker_found_s data;
	data.timestamp = timestamp - st->start_ts;
	data.score = score;
	data.qr_data = st->qr_data;

	calldata_set_ptr(&cd, "data", &data);
	signal_handler_signal(sh, "video_marker_found", &cd);

	sync_index_found(st, data.qr_data.index, data.timestamp, true, data.qr_data.index_max);
}

static void st_raw_video(void *data, struct video_data *frame)
{
	auto *st = (struct sync_test_output *)data;

	if (!st->video_pixelsize)
		return;

	if (!st->start_ts)
		st->start_ts = frame->timestamp;

	st_raw_video_qrcode_decode(st, frame);
	st_raw_video_find_marker(st, frame);

	// Emit render timing signal with frame timestamp and actual OBS time
	uint8_t stack[128];
	struct calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_int(&cd, "frame_ts", frame->timestamp);
	calldata_set_int(&cd, "rendered_ns", os_gettime_ns());
	signal_handler_signal(obs_output_get_signal_handler(st->context), "render_timing", &cd);
}

static uint32_t identify_audio_index_max(struct sync_test_output *st, int index)
{
	/* Find `index_max` for video marker that have the biggest index but
	 * the index is less than or equal to the given index.
	 * In other words, find the closest but not future video marker.
	 */

	std::unique_lock<std::mutex> lock(st->mutex);
	uint32_t last_index_max = 256;
	uint32_t cand = st->last_audio_index_max;
	uint32_t cand_diff = 256;

	for (auto it = st->sync_indices.begin(); it != st->sync_indices.end(); it++) {
		if (!it->video_ts || !it->index_max)
			continue;
		uint32_t diff = (last_index_max + index - it->index) % last_index_max;
		if (diff < cand_diff) {
			cand = it->index_max;
			cand_diff = diff;
		}
		last_index_max = it->index_max;
	}

	return st->last_audio_index_max = cand;
}

static uint32_t crc4_check(uint32_t data, uint32_t size)
{
	uint32_t p = 0x13 << (size - 5);
	while (size > 4) {
		if (data & (1 << (size - 1)))
			data ^= p;
		size--;
		p >>= 1;
	}
	return data;
}

static inline void st_raw_audio_decode_data(struct sync_test_output *st, std::complex<float> phase, uint64_t ts)
{
	uint32_t symbol_num = st->audio_sample_rate * st->c_last;
	uint32_t symbol_den = st->f_last;

	uint16_t index = 0;
	for (int i = 0; i < 12; i += 2) {
		auto s0 = st->audio_buffer.sum(symbol_num * i / 2 / symbol_den);
		auto s1 = st->audio_buffer.sum(symbol_num * (i / 2 + 1) / symbol_den);
		auto x = int16_to_complex(s0 - s1);
		auto real = (x / phase).real();
		auto imag = (x / phase).imag();
		if (real > 0.0f)
			index |= 1 << i;
		if (imag > 0.0f)
			index |= 2 << i;
	}

	auto crc4 = crc4_check(0xF0000 | index, 20);

	if (crc4 != 0) {
		blog(LOG_DEBUG, "st_raw_audio_decode_data: CRC mismatch: received data=0x%03X crc=0x%X", index, crc4);
		return;
	}

	uint8_t stack[64];
	struct calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	auto *sh = obs_output_get_signal_handler(st->context);

	struct audio_marker_found_s data;
	data.timestamp = ts - st->start_ts;
	data.index = index >> 4;
	data.score = 0.0f;
	data.index_max = identify_audio_index_max(st, index >> 4);

	calldata_set_ptr(&cd, "data", &data);
	signal_handler_signal(sh, "audio_marker_found", &cd);

	sync_index_found(st, index >> 4, ts - st->start_ts, false, data.index_max);
}

static inline void st_raw_audio_test_preamble(struct sync_test_output *st, uint64_t ts, float v0)
{
	uint32_t f = st->f_last;
	uint32_t c1 = st->c_last / 2;
	uint64_t symbol_ns = util_mul_div64(c1, 1000000000ULL, f);
	size_t buffer_length = (size_t)(st->audio_sample_rate * c1 * N_SYMBOL_BUFFER / f);

	/* Test the preamble pattern 0xF0  */
	auto s0 = st->audio_buffer.sum(0);
	auto s4 = st->audio_buffer.sum(buffer_length * 4 / N_SYMBOL_BUFFER);
	auto s8 = st->audio_buffer.sum(buffer_length * 8 / N_SYMBOL_BUFFER);
	auto s12 = st->audio_buffer.sum(buffer_length * 12 / N_SYMBOL_BUFFER);

	float det8_0 = std::abs(int16_to_complex(s4 - s0) - int16_to_complex(s8 - s4));
	float det12_8 = det8_0 * 0.5f - std::abs(int16_to_complex(s12 - s8));
	float det = det8_0 + det12_8;

	UNUSED_PARAMETER(v0);
	// auto dbg = int16_to_complex(st->audio_buffer.sum(1) - s0);
	// blog(LOG_INFO, "st_raw_audio-plot: %.05f %f %f %f %f", (ts - st->start_ts) * 1e-9, v0, det, dbg.real(), dbg.imag());

	if (st->audio_marker_finder.append(det, ts, symbol_ns * 12)) {
		auto s12 = st->audio_buffer.sum(buffer_length * 12 / N_SYMBOL_BUFFER);
		auto s16 = st->audio_buffer.sum(buffer_length * 16 / N_SYMBOL_BUFFER);
		auto s20 = st->audio_buffer.sum(buffer_length * 20 / N_SYMBOL_BUFFER);

		auto x = int16_to_complex(s16 - s20) - int16_to_complex(s12 - s16);
		x *= std::complex(1.0f, -1.0f);

		ts = st->audio_marker_finder.last_ts - symbol_ns * N_AUDIO_SYMBOLS / 2;

		st_raw_audio_decode_data(st, x / std::abs(x), ts);
	}
}

static void st_raw_audio(void *data, struct audio_data *frames)
{
	auto *st = (struct sync_test_output *)data;

	if (!st->start_ts)
		return;

	std::unique_lock<std::mutex> lock(st->mutex);
	uint32_t f = st->f;
	uint32_t c = st->c;
	uint32_t q_ms = st->q_ms;
	lock.unlock();

	if (f <= 0 || c <= 0)
		return;

	if (f != st->f_last || c != st->c_last) {
		st->f_last = f;
		st->c_last = c;
		st->audio_buffer.buffer.clear();
	}

	if (q_ms > 0)
		st->audio_marker_finder.dumping_range = q_ms * 1000000 * 6 * 2;

	float phase = (frames->timestamp % 1000000000) * (float)(1e-9 * 2 * M_PI * f);
	float phase_step = (float)(2 * M_PI * f) / st->audio_sample_rate;
	size_t buffer_length = (size_t)(st->audio_sample_rate * c * N_SYMBOL_BUFFER / f);

	for (uint32_t i = 0; i < frames->frames; i++) {
		float osc0 = sinf(phase + phase_step * i);
		float osc1 = cosf(phase + phase_step * i);
		uint64_t ts = frames->timestamp + util_mul_div64(i, 1000000000ULL, st->audio_sample_rate);

		float v0 = ((float *)frames->data[0])[i];
		float v1 = st->audio_channels >= 2 ? ((float *)frames->data[1])[i] : 0.0f;
		int16_t vr = (int16_t)((v0 * osc0 - v1 * osc1) * 16383.0f);
		int16_t vi = (int16_t)((v0 * osc1 + v1 * osc0) * 16383.0f);
		st->audio_buffer.push_back(vr, vi, buffer_length);

		if (st->audio_buffer.buffer.size() < buffer_length)
			continue;

		st_raw_audio_test_preamble(st, ts, v0);
	}
}

extern "C" void register_sync_test_output()
{
	struct obs_output_info info = {};
	info.id = SYNC_TEST_OUTPUT_ID;
	info.flags = OBS_OUTPUT_AV;
	info.get_name = st_get_name;
	info.create = st_create;
	info.destroy = st_destroy;
	info.start = st_start;
	info.stop = st_stop;
	info.raw_video = st_raw_video;
	info.raw_audio = st_raw_audio;

	obs_register_output(&info);
}
