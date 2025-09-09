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
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// RTP header structure
struct RTPHeader {
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t extension:1;
    uint8_t cc:4;
    uint8_t marker:1;
    uint8_t pt:7;
    uint16_t seq;
    uint32_t ts;
    uint32_t ssrc;
    
    void parseFromBuffer(const uint8_t* buffer) {
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
    
    static int getHeaderSize(const uint8_t* buffer) {
        uint8_t cc = buffer[0] & 0x0F;
        return 12 + (cc * 4); // Basic header + CSRC list
    }
};

// Media decoder base class
class MediaDecoder {
public:
    virtual ~MediaDecoder() = default;
    virtual bool initialize() = 0;
    virtual void processRTPPacket(const uint8_t* data, int size) = 0;
    virtual void cleanup() = 0;
};

// H.264 Video Decoder
class H264Decoder : public MediaDecoder {
private:
    const AVCodec* codec;
    AVCodecContext* codecCtx;
    AVFrame* frame;
    AVPacket* packet;
    SwsContext* swsCtx;
    FILE* outputFile;
    
    std::queue<std::vector<uint8_t>> nalQueue;
    std::mutex nalMutex;
    std::vector<uint8_t> fragmentBuffer;
    uint16_t lastSeq;
    bool firstPacket;
    
public:
    H264Decoder() : codec(nullptr), codecCtx(nullptr), frame(nullptr), 
                   packet(nullptr), swsCtx(nullptr), outputFile(nullptr),
                   lastSeq(0), firstPacket(true) {}
    
    ~H264Decoder() {
        cleanup();
    }
    
    bool initialize() override {
        // Find H.264 decoder
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            std::cerr << "H.264 decoder not found" << std::endl;
            return false;
        }
        
        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            std::cerr << "Failed to allocate H.264 decoder context" << std::endl;
            return false;
        }
        
        // Open codec
        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            std::cerr << "Failed to open H.264 decoder" << std::endl;
            return false;
        }
        
        frame = av_frame_alloc();
        packet = av_packet_alloc();
        
        if (!frame || !packet) {
            std::cerr << "Failed to allocate frame/packet" << std::endl;
            return false;
        }
        
        // Open output file for decoded frames (YUV format)
        outputFile = fopen("decoded_video.yuv", "wb");
        
        std::cout << "H.264 decoder initialized" << std::endl;
        return true;
    }
    
    void processRTPPacket(const uint8_t* data, int size) override {
        RTPHeader rtpHeader;
        rtpHeader.parseFromBuffer(data);
        
        int headerSize = RTPHeader::getHeaderSize(data);
        if (size <= headerSize) return;
        
        const uint8_t* payload = data + headerSize;
        int payloadSize = size - headerSize;
        
        // Check for sequence number gaps
        if (!firstPacket) {
            uint16_t expectedSeq = lastSeq + 1;
            if (rtpHeader.seq != expectedSeq) {
                std::cout << "Sequence gap detected: expected " << expectedSeq 
                         << ", got " << rtpHeader.seq << std::endl;
                fragmentBuffer.clear(); // Reset fragmentation buffer
            }
        }
        lastSeq = rtpHeader.seq;
        firstPacket = false;
        
        // Parse H.264 RTP payload (RFC 6184)
        if (payloadSize < 1) return;
        
        uint8_t nalHeader = payload[0];
        uint8_t nalType = nalHeader & 0x1F;
        
        if (nalType <= 23) {
            // Single NAL unit packet
            std::vector<uint8_t> nalUnit;
            nalUnit.insert(nalUnit.end(), {0x00, 0x00, 0x00, 0x01}); // Start code
            nalUnit.insert(nalUnit.end(), payload, payload + payloadSize);
            
            std::lock_guard<std::mutex> lock(nalMutex);
            nalQueue.push(std::move(nalUnit));
            processNALQueue();
            
        } else if (nalType == 28) {
            // FU-A (Fragmentation Unit Type A)
            if (payloadSize < 2) return;
            
            uint8_t fuHeader = payload[1];
            bool startBit = (fuHeader & 0x80) != 0;
            bool endBit = (fuHeader & 0x40) != 0;
            uint8_t nalTypeInFU = fuHeader & 0x1F;
            
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
                nalQueue.push(fragmentBuffer);
                fragmentBuffer.clear();
                processNALQueue();
            }
        }
    }
    
