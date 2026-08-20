#include "eye_output_mode.h"

#include <atomic>
#include <cstring>

#if defined(ARDUINO) || defined(ESP_PLATFORM)
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#include <chrono>
#include <sys/types.h>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#define GVEP_USE_UDP 1
#endif

#if (defined(ARDUINO) || defined(ESP_PLATFORM)) && \
    GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW
#include <WiFi.h>
#include <esp_now.h>
#define GVEP_USE_ESPNOW 1
#endif

namespace {

struct EyeState {
    bool initialized{false};
    uint32_t session_id{0};
    std::atomic<uint32_t> seq{0};
    uint8_t modes[3]{EYE_OUT_LEGACY, EYE_OUT_LEGACY, EYE_OUT_LEGACY};
#if defined(GVEP_USE_UDP)
    int udp_sock{-1};
#endif
};

static EyeState g_eyeState;

#if defined(GVEP_USE_ESPNOW)
constexpr uint8_t kGvepQueueCapacity = 32;
struct GvepQueue {
    eye_gvep_packet_t packets[kGvepQueueCapacity]{};
    std::atomic<uint8_t> write{0};
    std::atomic<uint8_t> read{0};
    std::atomic<uint32_t> dropped{0};
};

static GvepQueue g_gvepQueue;
#endif

uint8_t calcCrc8(const uint8_t* data, size_t len) {
    if (data == nullptr) return 0;
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80u) != 0u
                ? static_cast<uint8_t>((crc << 1u) ^ 0x07u)
                : static_cast<uint8_t>(crc << 1u);
        }
    }
    return crc;
}

uint32_t makeSessionId() {
#if defined(ARDUINO) || defined(ESP_PLATFORM)
    uint32_t value = esp_random();
#else
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const uint64_t ticks = static_cast<uint64_t>(now.count());
    uint32_t value = static_cast<uint32_t>(ticks) ^
                     static_cast<uint32_t>(ticks >> 32u) ^
                     0x9E3779B9u;
#endif
    return value == 0u ? 1u : value;
}

int64_t nowMicros() {
#if defined(ARDUINO) || defined(ESP_PLATFORM)
    return static_cast<int64_t>(esp_timer_get_time());
#else
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
#endif
}

uint32_t nextSequence() {
    uint32_t current = g_eyeState.seq.load(std::memory_order_relaxed);
    for (;;) {
        uint32_t next = current + 1u;
        // Sequence zero is reserved as invalid by the follower. Wrap to one;
        // the receiver compares sequence numbers with wrap-safe arithmetic.
        if (next == 0u) next = 1u;
        if (g_eyeState.seq.compare_exchange_weak(
                current, next, std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            return next;
        }
    }
}

void sendRaw(const void* data, size_t len) {
    if (data == nullptr || len == 0u) return;

#if defined(GVEP_USE_UDP)
    if (g_eyeState.udp_sock >= 0) {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9876);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        sendto(g_eyeState.udp_sock, data, len, 0,
               reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
    }
#endif

#if defined(GVEP_USE_ESPNOW)
    static uint8_t broadcastMac[6] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    esp_now_send(broadcastMac, static_cast<const uint8_t*>(data), len);
#endif
}

void enqueueOrSend(const eye_gvep_packet_t& packet) {
#if defined(GVEP_USE_ESPNOW)
    const uint8_t write = g_gvepQueue.write.load(std::memory_order_relaxed);
    const uint8_t next = static_cast<uint8_t>(
        (write + 1u) % kGvepQueueCapacity);
    if (next == g_gvepQueue.read.load(std::memory_order_acquire)) {
        g_gvepQueue.dropped.fetch_add(1u, std::memory_order_relaxed);
        return;
    }
    g_gvepQueue.packets[write] = packet;
    g_gvepQueue.write.store(next, std::memory_order_release);
#else
    // The host simulator has no realtime AudioTask/Wi-Fi boundary; direct UDP
    // delivery keeps its existing CLI behavior and does not affect Cardputer.
    sendRaw(&packet, sizeof(packet));
#endif
}

#if defined(GVEP_USE_ESPNOW)
void flushQueuedGvep() {
    uint32_t dropped = g_gvepQueue.dropped.exchange(0u, std::memory_order_acq_rel);
    (void)dropped;
    uint8_t read = g_gvepQueue.read.load(std::memory_order_relaxed);
    const uint8_t write = g_gvepQueue.write.load(std::memory_order_acquire);
    uint8_t budget = 8;
    while (read != write && budget-- > 0u) {
        const eye_gvep_packet_t packet = g_gvepQueue.packets[read];
        read = static_cast<uint8_t>((read + 1u) % kGvepQueueCapacity);
        g_gvepQueue.read.store(read, std::memory_order_release);
        sendRaw(&packet, sizeof(packet));
    }
}
#endif

}  // namespace

uint8_t eye_output_mode_calc_crc8(const eye_output_mode_packet_t* pkt) {
    return pkt == nullptr ? 0u : calcCrc8(reinterpret_cast<const uint8_t*>(pkt),
                                           sizeof(*pkt) - 1u);
}

uint8_t eye_gvep_calc_crc8(const eye_gvep_packet_t* pkt) {
    return pkt == nullptr ? 0u : calcCrc8(reinterpret_cast<const uint8_t*>(pkt),
                                          sizeof(*pkt) - 1u);
}

void eye_output_mode_init(void) {
    if (g_eyeState.initialized) return;

    g_eyeState.session_id = makeSessionId();
    g_eyeState.seq.store(0u, std::memory_order_relaxed);

#if defined(GVEP_USE_UDP)
    g_eyeState.udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_eyeState.udp_sock >= 0) {
        int opt = 1;
        setsockopt(g_eyeState.udp_sock, SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));
    }
