#include "cardputer_usb_midi_tx_stress.h"

#include "cardputer_usb_midi_service.h"

#include <cstddef>
#include <cstdint>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "src/midi/usb_midi_transport.h"

namespace {

constexpr uint16_t kTxStressRates[] = {50, 100, 200, 400, 800};
constexpr std::size_t kTxStressRateCount =
    sizeof(kTxStressRates) / sizeof(kTxStressRates[0]);
constexpr uint8_t kTxStressDefaultRateIndex = 1;
constexpr uint8_t kTxStressChannel = 15;  // MIDI CH16, zero based.
constexpr uint8_t kTxStressNote = 60;
constexpr uint8_t kTxStressVelocity = 100;
constexpr UBaseType_t kTxStressQueueDepth = 64;

struct TxStressPacket {
    uint32_t generation{0};
    bool noteOn{false};
};

struct TxStressRuntime {
    bool ready{false};
    bool active{false};
    bool nextNoteOn{true};
    bool cleanupPending{false};
    uint8_t rateIndex{kTxStressDefaultRateIndex};
    uint32_t generation{1};
    uint32_t accepted{0};
    uint32_t rejected{0};
    uint32_t endpointBusy{0};
    uint64_t startedAtUs{0};
    uint64_t stoppedAtUs{0};
    uint64_t firstBusyAtUs{0};
};

QueueHandle_t g_txStressQueue = nullptr;
esp_timer_handle_t g_txStressTimer = nullptr;
void (*g_notifyDispatcher)() = nullptr;
portMUX_TYPE g_txStressMux = portMUX_INITIALIZER_UNLOCKED;
TxStressRuntime g_txStress;

uint64_t txStressNowUs() {
    return static_cast<uint64_t>(esp_timer_get_time());
}

uint16_t txStressRateLocked() {
    const std::size_t index =
        g_txStress.rateIndex < kTxStressRateCount
  ? g_txStress.rateIndex
  : kTxStressDefaultRateIndex;
    return kTxStressRates[index];
}

void resetTxStressCountersLocked(uint64_t nowUs) {
    g_txStress.accepted = 0;
    g_txStress.rejected = 0;
    g_txStress.endpointBusy = 0;
    g_txStress.firstBusyAtUs = 0;
    g_txStress.startedAtUs = g_txStress.active ? nowUs : 0;
    g_txStress.stoppedAtUs = g_txStress.active ? 0 : nowUs;
}

void txStressTimerCallback(void*) {
    TxStressPacket packet{};
    bool enqueue = false;

    portENTER_CRITICAL(&g_txStressMux);
    if (g_txStress.ready && g_txStress.active) {
        packet.generation = g_txStress.generation;
        packet.noteOn = g_txStress.nextNoteOn;
        g_txStress.nextNoteOn = !g_txStress.nextNoteOn;
        enqueue = true;
    }
    portEXIT_CRITICAL(&g_txStressMux);

    if (!enqueue || g_txStressQueue == nullptr) return;

    if (xQueueSend(g_txStressQueue, &packet, 0) != pdPASS) {
        portENTER_CRITICAL(&g_txStressMux);
        if (g_txStress.active &&
  packet.generation == g_txStress.generation) {
  ++g_txStress.rejected;
        }
        portEXIT_CRITICAL(&g_txStressMux);
    }

    if (g_notifyDispatcher != nullptr) {
        g_notifyDispatcher();
    }
}

bool startTxStressTimer(uint16_t rateMessagesPerSecond) {
    if (g_txStressTimer == nullptr || rateMessagesPerSecond == 0) return false;
    const uint64_t periodUs =
        1000000ULL / static_cast<uint64_t>(rateMessagesPerSecond);
    return esp_timer_start_periodic(g_txStressTimer, periodUs) == ESP_OK;
}

void stopTxStressTimer() {
    if (g_txStressTimer != nullptr) {
        (void)esp_timer_stop(g_txStressTimer);
    }
}

void markTxStressReconfiguredLocked(bool active, uint64_t nowUs) {
    ++g_txStress.generation;
    g_txStress.active = active;
    g_txStress.nextNoteOn = true;
    g_txStress.cleanupPending = true;
    resetTxStressCountersLocked(nowUs);
}

}  // namespace

