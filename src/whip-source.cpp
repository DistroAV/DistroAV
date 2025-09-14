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

#include "plugin-main.h"
#include "obs.h"

#include <util/platform.h>
#include <util/threading.h>

#include <QDesktopServices>
#include <QUrl>

#include <thread>

#include <rtc/rtc.h>
#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstring>
#include <sstream>
#include <memory>
#include <queue>
#include <mutex>
#include <algorithm>
#include <iomanip>

// FFmpeg includes for decoding
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

// Simple HTTP server implementation for WHIP endpoint
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#undef min
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// Forward declarations
class WhipReceiver;
class WhipHttpServer;

// WHIP source structure
typedef struct {
	obs_source_t *obs_source;
	std::string endpoint;
	std::unique_ptr<WhipReceiver> receiver;
} whip_source_t;

// RTP header structure
struct RTPHeader {
	uint8_t version : 2;
	uint8_t padding : 1;
	uint8_t extension : 1;
	uint8_t cc : 4;
	uint8_t marker : 1;
	uint8_t pt : 7;
	uint16_t seq;
	uint32_t ts;
	uint32_t ssrc;

	void parseFromBuffer(const uint8_t *buffer)
	{
		uint8_t byte0 = buffer[0];
		version = (byte0 >> 6) & 0x03;
		padding = (byte0 >> 5) & 0x01;
		extension = (byte0 >> 4) & 0x01;
		cc = byte0 & 0x0F;

		uint8_t byte1 = buffer[1];
		marker = (byte1 >> 7) & 0x01;
		pt = byte1 & 0x7F;

		seq = (buffer[2] << 8) | buffer[3];
		ts = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
		ssrc = (buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11];
	}

	static int getHeaderSize(const uint8_t *buffer)
	{
		uint8_t cc = buffer[0] & 0x0F;
		return 12 + (cc * 4); // Basic header + CSRC list
	}
};

// Media decoder base class
class MediaDecoder {
public:
	virtual ~MediaDecoder() = default;
	virtual bool initialize(obs_source_t *obs_source = nullptr) = 0;
	virtual void processRTPPacket(const uint8_t *data, int size) = 0;
	virtual void cleanup() = 0;
};

// H.264 Video Decoder with proper parameter set handling
class H264Decoder : public MediaDecoder {
private:
	const AVCodec *codec;
	AVCodecContext *codecCtx;
	AVFrame *frame;
	AVPacket *packet;
	SwsContext *swsCtx;
	obs_source_t *obs_source;

	std::queue<std::vector<uint8_t>> nalQueue;
	std::mutex nalMutex;
	std::vector<uint8_t> fragmentBuffer;
	uint16_t lastSeq;
	bool firstPacket;

	// Parameter set storage
	std::vector<uint8_t> sps;
	std::vector<uint8_t> pps;
	bool hasParameterSets;
	bool codecInitialized;

public:
	H264Decoder()
		: codec(nullptr),
		  codecCtx(nullptr),
		  frame(nullptr),
		  packet(nullptr),
		  swsCtx(nullptr),
		  obs_source(nullptr),
		  lastSeq(0),
		  firstPacket(true),
		  hasParameterSets(false),
		  codecInitialized(false)
	{
	}

	~H264Decoder() { cleanup(); }

	bool initialize(obs_source_t *source = nullptr) override
	{
		obs_source = source;

		// Find H.264 decoder
		codec = avcodec_find_decoder(AV_CODEC_ID_H264);
		if (!codec) {
			obs_log(LOG_ERROR, "H.264 decoder not found");
			return false;
		}

		codecCtx = avcodec_alloc_context3(codec);
		if (!codecCtx) {
			obs_log(LOG_ERROR, "Failed to allocate H.264 decoder context");
			return false;
		}

		frame = av_frame_alloc();
		packet = av_packet_alloc();

		if (!frame || !packet) {
			obs_log(LOG_ERROR, "Failed to allocate frame/packet");
			return false;
		}

		obs_log(LOG_DEBUG, "H.264 decoder initialized (codec context not opened yet)");
		return true;
	}