private:
    void processNALQueue() {
        while (!nalQueue.empty()) {
            auto nalUnit = std::move(nalQueue.front());
            nalQueue.pop();
            
            // Send NAL unit to decoder
            packet->data = nalUnit.data();
            packet->size = (int)nalUnit.size();
            
            int ret = avcodec_send_packet(codecCtx, packet);
            if (ret < 0) {
                std::cerr << "Error sending packet to H.264 decoder" << std::endl;
                continue;
            }
            
            // Receive decoded frames
            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error during H.264 decoding" << std::endl;
                    break;
                }
                
                // Process decoded frame
                processDecodedFrame();
            }
        }
    }
    
    void processDecodedFrame() {
        std::cout << "Decoded H.264 frame: " << frame->width << "x" << frame->height 
                 << " format: " << frame->format << std::endl;
        
        // Write YUV data to file
        if (outputFile) {
            // Write Y plane
            for (int y = 0; y < frame->height; y++) {
                fwrite(frame->data[0] + y * frame->linesize[0], 1, frame->width, outputFile);
            }
            // Write U plane
            for (int y = 0; y < frame->height / 2; y++) {
                fwrite(frame->data[1] + y * frame->linesize[1], 1, frame->width / 2, outputFile);
            }
            // Write V plane
            for (int y = 0; y < frame->height / 2; y++) {
                fwrite(frame->data[2] + y * frame->linesize[2], 1, frame->width / 2, outputFile);
            }
            fflush(outputFile);
        }
        
        // Here you could also:
        // 1. Convert to RGB using swscale
        // 2. Display using OpenCV/SDL
        // 3. Save as PNG/JPEG
        // 4. Stream to another destination
    }
    
public:
    void cleanup() override {
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
        if (outputFile) {
            fclose(outputFile);
            outputFile = nullptr;
        }
        std::cout << "H.264 decoder cleaned up" << std::endl;
    }
};

// Opus Audio Decoder
class OpusDecoder : public MediaDecoder {
private:
    const AVCodec* codec;
    AVCodecContext* codecCtx;
    AVFrame* frame;
    AVPacket* packet;
    SwrContext* swrCtx;
    FILE* outputFile;
    
    std::queue<std::vector<uint8_t>> audioQueue;
    std::mutex audioMutex;
    
public:
    OpusDecoder() : codec(nullptr), codecCtx(nullptr), frame(nullptr),
                   packet(nullptr), swrCtx(nullptr), outputFile(nullptr) {}
    
    ~OpusDecoder() {
        cleanup();
    }
    
    bool initialize() override {
        // Find Opus decoder
        codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
        if (!codec) {
            std::cerr << "Opus decoder not found" << std::endl;
            return false;
        }
        
        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            std::cerr << "Failed to allocate Opus decoder context" << std::endl;
            return false;
        }
        