bool beginCardputerUsbMidiTxStress(void (*notifyDispatcher)()) {
    if (g_txStressQueue != nullptr && g_txStressTimer != nullptr) {
        portENTER_CRITICAL(&g_txStressMux);
        g_notifyDispatcher = notifyDispatcher;
        g_txStress.ready = true;
        portEXIT_CRITICAL(&g_txStressMux);
        return true;
    }

    g_txStressQueue =
        xQueueCreate(kTxStressQueueDepth, sizeof(TxStressPacket));
    if (g_txStressQueue == nullptr) return false;

    esp_timer_create_args_t timerArgs{};
    timerArgs.callback = txStressTimerCallback;
    timerArgs.dispatch_method = ESP_TIMER_TASK;
    timerArgs.name = "usbMidiTxStress";
    if (esp_timer_create(&timerArgs, &g_txStressTimer) != ESP_OK) {
        vQueueDelete(g_txStressQueue);
        g_txStressQueue = nullptr;
        g_txStressTimer = nullptr;
        return false;
    }

    portENTER_CRITICAL(&g_txStressMux);
    g_notifyDispatcher = notifyDispatcher;
    g_txStress.ready = true;
    portEXIT_CRITICAL(&g_txStressMux);
    return true;
}

void drainCardputerUsbMidiTxStress(IUsbMidiTransport& transport,
                         std::size_t budget) {
    if (g_txStressQueue == nullptr || budget == 0) return;

    bool cleanupPending = false;
    portENTER_CRITICAL(&g_txStressMux);
    if (g_txStress.cleanupPending) {
        g_txStress.cleanupPending = false;
        cleanupPending = true;
    }
    portEXIT_CRITICAL(&g_txStressMux);

    if (cleanupPending) {
        (void)transport.sendNoteOff(kTxStressChannel, kTxStressNote, 0);
    }

    TxStressPacket packet{};
    std::size_t drained = 0;
    while (drained < budget &&
 xQueueReceive(g_txStressQueue, &packet, 0) == pdPASS) {
        ++drained;

        bool current = false;
        portENTER_CRITICAL(&g_txStressMux);
        current = packet.generation == g_txStress.generation &&
        g_txStress.active;
        portEXIT_CRITICAL(&g_txStressMux);
        if (!current) continue;

        const bool mountedBefore = transport.mounted();
        const bool accepted = packet.noteOn
  ? transport.sendNoteOn(
        kTxStressChannel, kTxStressNote, kTxStressVelocity)
  : transport.sendNoteOff(kTxStressChannel, kTxStressNote, 0);
        const uint64_t nowUs = txStressNowUs();

        portENTER_CRITICAL(&g_txStressMux);
        if (packet.generation == g_txStress.generation &&
  g_txStress.active) {
  if (accepted) {
      ++g_txStress.accepted;
  } else {
      ++g_txStress.rejected;
      if (mountedBefore) {
          ++g_txStress.endpointBusy;
          if (g_txStress.firstBusyAtUs == 0) {
              g_txStress.firstBusyAtUs = nowUs;
          }
      }
  }
        }
        portEXIT_CRITICAL(&g_txStressMux);
    }
}

bool setCardputerUsbMidiTxStressEnabled(bool enabled) {
    bool ready = false;
    bool alreadyInState = false;
    portENTER_CRITICAL(&g_txStressMux);
    ready = g_txStress.ready;
    alreadyInState = g_txStress.active == enabled;
    portEXIT_CRITICAL(&g_txStressMux);
    if (!ready) return false;
    if (alreadyInState) return true;

    stopTxStressTimer();
    const uint64_t nowUs = txStressNowUs();
    uint16_t rate = 0;
    portENTER_CRITICAL(&g_txStressMux);
    markTxStressReconfiguredLocked(enabled, nowUs);
    rate = txStressRateLocked();
    portEXIT_CRITICAL(&g_txStressMux);

    if (enabled && !startTxStressTimer(rate)) {
        portENTER_CRITICAL(&g_txStressMux);
        g_txStress.active = false;
        g_txStress.stoppedAtUs = txStressNowUs();
        portEXIT_CRITICAL(&g_txStressMux);
        if (g_notifyDispatcher != nullptr) g_notifyDispatcher();
        return false;
    }

    if (g_notifyDispatcher != nullptr) g_notifyDispatcher();
    return true;
}