	bool initializeCodecContext()
	{
		if (codecInitialized) {
			return true;
		}

		if (!hasParameterSets) {
			obs_log(LOG_DEBUG, "Cannot initialize codec context without parameter sets");
			return false;
		}

		// Create extradata from SPS and PPS
		size_t extradata_size = sps.size() + pps.size() + 8; // 4 bytes start code each
		codecCtx->extradata = (uint8_t *)av_malloc(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
		codecCtx->extradata_size = (int)extradata_size;

		uint8_t *ptr = codecCtx->extradata;
		// Add SPS
		memcpy(ptr, "\x00\x00\x00\x01", 4);
		ptr += 4;
		memcpy(ptr, sps.data() + 4, sps.size() - 4); // Skip the start code we already added
		ptr += sps.size() - 4;

		// Add PPS
		memcpy(ptr, "\x00\x00\x00\x01", 4);
		ptr += 4;
		memcpy(ptr, pps.data() + 4, pps.size() - 4); // Skip the start code we already added

		// Zero padding
		memset(codecCtx->extradata + extradata_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

		// Open codec
		if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
			obs_log(LOG_ERROR, "Failed to open H.264 decoder");
			return false;
		}

		codecInitialized = true;
		obs_log(LOG_DEBUG, "H.264 codec context initialized successfully");
		return true;
	}

	void processRTPPacket(const uint8_t *data, int size) override
	{
		// Check if this is an RTP packet
		if (size < 12)
			return;

		uint8_t version = (data[0] >> 6) & 0x03;
		if (version != 2) {
			// Not RTP, might be raw H.264 data
			obs_log(LOG_DEBUG, "Received non-RTP data, attempting direct H.264 decode");

			// Check if it looks like H.264 (starts with 0x00 0x00 0x00 0x01)
			if (size >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) {
				std::vector<uint8_t> nalUnit(data, data + size);
				std::lock_guard<std::mutex> lock(nalMutex);
				processNALUnit(nalUnit);
			}
			return;
		}

		RTPHeader rtpHeader;
		rtpHeader.parseFromBuffer(data);

		// Check for RTCP packet (pt == 72)
		if (rtpHeader.pt == 72) {
			obs_log(LOG_DEBUG, "Received RTCP packet: seq=%u, ts=%u, ssrc=%u, size=%d", rtpHeader.seq, rtpHeader.ts, rtpHeader.ssrc, size);
			// Optionally, dump more RTCP statistics here
			return;
		}

		int headerSize = RTPHeader::getHeaderSize(data);
		if (size <= headerSize)
			return;

		const uint8_t *payload = data + headerSize;
		int payloadSize = size - headerSize;

		// Check for sequence number gaps
		if (!firstPacket) {
			uint16_t expectedSeq = lastSeq + 1;
			if (rtpHeader.seq != expectedSeq) {
				obs_log(LOG_DEBUG, "Sequence gap detected: expected %u, got %u", expectedSeq,
					rtpHeader.seq);
				fragmentBuffer.clear(); // Reset fragmentation buffer
			}
		}
		lastSeq = rtpHeader.seq;
		firstPacket = false;

		// Parse H.264 RTP payload (RFC 6184)
		if (payloadSize < 1)
			return;

		uint8_t nalHeader = payload[0];
		uint8_t nalType = nalHeader & 0x1F;

		obs_log(LOG_DEBUG, "H.264 NAL type: %d", nalType);

		if (nalType <= 23) {
			// Single NAL unit packet
			std::vector<uint8_t> nalUnit;
			nalUnit.insert(nalUnit.end(), {0x00, 0x00, 0x00, 0x01}); // Start code
			nalUnit.insert(nalUnit.end(), payload, payload + payloadSize);

			std::lock_guard<std::mutex> lock(nalMutex);
			processNALUnit(nalUnit);

		} else if (nalType == 28) {
			// FU-A (Fragmentation Unit Type A)
			if (payloadSize < 2)
				return;

			uint8_t fuHeader = payload[1];
			bool startBit = (fuHeader & 0x80) != 0;
			bool endBit = (fuHeader & 0x40) != 0;
			uint8_t nalTypeInFU = fuHeader & 0x1F;

			obs_log(LOG_DEBUG, "FU-A packet: start=%d, end=%d, nal_type=%d", startBit, endBit, nalTypeInFU);

			if (startBit) {
				fragmentBuffer.clear();
				fragmentBuffer.insert(fragmentBuffer.end(), {0x00, 0x00, 0x00, 0x01}); // Start code
				// Reconstruct NAL header
				uint8_t reconstructedNalHeader = (nalHeader & 0xE0) | nalTypeInFU;
				fragmentBuffer.push_back(reconstructedNalHeader);
				fragmentBuffer.insert(fragmentBuffer.end(), payload + 2, payload + payloadSize);
			} else if (!fragmentBuffer.empty()) {
				fragmentBuffer.insert(fragmentBuffer.end(), payload + 2, payload + payloadSize);
			}

			if (endBit && !fragmentBuffer.empty()) {
				std::lock_guard<std::mutex> lock(nalMutex);
				processNALUnit(fragmentBuffer);
				fragmentBuffer.clear();
			}
		} else {
			obs_log(LOG_DEBUG, "Unknown H.264 NAL type: %d", nalType);
		}
	}

private:
	void processNALUnit(const std::vector<uint8_t> &nalUnit)
	{
		if (nalUnit.size() < 5)
			return; // Must have start code + NAL header

		uint8_t nalType = nalUnit[4] & 0x1F; // NAL type after start code

		if ((nalType >= 11) && (nalType <= 23))
			return; // Ignore these NAL types

		// Handle parameter sets specially
		if (nalType == 7) { // SPS
			sps = nalUnit;
			obs_log(LOG_DEBUG, "Stored SPS, size: %zu", sps.size());
			checkParameterSets();
			return;
		} else if (nalType == 8) { // PPS
			pps = nalUnit;
			obs_log(LOG_DEBUG, "Stored PPS, size: %zu", pps.size());
			checkParameterSets();
			return;
		}

		// For other NAL units, only process if codec is initialized
		if (!codecInitialized) {
			obs_log(LOG_DEBUG, "Skipping NAL type %d - codec not initialized", nalType);
			return;
		}

		// Skip non-frame NAL units
		if (nalType == 6 || nalType == 9) { // SEI, AUD
			obs_log(LOG_DEBUG, "Skipping non-frame NAL type: %d", nalType);
			return;
		}

		// Send to decoder
		packet->data = (uint8_t *)nalUnit.data();
		packet->size = (int)nalUnit.size();

		obs_log(LOG_DEBUG, "Sending NAL type %d to decoder, size: %d", nalType, packet->size);

		int ret = avcodec_send_packet(codecCtx, packet);
		if (ret < 0) {
			char errbuf[256];
			av_strerror(ret, errbuf, sizeof(errbuf));
			obs_log(LOG_ERROR, "Error sending packet [%d] to H.264 decoder (%d): %s", nalType, ret, errbuf);
			return;
		}

		// Receive decoded frames
		while (ret >= 0) {
			ret = avcodec_receive_frame(codecCtx, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				char errbuf[256];
				av_strerror(ret, errbuf, sizeof(errbuf));
				obs_log(LOG_ERROR, "Error during H.264 decoding (%d): %s", ret, errbuf);
				break;
			}

			// Process decoded frame
			processDecodedFrame();
		}
	}

	void checkParameterSets()
	{
		if (!sps.empty() && !pps.empty() && !hasParameterSets) {
			hasParameterSets = true;
			obs_log(LOG_DEBUG, "Got both SPS and PPS, initializing codec context");
			initializeCodecContext();
		}
	}
	static video_colorspace prop_to_colorspace(AVColorSpace av)
	{
		switch (av) {
		case AVCOL_SPC_BT470BG:
			return VIDEO_CS_601;
		case AVCOL_SPC_ICTCP:
			return VIDEO_CS_2100_HLG;
		default:
		case AVCOL_SPC_BT709:
			return VIDEO_CS_709;
		}
	}

	static video_range_type prop_to_range_type(AVColorRange av)
	{
		switch (av) {
		case AVCOL_RANGE_JPEG:
			return VIDEO_RANGE_FULL;
		default:
		case AVCOL_RANGE_MPEG:
			return VIDEO_RANGE_PARTIAL;
		}
	}
	void processDecodedFrame()
	{
		obs_log(LOG_DEBUG, "Decoded H.264 frame: %dx%d format: %d", frame->width, frame->height, frame->format);

		if (!obs_source)
			return;

		// Convert to OBS video frame format
		obs_source_frame obs_frame = {};
		obs_frame.width = frame->width;
		obs_frame.height = frame->height;

		// Use current time if timestamp is not available
		obs_frame.timestamp = os_gettime_ns();

		// Convert YUV420P to appropriate format for OBS
		switch (frame->format) {
		case AV_PIX_FMT_YUV420P:
			obs_frame.format = VIDEO_FORMAT_I420;
			break;
		case AV_PIX_FMT_NV12:
			obs_frame.format = VIDEO_FORMAT_NV12;
			break;
		default:
			obs_frame.format = VIDEO_FORMAT_I420; // Default fallback
			break;
		}

		for (int i = 0; i < MAX_AV_PLANES && i < 3; i++) {
			obs_frame.data[i] = frame->data[i];
			obs_frame.linesize[i] = frame->linesize[i];
		}

		video_format_get_parameters(prop_to_colorspace(frame->colorspace),
					    prop_to_range_type(frame->color_range), obs_frame.color_matrix,
					    obs_frame.color_range_min, obs_frame.color_range_max);
		obs_source_output_video(obs_source, &obs_frame);
		obs_log(LOG_DEBUG, "Output video frame to OBS: %dx%d color: %d,%d", frame->width, frame->height,
			frame->colorspace, frame->color_range);
	}

public:
	void cleanup() override
	{
		if (codecCtx) {
			avcodec_free_context(&codecCtx);
		}
		if (frame) {
			av_frame_free(&frame);
		}
		if (packet) {
			av_packet_free(&packet);
		}
		if (swsCtx) {
			sws_freeContext(swsCtx);
		}

		sps.clear();
		pps.clear();
		hasParameterSets = false;
		codecInitialized = false;

		obs_log(LOG_DEBUG, "H.264 decoder cleaned up");
	}
};
// Opus Audio Decoder
class OpusDecoder : public MediaDecoder {
private:
	const AVCodec *codec;
	AVCodecContext *codecCtx;
	AVFrame *frame;
	AVPacket *packet;
	SwrContext *swrCtx;
	obs_source_t *obs_source;