        // Set Opus parameters
        codecCtx->sample_rate = 48000; // Opus native sample rate
	    codecCtx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        
        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            std::cerr << "Failed to open Opus decoder" << std::endl;
            return false;
        }
        
        frame = av_frame_alloc();
        packet = av_packet_alloc();
        
        if (!frame || !packet) {
            std::cerr << "Failed to allocate audio frame/packet" << std::endl;
            return false;
        }
        
        // Open output file for decoded audio (PCM format)
        outputFile = fopen("decoded_audio.pcm", "wb");
        
        std::cout << "Opus decoder initialized" << std::endl;
        return true;
    }
    
    void processRTPPacket(const uint8_t* data, int size) override {
        RTPHeader rtpHeader;
        rtpHeader.parseFromBuffer(data);
        
        int headerSize = RTPHeader::getHeaderSize(data);
        if (size <= headerSize) return;
        
        const uint8_t* payload = data + headerSize;
        int payloadSize = size - headerSize;
        
        // Opus RTP payload is the raw Opus packet
        std::vector<uint8_t> opusPacket(payload, payload + payloadSize);
        
        std::lock_guard<std::mutex> lock(audioMutex);
        audioQueue.push(std::move(opusPacket));
        processAudioQueue();
    }
    
private:
    void processAudioQueue() {
        while (!audioQueue.empty()) {
            auto opusPacket = std::move(audioQueue.front());
            audioQueue.pop();
            
            // Send packet to decoder
            packet->data = opusPacket.data();
            packet->size = (int)opusPacket.size();
            
            int ret = avcodec_send_packet(codecCtx, packet);
            if (ret < 0) {
                std::cerr << "Error sending packet to Opus decoder" << std::endl;
                continue;
            }
            
            // Receive decoded frames
            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error during Opus decoding" << std::endl;
                    break;
                }
                
                // Process decoded audio frame
                processDecodedAudioFrame();
            }
        }
    }
    
    void processDecodedAudioFrame() {
        std::cout << "Decoded Opus frame: " << frame->nb_samples << " samples, "
                 << frame->ch_layout.nb_channels << " channels, " << frame->sample_rate << " Hz" << std::endl;
        
        // Write PCM data to file
        if (outputFile) {
            int data_size = av_get_bytes_per_sample((AVSampleFormat)frame->format);
            for (int i = 0; i < frame->nb_samples; i++) {
		    for (int ch = 0; ch < frame->ch_layout.nb_channels; ch++) {
                    fwrite(frame->data[ch] + data_size * i, 1, data_size, outputFile);
                }
            }
            fflush(outputFile);
        }
        
        // Here you could also:
        // 1. Resample to different sample rate
        // 2. Play through audio device
        // 3. Apply audio effects
        // 4. Stream to another destination
    }
    
public:
    void cleanup() override {
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
        if (outputFile) {
            fclose(outputFile);
            outputFile = nullptr;
        }
        std::cout << "Opus decoder cleaned up" << std::endl;
    }
};

class WhipReceiver {
private:
    std::map<std::string, int> sessions; // sessionId -> peerConnection
    std::map<int, std::unique_ptr<MediaDecoder>> decoders; // track -> decoder
    int serverSocket;
    bool running;
    std::thread serverThread;

public:
    WhipReceiver() : serverSocket(-1), running(false) {
        // Initialize FFmpeg
        #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
        #endif
        
        // Initialize libdatachannel logging
        rtcInitLogger(RTC_LOG_DEBUG, logCallback);
        
        // Preload resources
        rtcPreload();
        
        std::cout << "WHIP Receiver with H.264/Opus decoding initialized" << std::endl;
    }
    
    ~WhipReceiver() {
        stop();
        rtcCleanup();
    }

    // Static callback functions
    static void logCallback(rtcLogLevel level, const char *message) {
        const char* levelStr = "UNKNOWN";
        switch(level) {
            case RTC_LOG_FATAL: levelStr = "FATAL"; break;
            case RTC_LOG_ERROR: levelStr = "ERROR"; break;
            case RTC_LOG_WARNING: levelStr = "WARNING"; break;
            case RTC_LOG_INFO: levelStr = "INFO"; break;
            case RTC_LOG_DEBUG: levelStr = "DEBUG"; break;
            case RTC_LOG_VERBOSE: levelStr = "VERBOSE"; break;
        }
        std::cout << "[" << levelStr << "] " << message << std::endl;
    }

    static void onLocalDescription(int pc, const char *sdp, const char *type, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        std::cout << "Local description generated (" << type << "):" << std::endl;
        std::cout << sdp << std::endl;
    }

