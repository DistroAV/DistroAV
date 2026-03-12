#pragma once
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

// Include NDI SDK definitions required for recv descriptor types
#include "..\lib\ndi\Processing.NDI.Lib.h"

#define NDI_MEM_NAME_PREFIX       L"Local\\NDI_"
#define NDI_REQUEST_SHM_SUFFIX    L"_RequestShm"
#define NDI_RESPONSE_SHM_SUFFIX   L"_ResponseShm"
#define NDI_COMMAND_EVENT_SUFFIX  L"_CommandEvent"
#define NDI_READY_EVENT_SUFFIX    L"_ReadyEvent"
#define NDI_RESPONSE_EVENT_SUFFIX L"_ResponseEvent"
#define NDI_CREATE_RECEIVER       100
#define NDI_CAPTURE_FRAME         200
#define NDI_SHUTDOWN              300
#define NDI_HARDWARE_ACCELERATION 400
#define NDI_CAPTURE_FRAME_SYNC    500
#define NDI_AUDIO_FRAME          1000
#define NDI_VIDEO_FRAME          2000
#define NDI_MULTI_FRAME          3000
#define NDI_NO_FRAME             9999

#define NDI_SERVER_DEBUG          false
#if NDI_SERVER_DEBUG
#define NDI_SERVER_WAIT           3000
#else
#define NDI_SERVER_WAIT           500
#endif

//
//  Buffer sizes
//
static const DWORD REQUEST_SIZE = 4096u;
static const DWORD RESPONSE_SIZE = 4000u * 3000u * 4u; // 48 000 000 bytes

//
//  Shared-memory layouts
//  NOTE: both structures are exactly REQUEST_SIZE / RESPONSE_SIZE bytes.
//        The spec says the first 32 bits carry the command; everything else is
//        free payload that real code would fill in.
//
#pragma pack(push, 1)

struct RequestBlock {
	uint32_t command;                  // bytes  0-3  : command the client sends
	uint32_t client_pid;               // bytes  4-7  : client's PID (set once at startup)
	uint8_t payload[REQUEST_SIZE - 8]; // bytes  8-4095
};

struct ResponseBlock {
	uint32_t payload_type; // bytes  0-3  : Content type of the payload (e.g. NDI_AUDIO_FRAME...)
	uint8_t payload[RESPONSE_SIZE - 4]; // bytes  4-47 999 999
};

#pragma pack(pop)

static_assert(sizeof(RequestBlock) == REQUEST_SIZE, "RequestBlock size mismatch");
static_assert(sizeof(ResponseBlock) == RESPONSE_SIZE, "ResponseBlock size mismatch");

//
//  Helper: die with a message
//
static void fatal(const char *msg)
{
	fprintf(stderr, "[Server] FATAL: %s  (error %u)\n", msg, GetLastError());
	ExitProcess(1);
}

//
//  Pre-fault a memory region by touching every page.
//  This converts the Windows "lazy-commit" behaviour into real physical pages
//  so there are zero page-faults during the 60-fps hot path.
//
static void PrefaultRegion(void *addr, SIZE_T bytes)
{
	volatile char *p = static_cast<volatile char *>(addr);
	for (SIZE_T i = 0; i < bytes; i += 4096)
		p[i] = 0;
}

//
//  Pre-fault a memory region by touching every page (read-only version).
//
static void PrefaultRegionRead(const void *addr, SIZE_T bytes)
{
	volatile const char *p = static_cast<volatile const char *>(addr);
	volatile char dummy = 0;
	for (SIZE_T i = 0; i < bytes; i += 4096)
		dummy |= p[i];
	(void)dummy;
}