	std::queue<std::vector<uint8_t>> audioQueue;
	std::mutex audioMutex;

public:
	OpusDecoder()
		: codec(nullptr),
		  codecCtx(nullptr),
		  frame(nullptr),
		  packet(nullptr),
		  swrCtx(nullptr),
		  obs_source(nullptr)
	{
	}

	~OpusDecoder() { cleanup(); }

	bool initialize(obs_source_t *source = nullptr) override
	{
		obs_source = source;

		// Find Opus decoder
		codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
		if (!codec) {
			obs_log(LOG_ERROR, "Opus decoder not found");
			return false;
		}

		codecCtx = avcodec_alloc_context3(codec);
		if (!codecCtx) {
			obs_log(LOG_ERROR, "Failed to allocate Opus decoder context");
			return false;
		}

		// Set Opus parameters
		codecCtx->sample_rate = 48000; // Opus native sample rate
		codecCtx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;

		if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
			obs_log(LOG_ERROR, "Failed to open Opus decoder");
			return false;
		}

		frame = av_frame_alloc();
		packet = av_packet_alloc();

		if (!frame || !packet) {
			obs_log(LOG_ERROR, "Failed to allocate audio frame/packet");
			return false;
		}

		// Initialize resampler for format conversion
		swrCtx = swr_alloc();
		if (!swrCtx) {
			obs_log(LOG_ERROR, "Failed to allocate resampler");
			return false;
		}

		// Set up resampler to convert to float planar
		av_opt_set_chlayout(swrCtx, "in_chlayout", &codecCtx->ch_layout, 0);
		av_opt_set_int(swrCtx, "in_sample_rate", codecCtx->sample_rate, 0);
		av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", codecCtx->sample_fmt, 0);

		AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
		av_opt_set_chlayout(swrCtx, "out_chlayout", &out_ch_layout, 0);
		av_opt_set_int(swrCtx, "out_sample_rate", 48000, 0);
		av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

		if (swr_init(swrCtx) < 0) {
			obs_log(LOG_ERROR, "Failed to initialize resampler");
			return false;
		}