    static void onLocalCandidate(int pc, const char *cand, const char *mid, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        std::cout << "Local candidate: " << cand << " (mid: " << (mid ? mid : "null") << ")" << std::endl;
    }

    static void onStateChange(int pc, rtcState state, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        const char* stateStr = "UNKNOWN";
        switch(state) {
            case RTC_CONNECTING: stateStr = "CONNECTING"; break;
            case RTC_CONNECTED: stateStr = "CONNECTED"; break;
            case RTC_DISCONNECTED: stateStr = "DISCONNECTED"; break;
            case RTC_FAILED: stateStr = "FAILED"; break;
            case RTC_CLOSED: stateStr = "CLOSED"; break;
        }
        std::cout << "PeerConnection state changed: " << stateStr << std::endl;
    }

    static void onGatheringStateChange(int pc, rtcGatheringState state, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        const char* stateStr = (state == RTC_GATHERING_COMPLETE) ? "COMPLETE" : "IN_PROGRESS";
        std::cout << "ICE gathering state: " << stateStr << std::endl;
    }

    static void onTrack(int pc, int tr, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        std::cout << "New track received!" << std::endl;
        
        // Get track information
        char buffer[1024];
        std::string mediaType;
        std::string codecName;
        
        if (rtcGetTrackDescription(tr, buffer, sizeof(buffer)) >= 0) {
            std::cout << "Track description: " << buffer << std::endl;
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
            std::cout << "Created H.264 decoder for track" << std::endl;
        } else if (mediaType == "audio" && codecName == "opus") {
            decoder = std::make_unique<OpusDecoder>();
            std::cout << "Created Opus decoder for track" << std::endl;
        }
        
        if (decoder && decoder->initialize()) {
            receiver->decoders[tr] = std::move(decoder);
        }
        
        // Set up track callbacks
        rtcSetUserPointer(tr, user_ptr);
        rtcSetOpenCallback(tr, onTrackOpen);
        rtcSetMessageCallback(tr, onTrackMessage);
        rtcSetClosedCallback(tr, onTrackClosed);
    }

    static void onTrackOpen(int tr, void *user_ptr) {
        std::cout << "Track " << tr << " opened, ready to receive media!" << std::endl;
    }

    static void onTrackMessage(int tr, const char *message, int size, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        
        // Find decoder for this track
        auto it = receiver->decoders.find(tr);
        if (it != receiver->decoders.end()) {
            // Process RTP packet through appropriate decoder
            it->second->processRTPPacket(reinterpret_cast<const uint8_t*>(message), size);
        } else {
            std::cout << "Received media packet of size: " << size << " bytes (no decoder)" << std::endl;
        }
    }

    static void onTrackClosed(int tr, void *user_ptr) {
        WhipReceiver* receiver = static_cast<WhipReceiver*>(user_ptr);
        std::cout << "Track " << tr << " closed" << std::endl;
        
        // Clean up decoder
        auto it = receiver->decoders.find(tr);
        if (it != receiver->decoders.end()) {
            it->second->cleanup();
            receiver->decoders.erase(it);
        }
    }

    // Create a new PeerConnection for WHIP session
    int createPeerConnection(const std::string& sessionId) {
        rtcConfiguration config = {};
        
        // Add STUN servers for NAT traversal
        const char* stunServers[] = {
            "stun:stun.l.google.com:19302",
            "stun:stun1.l.google.com:19302"
        };
        config.iceServers = stunServers;
        config.iceServersCount = 2;
        
        // Enable media transport for receiving tracks
        config.forceMediaTransport = true;
        
        int pc = rtcCreatePeerConnection(&config);
        if (pc < 0) {
            std::cerr << "Failed to create PeerConnection: " << pc << std::endl;
            return pc;
        }
        
        // Set user pointer and callbacks
        rtcSetUserPointer(pc, this);
        rtcSetLocalDescriptionCallback(pc, onLocalDescription);
        rtcSetLocalCandidateCallback(pc, onLocalCandidate);
        rtcSetStateChangeCallback(pc, onStateChange);
        rtcSetGatheringStateChangeCallback(pc, onGatheringStateChange);
        rtcSetTrackCallback(pc, onTrack);
        
        sessions[sessionId] = pc;
        
        std::cout << "Created PeerConnection for session: " << sessionId << std::endl;
        return pc;
    }

