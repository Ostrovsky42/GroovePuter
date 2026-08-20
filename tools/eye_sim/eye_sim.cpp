#include "../../src/eye_pair_sync/eye_output_mode.h"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef HAS_SDL2
#include <SDL2/SDL.h>
#endif

struct EyeVisualState {
    uint8_t synth_a_mode{0}; // 0: LEGACY, 1: INTERNAL, 2: MIDI, 3: LAYER
    uint8_t synth_b_mode{0};
    uint8_t drums_mode{0};
    std::string toast_msg;
    std::chrono::steady_clock::time_point toast_time;
};

static EyeVisualState g_visualState;
static std::atomic<bool> g_running{true};

struct ReceiverSequenceState {
    bool initialized{false};
    uint32_t session_id{0};
    uint32_t last_seq{0};
};

static ReceiverSequenceState g_sequenceState;

bool acceptSequence(uint32_t session_id, uint32_t seq) {
    if (seq == 0u) {
        std::cout << "[GVEP-RX] REJECT seq=0\n";
        return false;
    }
    if (!g_sequenceState.initialized ||
        g_sequenceState.session_id != session_id) {
        g_sequenceState.initialized = true;
        g_sequenceState.session_id = session_id;
        g_sequenceState.last_seq = seq;
        std::cout << "[GVEP-RX] NEW SESSION id=" << session_id << "\n";
        return true;
    }
    if (static_cast<int32_t>(seq - g_sequenceState.last_seq) <= 0) {
        std::cout << "[GVEP-RX] REJECT stale/duplicate seq=" << seq
                  << " last=" << g_sequenceState.last_seq << "\n";
        return false;
    }
    g_sequenceState.last_seq = seq;
    return true;
}

bool parseBoundedNumber(const std::string& text, long minimum,
                        long maximum, long& value) {
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' ||
        parsed < minimum || parsed > maximum) {
        return false;
    }
    value = parsed;
    return true;
}

const char* modeName(uint8_t m) {
    switch (m) {
        case 1: return "INTERNAL";
        case 2: return "MIDI";
        case 3: return "LAYER";
        default: return "LEGACY";
    }
}

const char* colorHex(uint8_t m) {
    switch (m) {
        case 1: return "#00FF9D (Cyber Green)";
        case 2: return "#00F0FF (Neon Blue)";
        case 3: return "#FF0055 (Hot Pink)";
        default: return "#333333 (Muted Dark)";
    }
}

void processPacket(const eye_output_mode_packet_t& pkt) {
    if (pkt.magic != EYE_SYNC_MAGIC_OUTPUT_MODE || pkt.version != EYE_SYNC_VERSION_OUTPUT_MODE) {
        std::cout << "[SIM] Rejecting invalid packet magic/version\n";
        return;
    }
    uint8_t expected_crc = eye_output_mode_calc_crc8(&pkt);
    if (pkt.crc != expected_crc) {
        std::cout << "[SIM] CRC mismatch! Expected: " << (int)expected_crc << " Got: " << (int)pkt.crc << "\n";
        return;
    }
    if (pkt.reserved != 0u || pkt.track > EYE_TRACK_DRUMS ||
        pkt.mode > EYE_OUT_LAYER || (pkt.flags & ~0x01u) != 0u) {
        std::cout << "[SIM] Rejecting invalid Output Mode fields\n";
        return;
    }
    if (!acceptSequence(pkt.session_id, pkt.seq)) return;

    const char* trackName = (pkt.track == 0) ? "SYNTH A" : (pkt.track == 1) ? "SYNTH B" : "DRUMS";
    if (pkt.track == 0) g_visualState.synth_a_mode = pkt.mode;
    else if (pkt.track == 1) g_visualState.synth_b_mode = pkt.mode;
    else if (pkt.track == 2) g_visualState.drums_mode = pkt.mode;

    bool animate = (pkt.flags & 0x01) != 0;
    if (animate) {
        std::string toast = std::string(trackName) + " OUT:" + modeName(pkt.mode);
        g_visualState.toast_msg = toast;
        g_visualState.toast_time = std::chrono::steady_clock::now();
        std::cout << "[MASTER/FOLLOWER EYE] ANIMATE -> " << toast << " Color: " << colorHex(pkt.mode) << std::endl;
    } else {
        std::cout << "[MASTER/FOLLOWER EYE] SILENT RESTORE -> " << trackName << " OUT:" << modeName(pkt.mode) << std::endl;
    }
}

const char* eventName(uint8_t eventType, uint8_t value0) {
    switch (eventType) {
        case EYE_GVEP_TRANSPORT: return value0 != 0u ? "TRANSPORT PLAY" : "TRANSPORT STOP";
        case EYE_GVEP_KICK: return "KICK";
        case EYE_GVEP_BAR: return "BAR";
        default: return "UNKNOWN";
    }
}

