#include "gvep_r0.h"

#if GROOVEPUTER_GVEP_R0

#include <Arduino.h>
#include <WiFi.h>

#include <atomic>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace GroovePuterVisual {
namespace {

constexpr uint8_t kEspNowChannel = 6;
constexpr uint32_t kDiagnosticsPeriodMs = 5000;
constexpr uint32_t kIdlePollMs = 5;
constexpr uint32_t kBootstrapPollMs = 25;
constexpr uint32_t kWorkerStackBytes = 4096;
constexpr UBaseType_t kWorkerPriority = 1;
constexpr BaseType_t kWorkerCore = 0;
constexpr uint8_t kBroadcastMac[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

GvepR0EventBus g_bus;
std::atomic<uint32_t> g_sendEnqueueOk{0};
std::atomic<uint32_t> g_sendEnqueueFail{0};
std::atomic<uint32_t> g_sendCallbackOk{0};
std::atomic<uint32_t> g_sendCallbackFail{0};
std::atomic<uint32_t> g_initFailures{0};
std::atomic<bool> g_sendInFlight{false};

StaticTask_t g_workerTaskBuffer{};
alignas(16) StackType_t g_workerTaskStack[kWorkerStackBytes]{};
TaskHandle_t g_workerTaskHandle = nullptr;

void logHeapCheckpoint(const char* tag) {
    const uint32_t freeInternal = static_cast<uint32_t>(
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t minimumInternal = static_cast<uint32_t>(
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t largestInternal = static_cast<uint32_t>(
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t freePsram = static_cast<uint32_t>(
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    Serial.printf(
        "[GVEP-R0][%s] freeInt=%u minInt=%u largestInt=%u freePsram=%u\n",
        tag,
        static_cast<unsigned>(freeInternal),
        static_cast<unsigned>(minimumInternal),
        static_cast<unsigned>(largestInternal),
        static_cast<unsigned>(freePsram));
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void onEspNowSent(const esp_now_send_info_t*, esp_now_send_status_t status) {
#else
void onEspNowSent(const uint8_t*, esp_now_send_status_t status) {
#endif
    if (status == ESP_NOW_SEND_SUCCESS) {
        g_sendCallbackOk.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_sendCallbackFail.fetch_add(1, std::memory_order_relaxed);
    }
    // The callback owns no musical state. Its only synchronization duty is to
    // release the single outstanding radio slot for the low-priority TX task.
    g_sendInFlight.store(false, std::memory_order_release);
}

bool initializeEspNow() {
    logHeapCheckpoint("before-wifi");

    if (!WiFi.mode(WIFI_STA)) {
        ++g_initFailures;
        Serial.println("[GVEP-R0] WiFi STA init failed");
        return false;
    }
    logHeapCheckpoint("after-wifi");

    const esp_err_t powerSaveResult = esp_wifi_set_ps(WIFI_PS_NONE);
    if (powerSaveResult != ESP_OK) {
        Serial.printf("[GVEP-R0] esp_wifi_set_ps failed: %d\n",
                      static_cast<int>(powerSaveResult));
    }

    const esp_err_t channelResult =
        esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE);
    if (channelResult != ESP_OK) {
        ++g_initFailures;
        Serial.printf("[GVEP-R0] set channel %u failed: %d\n",
                      static_cast<unsigned>(kEspNowChannel),
                      static_cast<int>(channelResult));
        WiFi.mode(WIFI_OFF);
        return false;
    }

    const esp_err_t initResult = esp_now_init();
    if (initResult != ESP_OK) {
        ++g_initFailures;
        Serial.printf("[GVEP-R0] esp_now_init failed: %d\n",
                      static_cast<int>(initResult));
        WiFi.mode(WIFI_OFF);
        return false;
    }
    logHeapCheckpoint("after-espnow");

    const esp_err_t callbackResult = esp_now_register_send_cb(onEspNowSent);
    if (callbackResult != ESP_OK) {
        ++g_initFailures;
        Serial.printf("[GVEP-R0] send callback registration failed: %d\n",
                      static_cast<int>(callbackResult));
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);
        return false;
    }

    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, kBroadcastMac, sizeof(kBroadcastMac));
    peer.channel = kEspNowChannel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    const esp_err_t peerResult = esp_now_add_peer(&peer);
    if (peerResult != ESP_OK && peerResult != ESP_ERR_ESPNOW_EXIST) {
        ++g_initFailures;
        Serial.printf("[GVEP-R0] broadcast peer add failed: %d\n",
                      static_cast<int>(peerResult));
        esp_now_unregister_send_cb();
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);
        return false;
    }
    logHeapCheckpoint("after-peer");

    uint8_t mac[6]{};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        Serial.printf(
            "[GVEP-R0] READY channel=%u tx=%02X:%02X:%02X:%02X:%02X:%02X packet=%uB\n",
            static_cast<unsigned>(kEspNowChannel),
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            static_cast<unsigned>(kGvepV1PacketSize));
    } else {
        Serial.printf("[GVEP-R0] READY channel=%u packet=%uB\n",
                      static_cast<unsigned>(kEspNowChannel),
                      static_cast<unsigned>(kGvepV1PacketSize));
    }

    return true;
}

bool trySendEvent(const GvepEvent& event) {
    if (g_sendInFlight.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }

    uint8_t packet[kGvepV1PacketSize]{};
    const uint32_t timestampUs =
        static_cast<uint32_t>(esp_timer_get_time() & 0xFFFFFFFFULL);
    serializeGvepV1Event(event, timestampUs, packet);

    const esp_err_t result =
        esp_now_send(kBroadcastMac, packet, sizeof(packet));
    if (result == ESP_OK) {
        g_sendEnqueueOk.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    g_sendEnqueueFail.fetch_add(1, std::memory_order_relaxed);
    g_sendInFlight.store(false, std::memory_order_release);
    return false;
}

void logRuntimeDiagnostics() {
    const uint32_t freeInternal = static_cast<uint32_t>(
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t minimumInternal = static_cast<uint32_t>(
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t largestInternal = static_cast<uint32_t>(
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    Serial.printf(
        "[GVEP-R0][RUN] freeInt=%u minInt=%u largestInt=%u "
        "pub=%u pop=%u drop=%u high=%u send=%u/%u cb=%u/%u inFlight=%u initFail=%u\n",
        static_cast<unsigned>(freeInternal),
        static_cast<unsigned>(minimumInternal),
        static_cast<unsigned>(largestInternal),
        static_cast<unsigned>(g_bus.publishedCount()),
        static_cast<unsigned>(g_bus.poppedCount()),
        static_cast<unsigned>(g_bus.droppedCount()),
        static_cast<unsigned>(g_bus.highWaterMark()),
        static_cast<unsigned>(g_sendEnqueueOk.load(std::memory_order_relaxed)),
        static_cast<unsigned>(g_sendEnqueueFail.load(std::memory_order_relaxed)),
        static_cast<unsigned>(g_sendCallbackOk.load(std::memory_order_relaxed)),
        static_cast<unsigned>(g_sendCallbackFail.load(std::memory_order_relaxed)),
        static_cast<unsigned>(g_sendInFlight.load(std::memory_order_relaxed) ? 1u : 0u),
        static_cast<unsigned>(g_initFailures.load(std::memory_order_relaxed)));
}

void gvepWorkerTask(void*) {
    // R0 has no persistent Settings hook yet. Avoid coupling bootstrap to any UI
    // object: the radio is initialized lazily only after AudioTask has published
    // the first real semantic event. For normal local operation this is PLAY,
    // which cannot be generated by keyboard/UI interaction until setup() has
    // completed and loop() is servicing input.
    while (g_bus.publishedCount() == 0u) {
        vTaskDelay(pdMS_TO_TICKS(kBootstrapPollMs));
    }

    if (!initializeEspNow()) {
        Serial.println("[GVEP-R0] DISABLED after initialization failure");
        vTaskDelete(nullptr);
        return;
    }

    uint32_t lastDiagnosticsMs = millis();
    for (;;) {
        bool sentAny = false;
        if (!g_sendInFlight.load(std::memory_order_acquire)) {
            GvepEvent event{};
            if (g_bus.tryPop(event)) {
                // A synchronous esp_now_send() failure loses this visual event.
                // The producer is never involved in retransmission; sequence
                // gaps and counters expose the loss without touching audio.
                trySendEvent(event);
                sentAny = true;
            }
        }

        const uint32_t nowMs = millis();
        if (nowMs - lastDiagnosticsMs >= kDiagnosticsPeriodMs) {
            logRuntimeDiagnostics();
            lastDiagnosticsMs = nowMs;
        }

        if (!sentAny) {
            vTaskDelay(pdMS_TO_TICKS(kIdlePollMs));
        } else {
            taskYIELD();
        }
    }
}

struct GvepR0WorkerStarter {
    GvepR0WorkerStarter() {
        g_workerTaskHandle = xTaskCreateStaticPinnedToCore(
            gvepWorkerTask,
            "GvepR0Tx",
            kWorkerStackBytes,
            nullptr,
            kWorkerPriority,
            g_workerTaskStack,
            &g_workerTaskBuffer,
            kWorkerCore);
    }
};

GvepR0WorkerStarter g_workerStarter;

}  // namespace

GvepR0EventBus& gvepR0EventBus() {
    return g_bus;
}

}  // namespace GroovePuterVisual

#endif  // GROOVEPUTER_GVEP_R0