    // Process WHIP offer and generate answer
    std::string processWhipOffer(const std::string& sessionId, const std::string& sdpOffer) {
        int pc = createPeerConnection(sessionId);
        if (pc < 0) {
            return "";
        }
        
        std::cout << "Processing SDP offer:" << std::endl;
        std::cout << sdpOffer << std::endl;
        
        // Set remote description (the offer)
        int result = rtcSetRemoteDescription(pc, sdpOffer.c_str(), "offer");
        if (result != RTC_ERR_SUCCESS) {
            std::cerr << "Failed to set remote description: " << result << std::endl;
            return "";
        }
        
        // Wait a bit for ICE gathering (in production, you'd handle this asynchronously)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Get the generated answer
        char answerBuffer[8192];
        int answerSize = rtcGetLocalDescription(pc, answerBuffer, sizeof(answerBuffer));
        if (answerSize < 0) {
            std::cerr << "Failed to get local description: " << answerSize << std::endl;
            return "";
        }
        
        std::string sdpAnswer(answerBuffer);
        std::cout << "Generated SDP answer:" << std::endl;
        std::cout << sdpAnswer << std::endl;
        
        return sdpAnswer;
    }

    // Simple HTTP server implementation
    void startHttpServer(int port = 8080) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return;
        }
#endif

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }

        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return;
        }

        if (listen(serverSocket, 3) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return;
        }

        running = true;
        std::cout << "WHIP server listening on port " << port << std::endl;
        
        serverThread = std::thread(&WhipReceiver::serverLoop, this);
    }

    void serverLoop() {
        while (running) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket < 0) {
                if (running) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }
            
            // Handle client in separate thread
            std::thread clientThread(&WhipReceiver::handleClient, this, clientSocket);
            clientThread.detach();
        }
    }

    void handleClient(int clientSocket) {
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
        
        std::cout << "Received HTTP request:" << std::endl;
        std::cout << request.substr(0, 200) << "..." << std::endl;
        
        // Parse HTTP method and path
        std::string method, path;
        std::istringstream iss(request);
        iss >> method >> path;
        
        if (method == "POST" && path == "/whip") {
            handleWhipPost(clientSocket, request);
        } else if (method == "OPTIONS" && path == "/whip") {
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

    void handleWhipPost(int clientSocket, const std::string& request) {
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
        std::string sessionId = generateSessionId();
        
        // Process the offer
        std::string sdpAnswer = processWhipOffer(sessionId, sdpOffer);
        if (sdpAnswer.empty()) {
            sendHttpResponse(clientSocket, 500, "text/plain", "Failed to process offer");
            return;
        }
        
        // Send response with Location header
        std::string locationHeader = "Location: /whip/sessions/" + sessionId + "\r\n";
        sendHttpResponse(clientSocket, 201, "application/sdp", sdpAnswer, locationHeader);
    }

    void handleOptions(int clientSocket) {
        std::string headers = 
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, PATCH, DELETE\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n";
        sendHttpResponse(clientSocket, 204, "", "", headers);
    }

    void sendHttpResponse(int clientSocket, int statusCode, const std::string& contentType, 
                         const std::string& body, const std::string& additionalHeaders = "") {
        std::string status;
        switch (statusCode) {
            case 200: status = "200 OK"; break;
            case 201: status = "201 Created"; break;
            case 204: status = "204 No Content"; break;
            case 400: status = "400 Bad Request"; break;
            case 404: status = "404 Not Found"; break;
            case 500: status = "500 Internal Server Error"; break;
            default: status = "500 Internal Server Error"; break;
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

    std::string generateSessionId() {
        static int counter = 0;
        return "session_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
    }

    void stop() {
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
        
        // Clean up all decoders
        for (auto& [trackId, decoder] : decoders) {
            decoder->cleanup();
        }
        decoders.clear();
        
        // Clean up all sessions
        for (auto& [sessionId, pc] : sessions) {
            rtcDeletePeerConnection(pc);
        }
        sessions.clear();
        
        std::cout << "WHIP Receiver stopped" << std::endl;
    }
};

int main() {
    std::cout << "Starting WHIP Stream Receiver with H.264 and Opus decoding..." << std::endl;
    
    WhipReceiver receiver;
    
    // Start HTTP server on port 8080
    receiver.startHttpServer(8080);
    
    std::cout << "WHIP endpoint available at: http://localhost:8080/whip" << std::endl;
    std::cout << "Decoded video will be saved to: decoded_video.yuv" << std::endl;
    std::cout << "Decoded audio will be saved to: decoded_audio.pcm" << std::endl;
    std::cout << "Press Enter to stop..." << std::endl;
    
    std::cin.get();
    
    receiver.stop();
    
    return 0;
}

#define PROP_SOURCE "source_name"
#define PROP_URL "url"

typedef struct {
	obs_source_t *obs_source;
    WhipReceiver receiver;
} whip_source_t;

const char *whip_source_getname(void *)
{
	return "WHIP";
}

obs_properties_t *whip_source_getproperties(void *data)
{
	auto s = (whip_source_t *)data;
	obs_log(LOG_DEBUG, "+whip_source_getproperties(…)");

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, PROP_URL, "WHIP Endpoint", OBS_TEXT_DEFAULT );
	
	return props;
}

void whip_source_getdefaults(obs_data_t *settings)
{
	obs_log(LOG_DEBUG, "+whip_source_getdefaults(…)");
	obs_data_set_default_string(settings, PROP_URL, "https://127.0.0.1:8080/whip");

	obs_log(LOG_DEBUG, "-whip_source_getdefaults(…)");
}

void whip_source_update(void *data, obs_data_t *settings)
{
	auto s = (whip_source_t *)data;
	auto obs_source = s->obs_source;
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_update(…)", obs_source_name);
    
    // Start HTTP server on port 8080
    s->receiver.startHttpServer(8080);
    
	obs_log(LOG_DEBUG, "'%s' -whip_source_update(…)", obs_source_name);
}

void whip_source_shown(void *data)
{

}

void whip_source_hidden(void *data)
{

}

void whip_source_activated(void *data)
{
	
}

void whip_source_deactivated(void *data)
{

}

void *whip_source_create(obs_data_t *settings, obs_source_t *obs_source)
{
	auto obs_source_name = obs_source_get_name(obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_create(…)", obs_source_name);

	auto s = (whip_source_t *)bzalloc(sizeof(whip_source_t));
	s->obs_source = obs_source;

	whip_source_update(s, settings);

	obs_log(LOG_DEBUG, "'%s' -whip_source_create(…)", obs_source_name);

	return s;
}

void whip_source_destroy(void *data)
{
	auto s = (whip_source_t *)data;
	auto obs_source_name = obs_source_get_name(s->obs_source);
	obs_log(LOG_DEBUG, "'%s' +whip_source_destroy(…)", obs_source_name);
    s->receiver.stop();
	bfree(s);

	obs_log(LOG_DEBUG, "'%s' -whip_source_destroy(…)", obs_source_name);
}

uint32_t whip_source_get_width(void *data)
{
	auto s = (whip_source_t *)data;
	return 1920;
}

uint32_t whip_source_get_height(void *data)
{
	auto s = (whip_source_t *)data;
	return 1080;
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
