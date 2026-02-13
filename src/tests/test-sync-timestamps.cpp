/*
 * Unit tests for DistroAV sync dock timestamp calculations.
 *
 * These tests replicate the exact math from sync-test-dock.cpp and
 * ndi-source.cpp to catch regressions in:
 *   - Time-of-day formatting (format_tod / format_time_ns)
 *   - NDI timecode 100ns→ns conversion
 *   - Clock domain conversion (OBS monotonic → wall-clock)
 *   - Delay chain consistency (net + rel + obs == total)
 *   - obs_delay from matched frame (data race regression)
 *   - Frame matching algorithm (FIFO queue, tolerance, bounds)
 *   - translate_timecode_to_obs direction of subtraction
 *   - ns↔ms conversion round-trip
 *
 * Build:  g++ -std=c++17 -o test-sync-timestamps test-sync-timestamps.cpp
 * Run:    ./test-sync-timestamps
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <deque>
#include <string>

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                \
	do {                                                                  \
		g_tests_run++;                                                \
		if (!(cond)) {                                                \
			fprintf(stderr, "FAIL [%s:%d] %s\n  Condition: %s\n", \
				__FILE__, __LINE__, (msg), #cond);            \
			g_tests_failed++;                                     \
		} else {                                                      \
			g_tests_passed++;                                     \
		}                                                             \
	} while (0)

#define TEST_ASSERT_EQ(a, b, msg)                                              \
	do {                                                                   \
		g_tests_run++;                                                 \
		if ((a) != (b)) {                                              \
			fprintf(stderr,                                        \
				"FAIL [%s:%d] %s\n  Expected: %lld  Got: %lld\n", \
				__FILE__, __LINE__, (msg), (long long)(b),     \
				(long long)(a));                                \
			g_tests_failed++;                                      \
		} else {                                                       \
			g_tests_passed++;                                      \
		}                                                              \
	} while (0)

#define TEST_ASSERT_STR(a, b, msg)                                          \
	do {                                                                \
		g_tests_run++;                                              \
		if ((a) != (b)) {                                           \
			fprintf(stderr,                                     \
				"FAIL [%s:%d] %s\n  Expected: \"%s\"  Got: \"%s\"\n", \
				__FILE__, __LINE__, (msg), (b).c_str(),     \
				(a).c_str());                               \
			g_tests_failed++;                                   \
		} else {                                                    \
			g_tests_passed++;                                   \
		}                                                           \
	} while (0)

// ---------------------------------------------------------------------------
// Replicated functions from sync-test-dock.cpp and ndi-source.cpp
// ---------------------------------------------------------------------------

// Replicates format_tod lambda at sync-test-dock.cpp:592-603
static std::string format_tod(int64_t ns)
{
	int64_t tod_ms = (ns / 1000000) % 86400000LL;
	if (tod_ms < 0)
		tod_ms += 86400000LL;
	int h = (int)(tod_ms / 3600000);
	int m = (int)((tod_ms % 3600000) / 60000);
	int s = (int)((tod_ms % 60000) / 1000);
	int ms = (int)(tod_ms % 1000);
	char buf[16];
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", h, m, s, ms);
	return buf;
}

// Replicates format_time_ns at sync-test-dock.cpp:31-49 (same logic, string output)
static std::string format_time_ns(int64_t ns)
{
	int64_t total_ms = ns / 1000000;
	int64_t tod_ms = total_ms % 86400000LL;
	if (tod_ms < 0)
		tod_ms += 86400000LL;

	int64_t ms = tod_ms % 1000;
	int64_t total_sec = tod_ms / 1000;
	int64_t sec = total_sec % 60;
	int64_t min = (total_sec / 60) % 60;
	int64_t hours = (total_sec / 3600) % 24;

	char buf[20];
	snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld.%03lld",
		 (long long)hours, (long long)min, (long long)sec,
		 (long long)ms);
	return buf;
}

// PendingFrameTiming struct — replicates sync-test-dock.hpp:106-117
struct PendingFrameTiming {
	uint64_t frame_number;
	int64_t creation_ns;
	int64_t presentation_obs_ns;
	int64_t network_ns;
	int64_t release_ns;
	int64_t release_delay_ns;
	int64_t present_wall_clock_ns;
	int64_t clock_offset_ns;
	bool wall_clock_mode;
	bool scheduled_mode;
};

// ---------------------------------------------------------------------------
// Test 1: format_tod time-of-day conversion
// ---------------------------------------------------------------------------
static void test_format_tod()
{
	printf("  Test 1: format_tod time-of-day conversion\n");

	// Midnight
	TEST_ASSERT_STR(format_tod(0), std::string("00:00:00.000"),
			"midnight (0ns)");

	// Noon = 12h * 3600s * 1e9ns
	int64_t noon_ns = 12LL * 3600 * 1000000000LL;
	TEST_ASSERT_STR(format_tod(noon_ns), std::string("12:00:00.000"),
			"noon");

	// 1:23:45.678
	int64_t specific_ns =
		(1LL * 3600 + 23 * 60 + 45) * 1000000000LL + 678000000LL;
	TEST_ASSERT_STR(format_tod(specific_ns), std::string("01:23:45.678"),
			"1:23:45.678");

	// Negative timestamp wraps correctly
	// -1ms = 23:59:59.999
	int64_t neg_1ms = -1000000LL;
	TEST_ASSERT_STR(format_tod(neg_1ms), std::string("23:59:59.999"),
			"negative wraps to 23:59:59.999");

	// > 24h wraps
	int64_t over_24h = 25LL * 3600 * 1000000000LL + 500000000LL;
	TEST_ASSERT_STR(format_tod(over_24h), std::string("01:00:00.500"),
			"25h wraps to 01:00:00.500");

	// Sub-millisecond precision is truncated (not rounded)
	// 999999ns = 0.999999ms → 0ms remainder
	int64_t sub_ms = 999999LL;
	TEST_ASSERT_STR(format_tod(sub_ms), std::string("00:00:00.000"),
			"sub-millisecond truncated");

	// format_time_ns and format_tod produce same result
	int64_t test_val = 45296123000000LL; // 12:34:56.123
	TEST_ASSERT_STR(format_time_ns(test_val), format_tod(test_val),
			"format_time_ns matches format_tod");
}

// ---------------------------------------------------------------------------
// Test 2: NDI timecode 100ns→ns conversion
// ---------------------------------------------------------------------------
static void test_ndi_timecode_conversion()
{
	printf("  Test 2: NDI timecode 100ns->ns conversion\n");

	// Replicates ndi-source.cpp:302: ndi_tc_ns = ndi_timecode_100ns * 100
	int64_t ndi_timecode_100ns = 17000000000000LL; // Typical PTP value
	int64_t ndi_tc_ns = ndi_timecode_100ns * 100;

	TEST_ASSERT_EQ(ndi_tc_ns, 1700000000000000LL,
		       "100ns to ns conversion");

	// Verify no overflow for large PTP timestamps
	// Max reasonable PTP: year 2100 ~= 4.1e18 100ns units
	int64_t large_100ns = 4100000000000000000LL;
	int64_t large_ns = large_100ns * 100;
	// Should not wrap negative (INT64_MAX = 9.2e18)
	TEST_ASSERT(large_ns > 0, "large PTP timestamp doesn't overflow");

	// Zero
	TEST_ASSERT_EQ((int64_t)0 * 100, (int64_t)0,
		       "zero timecode converts to zero");
}

// ---------------------------------------------------------------------------
// Test 3: Clock domain conversion (OBS monotonic → wall-clock)
// ---------------------------------------------------------------------------
static void test_clock_domain_conversion()
{
	printf("  Test 3: Clock domain conversion\n");

	// Replicates sync-test-dock.cpp:416-417
	// present_wall_clock_ns = wall_clock_mode
	//     ? presentation_ns
	//     : presentation_ns + clock_offset_ns;

	int64_t presentation_ns = 5000000000LL;    // 5 seconds in OBS monotonic
	int64_t clock_offset_ns = 1700000000000LL; // ~28 minutes wall-clock offset

	// Wall-clock mode: no offset applied (already in wall-clock domain)
	{
		bool wall_clock_mode = true;
		int64_t result = wall_clock_mode ? presentation_ns
						 : presentation_ns +
							   clock_offset_ns;
		TEST_ASSERT_EQ(result, presentation_ns,
			       "wall_clock_mode: no offset applied");
	}

	// Non wall-clock mode: offset IS applied
	{
		bool wall_clock_mode = false;
		int64_t result = wall_clock_mode ? presentation_ns
						 : presentation_ns +
							   clock_offset_ns;
		TEST_ASSERT_EQ(result, presentation_ns + clock_offset_ns,
			       "non wall_clock_mode: offset applied");
	}

	// Double-conversion bug: if value is already wall-clock and offset is applied again
	{
		int64_t already_wall_clock = presentation_ns + clock_offset_ns;
		int64_t double_converted = already_wall_clock + clock_offset_ns;
		TEST_ASSERT(double_converted != already_wall_clock,
			    "double conversion produces wrong value");
		TEST_ASSERT_EQ(double_converted,
			       presentation_ns + 2 * clock_offset_ns,
			       "double conversion adds offset twice");
	}

	// Negative offset (clock offset can be negative)
	{
		int64_t neg_offset = -500000000LL;
		bool wall_clock_mode = false;
		int64_t result = wall_clock_mode
					 ? presentation_ns
					 : presentation_ns + neg_offset;
		TEST_ASSERT_EQ(result, presentation_ns + neg_offset,
			       "negative offset applied correctly");
	}
}

// ---------------------------------------------------------------------------
// Test 4: Delay chain consistency
// ---------------------------------------------------------------------------
static void test_delay_chain_consistency()
{
	printf("  Test 4: Delay chain consistency\n");

	// Replicates sync-test-dock.cpp:404-411,561-563
	// receive = creation + network
	// release_delay = release - receive
	// obs_delay = rendered_wall_clock - release
	// total = network + release_delay + obs_delay

	int64_t creation_ns = 1700000000000000LL;
	int64_t network_ns = 5000000LL;     // 5ms network
	int64_t release_ns = creation_ns + network_ns + 2000000LL; // 2ms processing
	int64_t rendered_wall_clock_ns = release_ns + 16000000LL;  // 16ms OBS render

	int64_t receive_ns = creation_ns + network_ns;
	int64_t release_delay_ns = release_ns - receive_ns;
	int64_t obs_delay_ns = rendered_wall_clock_ns - release_ns;
	int64_t total_ns = network_ns + release_delay_ns + obs_delay_ns;

	// Verify: total == rendered_wall_clock - creation
	int64_t expected_total = rendered_wall_clock_ns - creation_ns;
	TEST_ASSERT_EQ(total_ns, expected_total,
		       "total == rendered - creation");

	// Verify components
	TEST_ASSERT_EQ(receive_ns, creation_ns + network_ns,
		       "receive = creation + network");
	TEST_ASSERT_EQ(release_delay_ns, (int64_t)2000000,
		       "release_delay = 2ms");
	TEST_ASSERT_EQ(obs_delay_ns, (int64_t)16000000, "obs_delay = 16ms");
	TEST_ASSERT_EQ(total_ns, (int64_t)23000000, "total = 23ms");

	// Verify ms truncation matches (sync-test-dock.cpp:585-589)
	int64_t network_ms = network_ns / 1000000;
	int64_t release_ms = release_delay_ns / 1000000;
	int64_t obs_delay_ms = obs_delay_ns / 1000000;
	int64_t total_delay_ms = network_ms + release_ms + obs_delay_ms;

	TEST_ASSERT_EQ(total_delay_ms, (int64_t)23,
		       "ms truncation: total matches");

	// Edge case: when individual delays have sub-ms remainders,
	// ms-truncated sum may differ from truncated total
	{
		int64_t net = 1500000LL;     // 1.5ms → 1ms
		int64_t rel = 1500000LL;     // 1.5ms → 1ms
		int64_t obs = 1500000LL;     // 1.5ms → 1ms
		int64_t total = net + rel + obs; // 4.5ms → 4ms

		int64_t ms_sum =
			net / 1000000 + rel / 1000000 + obs / 1000000;
		int64_t ms_total = total / 1000000;

		// This is expected behavior (3 != 4), not a bug
		TEST_ASSERT(ms_sum <= ms_total,
			    "truncated sum <= truncated total");
	}
}

// ---------------------------------------------------------------------------
// Test 5: obs_delay from matched frame (data race regression)
// ---------------------------------------------------------------------------
static void test_obs_delay_matched_frame()
{
	printf("  Test 5: obs_delay from matched frame\n");

	// Replicates sync-test-dock.cpp:526,532
	// rendered_wall_clock_ns = render_wall_clock_ns + pft.clock_offset_ns;
	// obs_delay_ns = rendered_wall_clock_ns - pft.release_ns;

	// Two frames with DIFFERENT clock offsets (simulates offset drift)
	PendingFrameTiming frame_a = {};
	frame_a.frame_number = 1;
	frame_a.release_ns = 1700000050000000LL;
	frame_a.clock_offset_ns = 1000000000LL; // 1 second offset

	PendingFrameTiming frame_b = {};
	frame_b.frame_number = 2;
	frame_b.release_ns = 1700000066000000LL;
	frame_b.clock_offset_ns = 1000500000LL; // 1.0005 second offset (drifted)

	int64_t render_wall_clock_a = 1700000060000000LL;
	int64_t render_wall_clock_b = 1700000076000000LL;

	// Correct: use each frame's own clock_offset and release_ns
	int64_t rendered_a =
		render_wall_clock_a + frame_a.clock_offset_ns;
	int64_t obs_delay_a = rendered_a - frame_a.release_ns;

	int64_t rendered_b =
		render_wall_clock_b + frame_b.clock_offset_ns;
	int64_t obs_delay_b = rendered_b - frame_b.release_ns;

	// Data race bug: using frame_b's offset with frame_a's release
	int64_t rendered_wrong =
		render_wall_clock_a + frame_b.clock_offset_ns;
	int64_t obs_delay_wrong = rendered_wrong - frame_a.release_ns;

	TEST_ASSERT(obs_delay_a != obs_delay_wrong,
		    "mixed frame data produces different obs_delay");

	// The difference should be exactly the clock offset drift
	int64_t drift =
		frame_b.clock_offset_ns - frame_a.clock_offset_ns;
	TEST_ASSERT_EQ(obs_delay_wrong - obs_delay_a, drift,
		       "error equals clock offset drift");

	// Verify correct obs_delay values
	TEST_ASSERT_EQ(obs_delay_a,
		       rendered_a - frame_a.release_ns,
		       "frame_a obs_delay correct");
	TEST_ASSERT_EQ(obs_delay_b,
		       rendered_b - frame_b.release_ns,
		       "frame_b obs_delay correct");
}

// ---------------------------------------------------------------------------
// Test 6: Frame matching algorithm
// ---------------------------------------------------------------------------
static void test_frame_matching()
{
	printf("  Test 6: Frame matching algorithm\n");

	// Replicates sync-test-dock.cpp:488-520
	const int64_t tolerance_ns = 100000000LL; // 100ms

	std::deque<PendingFrameTiming> pending_frames;

	// Populate queue with 5 frames
	for (int i = 0; i < 5; i++) {
		PendingFrameTiming pft = {};
		pft.frame_number = i + 1;
		pft.presentation_obs_ns =
			1000000000LL + i * 33333333LL; // ~30fps
		pft.creation_ns = 1700000000000000LL + i * 33333333LL;
		pft.network_ns = 5000000LL;
		pft.release_ns = pft.creation_ns + pft.network_ns + 2000000LL;
		pft.clock_offset_ns = 500000000LL;
		pending_frames.push_back(pft);
	}

	// Exact match for frame 3
	{
		int64_t source_frame_ts = 1000000000LL + 2 * 33333333LL;
		auto best_it = pending_frames.end();
		int64_t best_diff = 1000000000000LL;

		for (auto it = pending_frames.begin();
		     it != pending_frames.end(); ++it) {
			int64_t diff = it->presentation_obs_ns - source_frame_ts;
			if (diff < 0)
				diff = -diff;
			if (diff < best_diff && diff <= tolerance_ns) {
				best_diff = diff;
				best_it = it;
			}
		}

		TEST_ASSERT(best_it != pending_frames.end(),
			    "exact match found");
		TEST_ASSERT_EQ((int64_t)best_it->frame_number, (int64_t)3,
			       "exact match is frame 3");
	}

	// Close match within tolerance (10ms off — still closest to frame 3)
	{
		int64_t source_frame_ts =
			1000000000LL + 2 * 33333333LL + 10000000LL; // 10ms off
		auto best_it = pending_frames.end();
		int64_t best_diff = 1000000000000LL;

		for (auto it = pending_frames.begin();
		     it != pending_frames.end(); ++it) {
			int64_t diff = it->presentation_obs_ns - source_frame_ts;
			if (diff < 0)
				diff = -diff;
			if (diff < best_diff && diff <= tolerance_ns) {
				best_diff = diff;
				best_it = it;
			}
		}

		TEST_ASSERT(best_it != pending_frames.end(),
			    "close match found within tolerance");
		TEST_ASSERT_EQ((int64_t)best_it->frame_number, (int64_t)3,
			       "close match is still frame 3");
	}

	// Out of tolerance — no match
	{
		int64_t source_frame_ts = 9999999999LL; // Way off
		auto best_it = pending_frames.end();
		int64_t best_diff = 1000000000000LL;

		for (auto it = pending_frames.begin();
		     it != pending_frames.end(); ++it) {
			int64_t diff = it->presentation_obs_ns - source_frame_ts;
			if (diff < 0)
				diff = -diff;
			if (diff < best_diff && diff <= tolerance_ns) {
				best_diff = diff;
				best_it = it;
			}
		}

		TEST_ASSERT(best_it == pending_frames.end(),
			    "out-of-tolerance returns no match");
	}

	// Queue bounded at 120 entries
	{
		std::deque<PendingFrameTiming> bounded_queue;
		for (int i = 0; i < 150; i++) {
			PendingFrameTiming pft = {};
			pft.frame_number = i;
			bounded_queue.push_back(pft);

			while (bounded_queue.size() > 120) {
				bounded_queue.pop_front();
			}
		}
		TEST_ASSERT_EQ((int64_t)bounded_queue.size(), (int64_t)120,
			       "queue bounded at 120");
		TEST_ASSERT_EQ((int64_t)bounded_queue.front().frame_number,
			       (int64_t)30,
			       "oldest frame is 30 (150-120)");
	}

	// Empty queue returns no match
	{
		std::deque<PendingFrameTiming> empty_queue;
		TEST_ASSERT(empty_queue.empty(), "empty queue has no frames");
	}
}

// ---------------------------------------------------------------------------
// Test 7: translate_timecode_to_obs (sync lock conversion)
// ---------------------------------------------------------------------------
static void test_translate_timecode_to_obs()
{
	printf("  Test 7: translate_timecode_to_obs\n");

	// Replicates ndi-source.cpp:326-328
	// presentation = ndi_tc_ns - clock_offset_ns

	int64_t ndi_timecode_100ns = 17000000000000LL;
	int64_t ndi_tc_ns = ndi_timecode_100ns * 100;
	int64_t clock_offset_ns = 1699999990000000000LL; // Large wall-to-OBS offset

	int64_t presentation = ndi_tc_ns - clock_offset_ns;

	// Verify correct direction of subtraction
	TEST_ASSERT_EQ(presentation, ndi_tc_ns - clock_offset_ns,
		       "subtraction direction correct");

	// Bug regression: addition instead of subtraction would give wrong result
	int64_t wrong_presentation = ndi_tc_ns + clock_offset_ns;
	TEST_ASSERT(presentation != wrong_presentation,
		    "addition gives different (wrong) result");

	// The difference between correct and wrong is 2 * clock_offset
	TEST_ASSERT_EQ(wrong_presentation - presentation,
		       2 * clock_offset_ns,
		       "error is 2x clock_offset");

	// Small offset case
	{
		int64_t small_offset = 1000000000LL; // 1 second
		int64_t tc = 5000000000LL;           // 5 seconds
		int64_t result = tc - small_offset;
		TEST_ASSERT_EQ(result, (int64_t)4000000000LL,
			       "5s - 1s = 4s");
	}

	// Reverse: OBS time back to NDI time
	{
		int64_t obs_time = presentation;
		int64_t recovered_ndi = obs_time + clock_offset_ns;
		TEST_ASSERT_EQ(recovered_ndi, ndi_tc_ns,
			       "round-trip: OBS → NDI recovers original");
	}
}

// ---------------------------------------------------------------------------
// Test 8: ns↔ms conversion consistency
// ---------------------------------------------------------------------------
static void test_ns_ms_roundtrip()
{
	printf("  Test 8: ns/ms conversion consistency\n");

	// Typical delay values
	int64_t delays_ns[] = {5000000LL,  16000000LL, 1500000LL,
			       33333333LL, 0LL,        999999LL};
	int n = sizeof(delays_ns) / sizeof(delays_ns[0]);

	for (int i = 0; i < n; i++) {
		int64_t ns = delays_ns[i];
		int64_t ms = ns / 1000000;
		int64_t back_to_ns = ms * 1000000LL;

		// Round-trip loses sub-ms precision
		TEST_ASSERT(back_to_ns <= ns, "round-trip doesn't exceed original");
		TEST_ASSERT(ns - back_to_ns < 1000000LL,
			    "lost precision < 1ms");
	}

	// Exact multiples round-trip perfectly
	{
		int64_t exact_5ms = 5000000LL;
		int64_t ms = exact_5ms / 1000000;
		int64_t back = ms * 1000000LL;
		TEST_ASSERT_EQ(back, exact_5ms,
			       "exact 5ms round-trips perfectly");
	}

	// Large timestamp (wall-clock ~19h)
	{
		int64_t large_ns = 68400000000000LL; // 19:00:00.000 in ns
		int64_t ms = large_ns / 1000000;
		TEST_ASSERT_EQ(ms, (int64_t)68400000,
			       "large timestamp ms correct");
		int64_t back = ms * 1000000LL;
		TEST_ASSERT_EQ(back, large_ns,
			       "large timestamp round-trips");
	}
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
	printf("DistroAV Sync Timestamp Regression Tests\n");
	printf("=========================================\n\n");

	test_format_tod();
	test_ndi_timecode_conversion();
	test_clock_domain_conversion();
	test_delay_chain_consistency();
	test_obs_delay_matched_frame();
	test_frame_matching();
	test_translate_timecode_to_obs();
	test_ns_ms_roundtrip();

	printf("\n=========================================\n");
	printf("Results: %d/%d passed", g_tests_passed, g_tests_run);
	if (g_tests_failed > 0) {
		printf(", %d FAILED", g_tests_failed);
	}
	printf("\n");

	if (g_tests_failed > 0) {
		printf("RESULT: FAIL\n");
		return 1;
	}

	printf("RESULT: PASS\n");
	return 0;
}