		obs_log(LOG_DEBUG, "Opus decoder initialized");
		return true;
	}

	void processRTPPacket(const uint8_t *data, int size) override
	{
		// Check if this is an RTP packet
		if (size < 12)
			return;

		uint8_t version = (data[0] >> 6) & 0x03;
		if (version != 2) {
			// Not RTP, might be raw Opus data
			obs_log(LOG_DEBUG, "Received non-RTP data, attempting direct Opus decode");

			std::vector<uint8_t> opusPacket(data, data + size);
			std::lock_guard<std::mutex> lock(audioMutex);
			audioQueue.push(std::move(opusPacket));
			processAudioQueue();
			return;
		}

		RTPHeader rtpHeader;
		rtpHeader.parseFromBuffer(data);

		// Check for RTCP packet (pt == 72)
		if (rtpHeader.pt == 72) {
			obs_log(LOG_DEBUG, "Received RTCP packet: seq=%u, ts=%u, ssrc=%u, size=%d", rtpHeader.seq,
				rtpHeader.ts, rtpHeader.ssrc, size);
			// Optionally, dump more RTCP statistics here
			return;
		}

		int headerSize = RTPHeader::getHeaderSize(data);
		if (size <= headerSize)
			return;

		const uint8_t *payload = data + headerSize;
		int payloadSize = size - headerSize;

		// Log the actual payload being processed
		std::ostringstream payloadHex;
		payloadHex << std::hex << std::setfill('0');
		int dumpLen = std::min(16, payloadSize);
		for (int i = 0; i < dumpLen; ++i) {
			payloadHex << std::setw(2) << (int)payload[i] << " ";
		}
		obs_log(LOG_DEBUG, "Opus RTP payload size: %d, PT: %d, first %d bytes: %s", payloadSize, rtpHeader.pt,
			dumpLen, payloadHex.str().c_str());

		// Opus RTP payload is the raw Opus packet
		std::vector<uint8_t> opusPacket(payload, payload + payloadSize);

		std::lock_guard<std::mutex> lock(audioMutex);
		audioQueue.push(std::move(opusPacket));
		processAudioQueue();
	}

private:
	void processAudioQueue()
	{
		while (!audioQueue.empty()) {
			auto opusPacket = std::move(audioQueue.front());
			audioQueue.pop();

			// Send packet to decoder
			packet->data = opusPacket.data();
			packet->size = (int)opusPacket.size();

			int ret = avcodec_send_packet(codecCtx, packet);
			if (ret < 0) {
				char errbuf[256];
				av_strerror(ret, errbuf, sizeof(errbuf));
				obs_log(LOG_ERROR, "Error sending packet to Opus decoder (%d): %s", ret, errbuf);
				continue;
			}

			// Receive decoded frames
			while (ret >= 0) {
				ret = avcodec_receive_frame(codecCtx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					char errbuf[256];
					av_strerror(ret, errbuf, sizeof(errbuf));
					obs_log(LOG_ERROR, "Error during Opus decoding (%d): %s", ret, errbuf);
					break;
				}

				// Process decoded audio frame
				processDecodedAudioFrame();
			}
		}
	}

	void processDecodedAudioFrame()
	{
		obs_log(LOG_DEBUG, "Decoded Opus frame: %d samples, %d channels, %d Hz, format: %d", frame->nb_samples,
			frame->ch_layout.nb_channels, frame->sample_rate, frame->format);

		if (!obs_source)
			return;

		// Convert to float planar format for OBS
		uint8_t *output_buffer[8] = {nullptr};
		int output_samples = av_rescale_rnd(swr_get_delay(swrCtx, frame->sample_rate) + frame->nb_samples,
						    48000, frame->sample_rate, AV_ROUND_UP);

		av_samples_alloc(output_buffer, nullptr, 2, output_samples, AV_SAMPLE_FMT_FLTP, 0);

		int converted_samples = swr_convert(swrCtx, output_buffer, output_samples,
						    (const uint8_t **)frame->data, frame->nb_samples);

		if (converted_samples > 0) {
			const int channelCount = 2;

			obs_source_audio obs_audio_frame = {};

			obs_audio_frame.speakers = SPEAKERS_STEREO;

			//obs_audio_frame.timestamp = (uint64_t)(frame->best_effort_timestamp);
			obs_audio_frame.timestamp = os_gettime_ns();
			obs_audio_frame.samples_per_sec = frame->sample_rate;
			obs_audio_frame.format = AUDIO_FORMAT_FLOAT_PLANAR;
			obs_audio_frame.frames = frame->nb_samples;
			for (int i = 0; i < channelCount; ++i) {
				obs_audio_frame.data[i] = output_buffer[i];
			}

			obs_source_output_audio(obs_source, &obs_audio_frame);
		}

		av_freep(&output_buffer[0]);
	}

public:
	void cleanup() override
	{
		if (codecCtx) {
			avcodec_free_context(&codecCtx);
		}
		if (frame) {
			av_frame_free(&frame);
		}
		if (packet) {
			av_packet_free(&packet);
		}
		if (swrCtx) {
			swr_free(&swrCtx);
		}
		obs_log(LOG_DEBUG, "Opus decoder cleaned up");
	}
};

// Individual WHIP Receiver per endpoint
class WhipReceiver {
private:
	std::map<std::string, int> sessions;
	std::map<int, std::unique_ptr<MediaDecoder>> decoders;
	std::mutex sessionsMutex;
	std::mutex decodersMutex;
	obs_source_t *obs_source;
	std::string endpoint;

public:
	WhipReceiver(obs_source_t *source, const std::string &ep) : obs_source(source), endpoint(ep)
	{
		obs_log(LOG_DEBUG, "WHIP Receiver created for endpoint: %s", endpoint.c_str());
	}