// Serialize NDIlib_recv_create_v3_t into a byte buffer suitable for passing via shared memory.
// Format:
// [uint32_t color_format][uint32_t bandwidth][uint8_t allow_video_fields][uint32_t recv_name_len][uint32_t source_p_ndi_name_len][uint32_t source_p_url_len]
// [recv_name bytes][source_p_ndi_name bytes][source_p_url bytes]
// All string lengths are byte counts (no terminating NUL included). If a string is NULL, length==0 and no bytes are written.
static bool serialize_recv_desc(const NDIlib_recv_create_v3_t &desc, void *out_buf, size_t out_size,
				size_t &out_written)
{
	if (!out_buf || out_size < sizeof(uint32_t) * 5 + sizeof(uint8_t))
		return false;

	uint8_t *p = static_cast<uint8_t *>(out_buf);
	size_t remaining = out_size;

	uint32_t command_code = NDI_CREATE_RECEIVER;

	auto write_u32 = [&](uint32_t v) {
		if (remaining < sizeof(uint32_t))
			return false;
		memcpy(p, &v, sizeof(uint32_t));
		p += sizeof(uint32_t);
		remaining -= sizeof(uint32_t);
		return true;
	};
	auto write_u8 = [&](uint8_t v) {
		if (remaining < sizeof(uint8_t))
			return false;
		*p = v;
		p += sizeof(uint8_t);
		remaining -= sizeof(uint8_t);
		return true;
	};

	// prepare string data
	const char *recv_name = desc.p_ndi_recv_name ? desc.p_ndi_recv_name : "";
	const char *src_name = desc.source_to_connect_to.p_ndi_name ? desc.source_to_connect_to.p_ndi_name : "";
	const char *src_url = desc.source_to_connect_to.p_url_address ? desc.source_to_connect_to.p_url_address : "";

	uint32_t recv_name_len = static_cast<uint32_t>(strlen(recv_name));
	uint32_t src_name_len = static_cast<uint32_t>(strlen(src_name));
	uint32_t src_url_len = static_cast<uint32_t>(strlen(src_url));

	// header
	if (!write_u32(static_cast<uint32_t>(desc.color_format)))
		return false;
	if (!write_u32(static_cast<uint32_t>(desc.bandwidth)))
		return false;
	if (!write_u8(desc.allow_video_fields ? 1 : 0))
		return false;
	if (!write_u32(recv_name_len))
		return false;
	if (!write_u32(src_name_len))
		return false;
	if (!write_u32(src_url_len))
		return false;

	// write strings
	auto write_bytes = [&](const char *s, uint32_t len) {
		if (len == 0)
			return true;
		if (remaining < len)
			return false;
		memcpy(p, s, len);
		p += len;
		remaining -= len;
		return true;
	};

	if (!write_bytes(recv_name, recv_name_len))
		return false;
	if (!write_bytes(src_name, src_name_len))
		return false;
	if (!write_bytes(src_url, src_url_len))
		return false;

	out_written = static_cast<size_t>(out_size - remaining);
	return true;
}