bool stepCardputerUsbMidiTxStressRate(int direction) {
    if (direction == 0) return true;

    bool ready = false;
    bool active = false;
    uint8_t currentIndex = 0;
    portENTER_CRITICAL(&g_txStressMux);
    ready = g_txStress.ready;
    active = g_txStress.active;
    currentIndex = g_txStress.rateIndex;
    portEXIT_CRITICAL(&g_txStressMux);
    if (!ready) return false;

    int nextIndex = static_cast<int>(currentIndex) +
          (direction > 0 ? 1 : -1);
    if (nextIndex < 0) nextIndex = 0;
    if (nextIndex >= static_cast<int>(kTxStressRateCount)) {
        nextIndex = static_cast<int>(kTxStressRateCount) - 1;
    }
    if (nextIndex == currentIndex) return true;

    stopTxStressTimer();
    const uint64_t nowUs = txStressNowUs();
    uint16_t rate = 0;
    portENTER_CRITICAL(&g_txStressMux);
    g_txStress.rateIndex = static_cast<uint8_t>(nextIndex);
    markTxStressReconfiguredLocked(active, nowUs);
    rate = txStressRateLocked();
    portEXIT_CRITICAL(&g_txStressMux);

    if (active && !startTxStressTimer(rate)) {
        portENTER_CRITICAL(&g_txStressMux);
        g_txStress.active = false;
        g_txStress.stoppedAtUs = txStressNowUs();
        portEXIT_CRITICAL(&g_txStressMux);
        if (g_notifyDispatcher != nullptr) g_notifyDispatcher();
        return false;
    }

    if (g_notifyDispatcher != nullptr) g_notifyDispatcher();
    return true;
}

bool resetCardputerUsbMidiTxStressCounters() {
    bool ready = false;
    bool active = false;
    portENTER_CRITICAL(&g_txStressMux);
    ready = g_txStress.ready;
    active = g_txStress.active;
    portEXIT_CRITICAL(&g_txStressMux);
    if (!ready) return false;

    stopTxStressTimer();
    const uint64_t nowUs = txStressNowUs();
    uint16_t rate = 0;
    portENTER_CRITICAL(&g_txStressMux);
    markTxStressReconfiguredLocked(active, nowUs);
    rate = txStressRateLocked();
    portEXIT_CRITICAL(&g_txStressMux);

    if (active && !startTxStressTimer(rate)) {
        portENTER_CRITICAL(&g_txStressMux);
        g_txStress.active = false;
        g_txStress.stoppedAtUs = txStressNowUs();
        portEXIT_CRITICAL(&g_txStressMux);
        if (g_notifyDispatcher != nullptr) g_notifyDispatcher();
        return false;
    }

    if (g_notifyDispatcher != nullptr) g_notifyDispatcher();
    return true;
}

CardputerUsbMidiTxStressSnapshot snapshotCardputerUsbMidiTxStress() {
    CardputerUsbMidiTxStressSnapshot snapshot{};
    uint64_t startedAtUs = 0;
    uint64_t stoppedAtUs = 0;
    uint64_t firstBusyAtUs = 0;

    portENTER_CRITICAL(&g_txStressMux);
    snapshot.active = g_txStress.active;
    snapshot.rateMessagesPerSecond = txStressRateLocked();
    snapshot.accepted = g_txStress.accepted;
    snapshot.rejected = g_txStress.rejected;
    snapshot.endpointBusy = g_txStress.endpointBusy;
    startedAtUs = g_txStress.startedAtUs;
    stoppedAtUs = g_txStress.stoppedAtUs;
    firstBusyAtUs = g_txStress.firstBusyAtUs;
    portEXIT_CRITICAL(&g_txStressMux);

    if (startedAtUs != 0) {
        uint64_t endUs = firstBusyAtUs;
        if (endUs == 0) {
  endUs = snapshot.active ? txStressNowUs() : stoppedAtUs;
        }
        if (endUs >= startedAtUs) {
  snapshot.stallFreeSeconds = static_cast<uint32_t>(
      (endUs - startedAtUs) / 1000000ULL);
        }
    }
    snapshot.oneBasedChannel = kTxStressChannel + 1;
    snapshot.note = kTxStressNote;
    return snapshot;
}