	~WhipReceiver() { cleanup(); }

	void cleanup()
	{
		// Thread-safe cleanup of all decoders
		{
			std::lock_guard<std::mutex> lock(decodersMutex);
			for (auto &[trackId, decoder] : decoders) {
				decoder->cleanup();
			}
			decoders.clear();
		}

		// Thread-safe cleanup of all sessions
		{
			std::lock_guard<std::mutex> lock(sessionsMutex);
			for (auto &[sessionId, pc] : sessions) {
				rtcDeletePeerConnection(pc);
			}
			sessions.clear();
		}
		obs_log(LOG_DEBUG, "WHIP Receiver cleaned up for endpoint: %s", endpoint.c_str());
	}

	// Static callback functions
	static void logCallback(rtcLogLevel level, const char *message)
	{
		const char *levelStr = "UNKNOWN";
		switch (level) {
		case RTC_LOG_FATAL:
			levelStr = "FATAL";
			break;
		case RTC_LOG_ERROR:
			levelStr = "ERROR";
			break;
		case RTC_LOG_WARNING:
			levelStr = "WARNING";
			break;
		case RTC_LOG_INFO:
			levelStr = "INFO";
			break;
		case RTC_LOG_DEBUG:
			levelStr = "DEBUG";
			break;
		case RTC_LOG_VERBOSE:
			levelStr = "VERBOSE";
			break;
		}
		obs_log(LOG_DEBUG, "[%s] %s", levelStr, message);
	}

	static void onLocalDescription(int pc, const char *sdp, const char *type, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		obs_log(LOG_DEBUG, "Local description generated (%s) for endpoint %s:", type,
			receiver->endpoint.c_str());
		obs_log(LOG_DEBUG, "%s", sdp);
	}

	static void onLocalCandidate(int pc, const char *cand, const char *mid, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		obs_log(LOG_DEBUG, "Local candidate for %s: %s (mid: %s)", receiver->endpoint.c_str(), cand,
			mid ? mid : "null");
	}

	static void onStateChange(int pc, rtcState state, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		const char *stateStr = "UNKNOWN";
		switch (state) {
		case RTC_CONNECTING:
			stateStr = "CONNECTING";
			break;
		case RTC_CONNECTED:
			stateStr = "CONNECTED";
			break;
		case RTC_DISCONNECTED:
			stateStr = "DISCONNECTED";
			break;
		case RTC_FAILED:
			stateStr = "FAILED";
			break;
		case RTC_CLOSED:
			stateStr = "CLOSED";
			break;
		}
		obs_log(LOG_DEBUG, "PeerConnection state changed for %s: %s", receiver->endpoint.c_str(), stateStr);
	}

	static void onGatheringStateChange(int pc, rtcGatheringState state, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		const char *stateStr = (state == RTC_GATHERING_COMPLETE) ? "COMPLETE" : "IN_PROGRESS";
		obs_log(LOG_DEBUG, "ICE gathering state for %s: %s", receiver->endpoint.c_str(), stateStr);
	}

	static void onTrackOpen(int tr, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		obs_log(LOG_DEBUG, "Track %d opened for endpoint %s, ready to receive media!", tr,
			receiver->endpoint.c_str());
	}

	// Create a new PeerConnection for WHIP session - THREAD SAFE VERSION
	int createPeerConnection(const std::string &sessionId)
	{
		rtcConfiguration config = {};

		// Add STUN servers for NAT traversal
		const char *stunServers[] = {"stun:stun.l.google.com:19302", "stun:stun1.l.google.com:19302"};
		config.iceServers = stunServers;
		config.iceServersCount = 2;

		// Enable media transport for receiving tracks
		config.forceMediaTransport = true;

		int pc = rtcCreatePeerConnection(&config);
		if (pc < 0) {
			obs_log(LOG_ERROR, "Failed to create PeerConnection: %d", pc);
			return pc;
		}

		// Set user pointer and callbacks
		rtcSetUserPointer(pc, this);
		rtcSetLocalDescriptionCallback(pc, onLocalDescription);
		rtcSetLocalCandidateCallback(pc, onLocalCandidate);
		rtcSetStateChangeCallback(pc, onStateChange);
		rtcSetGatheringStateChangeCallback(pc, onGatheringStateChange);
		rtcSetTrackCallback(pc, onTrack);

		// Thread-safe access to sessions map
		{
			std::lock_guard<std::mutex> lock(sessionsMutex);
			sessions[sessionId] = pc;
		}

		obs_log(LOG_DEBUG, "Created PeerConnection for session: %s on endpoint: %s", sessionId.c_str(),
			endpoint.c_str());
		return pc;
	}