// Deserialize buffer to NDIlib_recv_create_v3_t. The function fills out_desc and returns a vector of strings that own the memory
// pointed to by out_desc (the caller must keep the returned vector alive while using out_desc pointers).
static bool deserialize_recv_desc(const void *buf, size_t buf_len, NDIlib_recv_create_v3_t &out_desc,
				  std::vector<std::string> &out_strings)
{
	if (!buf || buf_len < sizeof(uint32_t) * 5 + sizeof(uint8_t))
		return false;
	const uint8_t *p = static_cast<const uint8_t *>(buf);
	size_t remaining = buf_len;

	auto read_u32 = [&](uint32_t &v) {
		if (remaining < sizeof(uint32_t))
			return false;
		memcpy(&v, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		remaining -= sizeof(uint32_t);
		return true;
	};
	auto read_u8 = [&](uint8_t &v) {
		if (remaining < sizeof(uint8_t))
			return false;
		v = *p;
		p += sizeof(uint8_t);
		remaining -= sizeof(uint8_t);
		return true;
	};

	uint32_t cf_u32 = 0;
	uint32_t bandwidth = 0;
	uint8_t allow_video_fields = 0;
	uint32_t recv_name_len = 0;
	uint32_t src_name_len = 0;
	uint32_t src_url_len = 0;

	if (!read_u32(cf_u32))
		return false;
	if (!read_u32(bandwidth))
		return false;
	if (!read_u8(allow_video_fields))
		return false;
	if (!read_u32(recv_name_len))
		return false;
	if (!read_u32(src_name_len))
		return false;
	if (!read_u32(src_url_len))
		return false;

	// Validate lengths
	if (recv_name_len > remaining)
		return false;
	// We'll check combined lengths
	size_t total_str_len = static_cast<size_t>(recv_name_len) + static_cast<size_t>(src_name_len) +
			       static_cast<size_t>(src_url_len);
	if (total_str_len > remaining)
		return false;

	// Extract strings
	out_strings.clear();
	out_strings.reserve(3);

	if (recv_name_len > 0) {
		out_strings.emplace_back(reinterpret_cast<const char *>(p), recv_name_len);
	} else {
		out_strings.emplace_back();
	}
	p += recv_name_len;
	remaining -= recv_name_len;

	if (src_name_len > 0) {
		out_strings.emplace_back(reinterpret_cast<const char *>(p), src_name_len);
	} else {
		out_strings.emplace_back();
	}
	p += src_name_len;
	remaining -= src_name_len;

	if (src_url_len > 0) {
		out_strings.emplace_back(reinterpret_cast<const char *>(p), src_url_len);
	} else {
		out_strings.emplace_back();
	}
	p += src_url_len;
	remaining -= src_url_len;

	// Fill out_desc
	memset(&out_desc, 0, sizeof(out_desc));
	// NOTE: Do not assign color_format here to avoid cross-library enum type mismatches.
	// The structure was zero-initialized above; callers can set color_format if required.
	out_desc.bandwidth = static_cast<NDIlib_recv_bandwidth_e>(bandwidth);
	out_desc.allow_video_fields = (allow_video_fields != 0);

	// Set pointers into the strings stored in out_strings; ensure c_str() remains valid by keeping out_strings alive.
	out_desc.p_ndi_recv_name = out_strings[0].empty() ? nullptr : out_strings[0].c_str();
	out_desc.source_to_connect_to.p_ndi_name = out_strings[1].empty() ? nullptr : out_strings[1].c_str();
	out_desc.source_to_connect_to.p_url_address = out_strings[2].empty() ? nullptr : out_strings[2].c_str();

	return true;
}

// Serialize a video or audio frame into the provided buffer.
// New buffer layout:
// [uint32_t frame_type]
// [struct bytes (NDIlib_video_frame_v2_t or NDIlib_audio_frame_v3_t)]
// [data bytes (size determined by struct fields)]
// Returns number of bytes written, or0 on error (insufficient space).
static size_t serialize_frame(NDIlib_frame_type_e frame_type, const void *frame, uint8_t *out_buf, size_t max_size)
{
	if (!out_buf || !frame || max_size < sizeof(uint32_t))
		return 0;

	uint8_t *p = out_buf;
	size_t remaining = max_size;

	// write frame type (32-bit)
	uint32_t ft = static_cast<uint32_t>(frame_type);
	memcpy(p, &ft, sizeof(ft));
	p += sizeof(ft);
	remaining -= sizeof(ft);

	// Helper to write raw bytes
	auto write_bytes = [&](const void *src, size_t sz) -> bool {
		if (remaining < sz)
			return false;
		memcpy(p, src, sz);
		p += sz;
		remaining -= sz;
		return true;
	};

	if (frame_type == NDIlib_frame_type_video) {
		const NDIlib_video_frame_v2_t *vf = static_cast<const NDIlib_video_frame_v2_t *>(frame);
		size_t struct_sz = sizeof(NDIlib_video_frame_v2_t);

		// Determine data size
		int data_size = vf->data_size_in_bytes * vf->yres;

		// metadata length (count bytes including NUL)
		int meta_len = vf->p_metadata ? static_cast<int>(strlen(vf->p_metadata)) + 1 : 1;

		// Check total space
		if (remaining < struct_sz + (size_t)data_size + (size_t)meta_len)
			return 0;

		// memcpy entire struct representation
		if (!write_bytes(vf, struct_sz))
			return 0;

		// append frame data
		if (data_size > 0) {
			if (!write_bytes(vf->p_data, (size_t)data_size))
				return 0;
		}

		return max_size - remaining;
	}

	if (frame_type == NDIlib_frame_type_audio) {
		const NDIlib_audio_frame_v3_t *af = static_cast<const NDIlib_audio_frame_v3_t *>(frame);
		size_t struct_sz = sizeof(NDIlib_audio_frame_v3_t);

		int data_size = 0;

		data_size = af->channel_stride_in_bytes * af->no_channels;

		if (remaining < struct_sz + (size_t)data_size)
			return 0;

		// copy whole struct
		if (!write_bytes(af, struct_sz))
			return 0;

		// append audio data
		if (data_size > 0) {
			if (!write_bytes(af->p_data, (size_t)data_size))
				return 0;
		}

		return max_size - remaining;
	}

	// unsupported frame type
	return 0;
}

// Deserialize a buffer produced by serialize_frame. Populates either out_video or out_audio depending on frame type.
// The buffer memory must remain valid for the lifetime of the returned frame pointers.
// Returns true on success, false on failure.
static size_t deserialize_frame(const uint8_t *buf, size_t buf_len, NDIlib_frame_type_e &out_type,
				NDIlib_video_frame_v2_t *out_video, NDIlib_audio_frame_v3_t *out_audio)
{
	if (!buf || buf_len < sizeof(uint32_t))
		return 0;

	const uint8_t *p = buf;
	size_t remaining = buf_len;

	// Read frame type
	uint32_t ft = 0;
	memcpy(&ft, p, sizeof(ft));
	p += sizeof(ft);
	remaining -= sizeof(ft);

	out_type = static_cast<NDIlib_frame_type_e>(ft);

	if (out_type == NDIlib_frame_type_video) {
		if (!out_video)
			return 0;

		size_t struct_sz = sizeof(NDIlib_video_frame_v2_t);
		if (remaining < struct_sz)
			return 0;

		// Copy entire struct from buffer into out_video
		memcpy(out_video, p, struct_sz);
		p += struct_sz;
		remaining -= struct_sz;

		// Determine data size from the struct fields
		int data_size = out_video->data_size_in_bytes * out_video->yres;

		if (remaining < (size_t)data_size + 1) // need at least data + one byte for metadata NUL
			return 0;

		// Set p_data pointer into buffer
		if (data_size > 0) {
			out_video->p_data = const_cast<uint8_t *>(p);
			p += data_size;
			remaining -= data_size;
		} else {
			out_video->p_data = nullptr;
		}

		out_video->p_metadata = nullptr;

		// Note: timestamp and timecode already copied from struct bytes
		return buf_len - remaining;
	}

	if (out_type == NDIlib_frame_type_audio) {
		if (!out_audio)
			return 0;

		size_t struct_sz = sizeof(NDIlib_audio_frame_v3_t);
		if (remaining < struct_sz)
			return 0;

		memcpy(out_audio, p, struct_sz);
		p += struct_sz;
		remaining -= struct_sz;

		int data_size = out_audio->channel_stride_in_bytes * out_audio->no_channels;

		if (remaining < (size_t)data_size + 1)
			return 0;

		if (data_size > 0) {
			out_audio->p_data = const_cast<uint8_t *>(p);
			p += data_size;
			remaining -= data_size;
		} else {
			out_audio->p_data = nullptr;
		}

		out_audio->p_metadata = nullptr;

		return buf_len - remaining;
	}

	// unsupported frame type
	return 0;
}