void processGvepPacket(const eye_gvep_packet_t& pkt) {
    if (pkt.magic != EYE_SYNC_MAGIC_GVEP ||
        pkt.version != EYE_SYNC_VERSION_GVEP) {
        std::cout << "[GVEP-RX] REJECT magic/version\n";
        return;
    }
    if (pkt.crc != eye_gvep_calc_crc8(&pkt)) {
        std::cout << "[GVEP-RX] REJECT CRC seq=" << pkt.seq << "\n";
        return;
    }
    if (pkt.event_type > EYE_GVEP_BAR ||
        (pkt.event_type == EYE_GVEP_TRANSPORT && pkt.value0 > 1u) ||
        (pkt.event_type == EYE_GVEP_KICK && pkt.value0 > 127u) ||
        (pkt.event_type == EYE_GVEP_BAR && pkt.value1 == 0u)) {
        std::cout << "[GVEP-RX] REJECT fields seq=" << pkt.seq << "\n";
        return;
    }
    if (!acceptSequence(pkt.session_id, pkt.seq)) return;

    std::cout << "[GVEP-RX] seq=" << pkt.seq
              << " event=" << eventName(pkt.event_type, pkt.value0)
              << " value0=" << static_cast<unsigned>(pkt.value0)
              << " value1=" << pkt.value1
              << " timestamp_us=" << pkt.timestamp_us << "\n";
}

void udpListenerThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "[SIM] Failed to create UDP socket\n";
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    struct timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr{};
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9876);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[SIM] Failed to bind UDP port 9876\n";
        close(sock);
        return;
    }

    std::cout << "[SIM] Listening for Dual-Eye 0xAF/0xB0 UDP packets on port 9876...\n";

    while (g_running) {
        uint8_t bytes[sizeof(eye_gvep_packet_t)]{};
        struct sockaddr_in client{};
        socklen_t len = sizeof(client);
        ssize_t n = recvfrom(sock, bytes, sizeof(bytes), 0,
                             (struct sockaddr*)&client, &len);
        if (n < 0) continue;
        if (n != static_cast<ssize_t>(sizeof(bytes))) {
            std::cout << "[GVEP-RX] REJECT length=" << n << "\n";
            continue;
        }
        if (bytes[0] == EYE_SYNC_MAGIC_OUTPUT_MODE) {
            eye_output_mode_packet_t pkt{};
            std::memcpy(&pkt, bytes, sizeof(pkt));
            processPacket(pkt);
        } else if (bytes[0] == EYE_SYNC_MAGIC_GVEP) {
            eye_gvep_packet_t pkt{};
            std::memcpy(&pkt, bytes, sizeof(pkt));
            processGvepPacket(pkt);
        } else {
            std::cout << "[GVEP-RX] REJECT magic="
                      << static_cast<unsigned>(bytes[0]) << "\n";
        }
    }
    close(sock);
}

void cliThread() {
    std::cout << "\n=== Linux Dual-Eye Simulator (tools/eye_sim) ===\n";
    std::cout << "Available commands:\n";
    std::cout << "  master> output synth_a internal\n";
    std::cout << "  master> output synth_a midi\n";
    std::cout << "  master> output drums layer\n";
    std::cout << "  master> transport play|stop\n";
    std::cout << "  master> kick 1..127\n";
    std::cout << "  master> bar 1..65535\n";
    std::cout << "  master> output status\n";
    std::cout << "  master> quit\n\n";

    std::string line;
    while (g_running) {
        std::cout << "master> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;

        if (line == "quit" || line == "exit") {
            g_running = false;
            break;
        }

        if (line == "output status" || line == "status") {
            std::cout << "Current Output Modes:\n";
            std::cout << "  Left Eye (Follower)  - SYNTH A: " << modeName(g_visualState.synth_a_mode) << " (" << colorHex(g_visualState.synth_a_mode) << ")\n";
            std::cout << "  Right Eye (Master)   - SYNTH B: " << modeName(g_visualState.synth_b_mode) << " (" << colorHex(g_visualState.synth_b_mode) << ")\n";
            std::cout << "  Both Eyes (Rings)    - DRUMS:   " << modeName(g_visualState.drums_mode) << " (" << colorHex(g_visualState.drums_mode) << ")\n";
            continue;
        }

        if (line == "transport play") {
            eye_gvep_notify_transport(true);
            continue;
        }
        if (line == "transport stop") {
            eye_gvep_notify_transport(false);
            continue;
        }
        if (line.find("kick ") == 0) {
            long velocity = 0;
            if (parseBoundedNumber(line.substr(5), 1, 127, velocity)) {
                eye_gvep_notify_kick(static_cast<uint8_t>(velocity));
                continue;
            }
        }
        if (line.find("bar ") == 0) {
            long bar = 0;
            if (parseBoundedNumber(line.substr(4), 1, 65535, bar)) {
                eye_gvep_notify_bar(static_cast<uint16_t>(bar));
                continue;
            }
        }

        if (line.find("output ") == 0) {
            std::string sub = line.substr(7);
            size_t space = sub.find(' ');
            if (space != std::string::npos) {
                std::string target = sub.substr(0, space);
                std::string modeStr = sub.substr(space + 1);

                eye_track_t track = EYE_TRACK_SYNTH_A;
                if (target == "synth_b") track = EYE_TRACK_SYNTH_B;
                else if (target == "drums") track = EYE_TRACK_DRUMS;

                eye_output_mode_t mode = EYE_OUT_INTERNAL;
                if (modeStr == "midi") mode = EYE_OUT_MIDI;
                else if (modeStr == "layer") mode = EYE_OUT_LAYER;
                else if (modeStr == "legacy") mode = EYE_OUT_LEGACY;

                eye_output_mode_notify(track, mode);
                continue;
            }
        }

        std::cout << "Unknown command. Try: transport play | transport stop | kick 120 | bar 1 | output status\n";
    }
}

int main() {
    eye_output_mode_init();

    std::thread udp_thread(udpListenerThread);
    cliThread();

    g_running = false;
    if (udp_thread.joinable()) udp_thread.join();

    return 0;
}