#endif

#if defined(GVEP_USE_ESPNOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK) {
        esp_now_peer_info_t peerInfo = {};
        for (uint8_t i = 0; i < 6; ++i) peerInfo.peer_addr[i] = 0xFF;
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
#endif

    g_eyeState.initialized = true;
}

bool eye_output_mode_transport_enabled(void) {
#if defined(GVEP_USE_UDP) || defined(GVEP_USE_ESPNOW)
    return true;
#else
    return false;
#endif
}

uint32_t eye_output_mode_session_id(void) {
    eye_output_mode_init();
    return g_eyeState.session_id;
}

bool eye_output_mode_build_packet(eye_output_mode_packet_t* packet,
                                  eye_track_t track,
                                  eye_output_mode_t mode,
                                  bool animate) {
    if (packet == nullptr || track > EYE_TRACK_DRUMS || mode > EYE_OUT_LAYER) {
        return false;
    }
    eye_output_mode_init();

    *packet = {};
    packet->magic = EYE_SYNC_MAGIC_OUTPUT_MODE;
    packet->version = EYE_SYNC_VERSION_OUTPUT_MODE;
    packet->session_id = g_eyeState.session_id;
    packet->seq = nextSequence();
    if (animate) {
        packet->effect_t0_us = nowMicros();
        // The v2 contract requires a strictly positive animated origin. The
        // ESP timer normally starts above zero, but preserve that invariant
        // even if a packet is built immediately during platform startup.
        if (packet->effect_t0_us <= 0) packet->effect_t0_us = 1;
    } else {
        packet->effect_t0_us = 0;
    }
    packet->track = static_cast<uint8_t>(track);
    packet->mode = static_cast<uint8_t>(mode);
    packet->flags = animate ? 0x01u : 0u;
    packet->reserved = 0u;
    packet->crc = eye_output_mode_calc_crc8(packet);
    return true;
}

void eye_output_mode_notify(eye_track_t track, eye_output_mode_t mode) {
    eye_output_mode_packet_t packet{};
    if (!eye_output_mode_build_packet(&packet, track, mode, true)) return;
    g_eyeState.modes[static_cast<size_t>(track)] = static_cast<uint8_t>(mode);
    sendRaw(&packet, sizeof(packet));
}

void eye_output_mode_restore(eye_track_t track, eye_output_mode_t mode) {
    eye_output_mode_packet_t packet{};
    if (!eye_output_mode_build_packet(&packet, track, mode, false)) return;
    g_eyeState.modes[static_cast<size_t>(track)] = static_cast<uint8_t>(mode);
    sendRaw(&packet, sizeof(packet));
}

void eye_output_mode_flush(void) {
#if defined(GVEP_USE_ESPNOW)
    flushQueuedGvep();
#endif
}

bool eye_gvep_build_packet(eye_gvep_packet_t* packet,
                           eye_gvep_event_type_t event_type,
                           uint8_t value0,
                           uint16_t value1,
                           int64_t timestamp_us) {
    if (packet == nullptr || event_type > EYE_GVEP_BAR) return false;
    if (event_type == EYE_GVEP_TRANSPORT && value0 > 1u) return false;
    if (event_type == EYE_GVEP_BAR && value1 == 0u) return false;
    eye_output_mode_init();

    *packet = {};
    packet->magic = EYE_SYNC_MAGIC_GVEP;
    packet->version = EYE_SYNC_VERSION_GVEP;
    packet->session_id = g_eyeState.session_id;
    packet->seq = nextSequence();
    packet->timestamp_us = timestamp_us > 0 ? timestamp_us : nowMicros();
    packet->event_type = static_cast<uint8_t>(event_type);
    packet->value0 = value0;
    packet->value1 = value1;
    packet->crc = eye_gvep_calc_crc8(packet);
    return true;
}

void eye_gvep_notify_transport_at(bool is_playing, int64_t timestamp_us) {
    if (!eye_output_mode_transport_enabled()) return;
    eye_gvep_packet_t packet{};
    if (eye_gvep_build_packet(&packet, EYE_GVEP_TRANSPORT,
                              is_playing ? 1u : 0u, 0u, timestamp_us)) {
        enqueueOrSend(packet);
    }
}

void eye_gvep_notify_transport(bool is_playing) {
    eye_gvep_notify_transport_at(is_playing, nowMicros());
}

void eye_gvep_notify_kick_at(uint8_t velocity, int64_t timestamp_us) {
    if (!eye_output_mode_transport_enabled()) return;
    if (velocity > 127u) velocity = 127u;
    eye_gvep_packet_t packet{};
    if (eye_gvep_build_packet(&packet, EYE_GVEP_KICK, velocity, 0u,
                              timestamp_us)) {
        enqueueOrSend(packet);
    }
}

void eye_gvep_notify_kick(uint8_t velocity) {
    eye_gvep_notify_kick_at(velocity, nowMicros());
}

void eye_gvep_notify_bar_at(uint16_t bar_number, int64_t timestamp_us) {
    if (!eye_output_mode_transport_enabled()) return;
    eye_gvep_packet_t packet{};
    if (eye_gvep_build_packet(&packet, EYE_GVEP_BAR, 0u, bar_number,
                              timestamp_us)) {
        enqueueOrSend(packet);
    }
}

void eye_gvep_notify_bar(uint16_t bar_number) {
    eye_gvep_notify_bar_at(bar_number, nowMicros());
}