	// Process WHIP offer and generate answer - THREAD SAFE VERSION
	std::string processWhipOffer(const std::string &sessionId, const std::string &sdpOffer)
	{
		int pc = createPeerConnection(sessionId);
		if (pc < 0) {
			return "";
		}

		obs_log(LOG_DEBUG, "Processing SDP offer for endpoint %s:", endpoint.c_str());
		obs_log(LOG_DEBUG, "%s", sdpOffer.c_str());

		// Set remote description (the offer)
		int result = rtcSetRemoteDescription(pc, sdpOffer.c_str(), "offer");
		if (result != RTC_ERR_SUCCESS) {
			obs_log(LOG_ERROR, "Failed to set remote description: %d", result);
			// Clean up the session since it failed
			rtcDeletePeerConnection(pc);
			{
				std::lock_guard<std::mutex> lock(sessionsMutex);
				sessions.erase(sessionId);
			}
			return "";
		}

		// Wait a bit for ICE gathering (in production, you'd handle this asynchronously)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		// Get the generated answer
		char answerBuffer[8192];
		int answerSize = rtcGetLocalDescription(pc, answerBuffer, sizeof(answerBuffer));
		if (answerSize < 0) {
			obs_log(LOG_ERROR, "Failed to get local description: %d", answerSize);
			// Clean up the session since it failed
			rtcDeletePeerConnection(pc);
			{
				std::lock_guard<std::mutex> lock(sessionsMutex);
				sessions.erase(sessionId);
			}
			return "";
		}

		std::string sdpAnswer(answerBuffer);
		obs_log(LOG_DEBUG, "Generated SDP answer for endpoint %s:", endpoint.c_str());
		obs_log(LOG_DEBUG, "%s", sdpAnswer.c_str());

		return sdpAnswer;
	}

	// Thread-safe track callback
	static void onTrack(int pc, int tr, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		obs_log(LOG_DEBUG, "New track received on endpoint %s!", receiver->endpoint.c_str());

		// Get track information
		char buffer[1024];
		std::string mediaType;
		std::string codecName;

		if (rtcGetTrackDescription(tr, buffer, sizeof(buffer)) >= 0) {
			obs_log(LOG_DEBUG, "Track description for %s: %s", receiver->endpoint.c_str(), buffer);
			std::string desc(buffer);

			// Parse media type and codec from SDP
			if (desc.find("m=video") != std::string::npos) {
				mediaType = "video";
				if (desc.find("H264") != std::string::npos || desc.find("h264") != std::string::npos) {
					codecName = "h264";
				}
			} else if (desc.find("m=audio") != std::string::npos) {
				mediaType = "audio";
				if (desc.find("opus") != std::string::npos || desc.find("OPUS") != std::string::npos) {
					codecName = "opus";
				}
			}
		}

		// Create appropriate decoder
		std::unique_ptr<MediaDecoder> decoder;
		if (mediaType == "video" && codecName == "h264") {
			decoder = std::make_unique<H264Decoder>();
			obs_log(LOG_DEBUG, "Created H.264 decoder for track on endpoint %s",
				receiver->endpoint.c_str());
		} else if (mediaType == "audio" && codecName == "opus") {
			decoder = std::make_unique<OpusDecoder>();
			obs_log(LOG_DEBUG, "Created Opus decoder for track on endpoint %s", receiver->endpoint.c_str());
		}

		if (decoder && decoder->initialize(receiver->obs_source)) {
			std::lock_guard<std::mutex> lock(receiver->decodersMutex);
			receiver->decoders[tr] = std::move(decoder);
		}

		// Set up track callbacks
		rtcSetUserPointer(tr, user_ptr);
		rtcSetOpenCallback(tr, onTrackOpen);
		rtcSetMessageCallback(tr, onTrackMessage);
		rtcSetClosedCallback(tr, onTrackClosed);
	}

	static void onTrackMessage(int tr, const char *message, int size, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);

		// Check if this is actually an RTP packet (should start with version 2)
		if (size < 12) {
			obs_log(LOG_DEBUG, "Packet too small to be RTP: %d bytes", size);
			return;
		}

		const uint8_t *data = reinterpret_cast<const uint8_t *>(message);

		// Check RTP version (should be 2)
		uint8_t version = (data[0] >> 6) & 0x03;
		if (version != 2) {
			obs_log(LOG_DEBUG, "Not an RTP packet (version %d), passing directly to decoder", version);

			// This might be raw codec data, try to process it directly
			std::lock_guard<std::mutex> lock(receiver->decodersMutex);
			auto it = receiver->decoders.find(tr);
			if (it != receiver->decoders.end()) {
				// For non-RTP data, we might need different processing
				// This is a fallback - ideally we should receive RTP
				it->second->processRTPPacket(data, size);
			}
			return;
		}

		// Parse RTP header to get payload type
		RTPHeader rtpHeader;
		rtpHeader.parseFromBuffer(data);

		obs_log(LOG_DEBUG, "RTP packet on %s: PT=%d, seq=%d, ts=%d, size=%d", receiver->endpoint.c_str(),
			rtpHeader.pt, rtpHeader.seq, rtpHeader.ts, size);

		std::lock_guard<std::mutex> lock(receiver->decodersMutex);
		auto it = receiver->decoders.find(tr);
		if (it != receiver->decoders.end()) {
			// Process RTP packet through appropriate decoder
			it->second->processRTPPacket(data, size);
		} else {
			obs_log(LOG_DEBUG, "Received RTP packet of size: %d bytes (no decoder) on %s", size,
				receiver->endpoint.c_str());
		}
	}

	static void onTrackClosed(int tr, void *user_ptr)
	{
		WhipReceiver *receiver = static_cast<WhipReceiver *>(user_ptr);
		obs_log(LOG_DEBUG, "Track %d closed on endpoint %s", tr, receiver->endpoint.c_str());

		// Thread-safe cleanup of decoder
		std::lock_guard<std::mutex> lock(receiver->decodersMutex);
		auto it = receiver->decoders.find(tr);
		if (it != receiver->decoders.end()) {
			it->second->cleanup();
			receiver->decoders.erase(it);
		}
	}

	std::string generateSessionId()
	{
		static int counter = 0;
		return "session_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
	}
};

// Global HTTP Server that handles all endpoints
class WhipHttpServer {
private:
	int serverSocket;
	bool running;
	std::thread serverThread;
	std::map<std::string, WhipReceiver *> endpointMap;
	std::mutex endpointMapMutex;
	static WhipHttpServer *instance;

public:
	WhipHttpServer() : serverSocket(-1), running(false) {}

	~WhipHttpServer() { stop(); }

	static WhipHttpServer *getInstance()
	{
		if (!instance) {
			instance = new WhipHttpServer();
		}
		return instance;
	}

	static void destroyInstance()
	{
		if (instance) {
			delete instance;
			instance = nullptr;
		}
	}

	void registerEndpoint(const std::string &endpoint, WhipReceiver *receiver)
	{
		std::lock_guard<std::mutex> lock(endpointMapMutex);
		endpointMap[endpoint] = receiver;
		obs_log(LOG_DEBUG, "Registered endpoint: %s", endpoint.c_str());
	}

	void unregisterEndpoint(const std::string &endpoint)
	{
		std::lock_guard<std::mutex> lock(endpointMapMutex);
		auto it = endpointMap.find(endpoint);
		if (it != endpointMap.end()) {
			endpointMap.erase(it);
			obs_log(LOG_DEBUG, "Unregistered endpoint: %s", endpoint.c_str());
		}
	}

	bool start(int port = 8080)
	{
		if (running) {
			return true; // Already running
		}

#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			obs_log(LOG_ERROR, "WSAStartup failed");
			return false;
		}
#endif

		serverSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSocket < 0) {
			obs_log(LOG_ERROR, "Failed to create socket");
			return false;
		}

		int opt = 1;
		setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
			obs_log(LOG_ERROR, "Bind failed");
			return false;
		}

		if (listen(serverSocket, 10) < 0) {
			obs_log(LOG_ERROR, "Listen failed");
			return false;
		}

		running = true;
		obs_log(LOG_DEBUG, "WHIP HTTP server listening on port %d", port);

		serverThread = std::thread(&WhipHttpServer::serverLoop, this);
		return true;
	}

	void stop()
	{
		running = false;

		if (serverSocket >= 0) {
#ifdef _WIN32
			closesocket(serverSocket);
			WSACleanup();
#else
			close(serverSocket);
#endif
			serverSocket = -1;
		}

		if (serverThread.joinable()) {
			serverThread.join();
		}

		// Clear endpoint map
		{
			std::lock_guard<std::mutex> lock(endpointMapMutex);
			endpointMap.clear();
		}

		obs_log(LOG_DEBUG, "WHIP HTTP server stopped");
	}

private:
	void serverLoop()
	{
		while (running) {
			struct sockaddr_in clientAddr;
			socklen_t clientLen = sizeof(clientAddr);

			int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
			if (clientSocket < 0) {
				if (running) {
					obs_log(LOG_ERROR, "Accept failed");
				}
				continue;
			}

			// Handle client in separate thread
			std::thread clientThread(&WhipHttpServer::handleClient, this, clientSocket);
			clientThread.detach();
		}
	}

	void handleClient(int clientSocket)
	{
		char buffer[8192];
		int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
		if (bytesRead <= 0) {
#ifdef _WIN32
			closesocket(clientSocket);
#else
			close(clientSocket);
#endif
			return;
		}

		buffer[bytesRead] = '\0';
		std::string request(buffer);

		obs_log(LOG_DEBUG, "Received HTTP request:");
		obs_log(LOG_DEBUG, "%s...", request.substr(0, 200).c_str());

		// Parse HTTP method and path
		std::string method, path;
		std::istringstream iss(request);
		iss >> method >> path;

		// Find the receiver for this endpoint
		WhipReceiver *receiver = nullptr;
		{
			std::lock_guard<std::mutex> lock(endpointMapMutex);
			auto it = endpointMap.find(path);
			if (it != endpointMap.end()) {
				receiver = it->second;
			}
		}

		if (method == "POST" && receiver) {
			handleWhipPost(clientSocket, request, path, receiver);
		} else if (method == "OPTIONS" && receiver) {
			handleOptions(clientSocket);
		} else {
			sendHttpResponse(clientSocket, 404, "text/plain", "Not Found");
		}

#ifdef _WIN32
		closesocket(clientSocket);
#else
		close(clientSocket);
#endif
	}

	void handleWhipPost(int clientSocket, const std::string &request, const std::string &path,
			    WhipReceiver *receiver)
	{
		// Extract Content-Type
		if (request.find("Content-Type: application/sdp") == std::string::npos) {
			sendHttpResponse(clientSocket, 400, "text/plain", "Content-Type must be application/sdp");
			return;
		}

		// Extract SDP from request body
		size_t bodyStart = request.find("\r\n\r\n");
		if (bodyStart == std::string::npos) {
			sendHttpResponse(clientSocket, 400, "text/plain", "Invalid HTTP request");
			return;
		}
		bodyStart += 4;

		std::string sdpOffer = request.substr(bodyStart);

		// Generate session ID
		std::string sessionId = receiver->generateSessionId();

		// Process the offer
		std::string sdpAnswer = receiver->processWhipOffer(sessionId, sdpOffer);
		if (sdpAnswer.empty()) {
			sendHttpResponse(clientSocket, 500, "text/plain", "Failed to process offer");
			return;
		}

		// Send response with Location header
		std::string locationHeader = "Location: " + path + "/sessions/" + sessionId + "\r\n";
		sendHttpResponse(clientSocket, 201, "application/sdp", sdpAnswer, locationHeader);
	}

	void handleOptions(int clientSocket)
	{
		std::string headers = "Access-Control-Allow-Origin: *\r\n"
				      "Access-Control-Allow-Methods: POST, PATCH, DELETE\r\n"
				      "Access-Control-Allow-Headers: Content-Type\r\n";
		sendHttpResponse(clientSocket, 204, "", "", headers);
	}

	void sendHttpResponse(int clientSocket, int statusCode, const std::string &contentType, const std::string &body,
			      const std::string &additionalHeaders = "")
	{
		std::string status;
		switch (statusCode) {
		case 200:
			status = "200 OK";
			break;
		case 201:
			status = "201 Created";
			break;
		case 204:
			status = "204 No Content";
			break;
		case 400:
			status = "400 Bad Request";
			break;
		case 404:
			status = "404 Not Found";
			break;
		case 500:
			status = "500 Internal Server Error";
			break;
		default:
			status = "500 Internal Server Error";
			break;
		}

		std::string response = "HTTP/1.1 " + status + "\r\n";
		if (!contentType.empty()) {
			response += "Content-Type: " + contentType + "\r\n";
		}
		response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
		response += "Access-Control-Allow-Origin: *\r\n";
		response += additionalHeaders;
		response += "\r\n";
		response += body;

		send(clientSocket, response.c_str(), (int)response.length(), 0);
	}
};

// Initialize static member
WhipHttpServer *WhipHttpServer::instance = nullptr;

#define PROP_SOURCE "source_name"
#define PROP_URL "url"

const char *whip_source_getname(void *)
{
	return "WHIP";
}

obs_properties_t *whip_source_getproperties(void *data)
{
	auto s = (whip_source_t *)data;
	obs_log(LOG_DEBUG, "+whip_source_getproperties(…)");

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, PROP_URL, "WHIP Endpoint", OBS_TEXT_DEFAULT);

	return props;
}

void whip_source_getdefaults(obs_data_t *settings)
{
	obs_log(LOG_DEBUG, "+whip_source_getdefaults(…)");
	obs_data_set_default_string(settings, PROP_URL, "/whip");
	obs_log(LOG_DEBUG, "-whip_source_getdefaults(…)");
}

void whip_source_update(void *data, obs_data_t *settings)
{
	auto s = (whip_source_t *)data;
	auto obs_source = s->obs_source;
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_update(…)", obs_source_name);

	auto url = std::string(obs_data_get_string(settings, PROP_URL));

	// If endpoint changed, unregister old, destroy receiver, and create new
	if (s->endpoint != url) {
		if (!s->endpoint.empty()) {
			WhipHttpServer::getInstance()->unregisterEndpoint(s->endpoint);
		}
		// Destroy old receiver
		if (s->receiver) {
			s->receiver.reset();
		}

		s->endpoint = std::move(url);
		s->receiver = std::make_unique<WhipReceiver>(obs_source, s->endpoint);
		WhipHttpServer::getInstance()->registerEndpoint(s->endpoint, s->receiver.get());
	}

	obs_log(LOG_DEBUG, "'%s' -whip_source_update(…)", obs_source_name);
}

void whip_source_shown(void *data) {}

void whip_source_hidden(void *data) {}

void whip_source_activated(void *data) {}

void whip_source_deactivated(void *data) {}

void *whip_source_create(obs_data_t *settings, obs_source_t *obs_source)
{
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_create(…)", obs_source_name);

	// Initialize FFmpeg (only once globally)
	static std::once_flag ffmpeg_init_flag;
	std::call_once(ffmpeg_init_flag, []() {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
		av_register_all();
#endif
		// Initialize libdatachannel logging
		rtcInitLogger(RTC_LOG_DEBUG, WhipReceiver::logCallback);
		// Preload resources
		rtcPreload();
		obs_log(LOG_DEBUG, "FFmpeg and libdatachannel initialized");
	});

	auto s = (whip_source_t *)bzalloc(sizeof(whip_source_t));
	s->endpoint = "";
	s->obs_source = obs_source;

	// Start HTTP server if not already running
	WhipHttpServer::getInstance()->start(8080);

	whip_source_update(s, settings);

	obs_log(LOG_DEBUG, "'%s' -whip_source_create(…)", obs_source_name);

	return s;
}

void whip_source_destroy(void *data)
{
	auto s = (whip_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_destroy(…)", obs_source_name);

	// Unregister endpoint
	if (!s->endpoint.empty()) {
		WhipHttpServer::getInstance()->unregisterEndpoint(s->endpoint);
	}

	// Reset receiver (will call destructor)
	s->receiver.reset();

	bfree(s);

	obs_log(LOG_DEBUG, "'%s' -whip_source_destroy(…)", obs_source_name);
}

uint32_t whip_source_get_width(void *data)
{
	auto s = (whip_source_t *)data;
	return 1280;
}

uint32_t whip_source_get_height(void *data)
{
	auto s = (whip_source_t *)data;
	return 720;
}

obs_source_info create_whip_source_info()
{
	// https://docs.obsproject.com/reference-sources#source-definition-structure-obs-source-info
	obs_source_info whip_source_info = {};
	whip_source_info.id = "whip_source";
	whip_source_info.type = OBS_SOURCE_TYPE_INPUT;
	whip_source_info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;

	whip_source_info.get_name = whip_source_getname;
	whip_source_info.get_properties = whip_source_getproperties;
	whip_source_info.get_defaults = whip_source_getdefaults;

	whip_source_info.create = whip_source_create;
	whip_source_info.activate = whip_source_activated;
	whip_source_info.show = whip_source_shown;
	whip_source_info.update = whip_source_update;
	whip_source_info.hide = whip_source_hidden;
	whip_source_info.deactivate = whip_source_deactivated;
	whip_source_info.destroy = whip_source_destroy;

	whip_source_info.get_width = whip_source_get_width;
	whip_source_info.get_height = whip_source_get_height;

	return whip_source_info;
}
