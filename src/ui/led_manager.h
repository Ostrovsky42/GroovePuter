#pragma once

#include <stdint.h>
#include <atomic>
#include "../../scenes.h"

struct LedPulseEvent {
    uint32_t atMs;
    Rgb8 color;
    uint8_t brightness;
    uint16_t durationMs;
};

class LedManager {
public:
    static LedManager& instance();

    void init();
    void update();

    // Triggered from Audio/DSP logic. Events use a bounded single-slot
    // publication protocol and are dropped when the slot is busy.
    void onVoiceTriggered(VoiceId v, const LedSettings& settings);

    // Triggered from UI/Logic
    void onMuteChanged(bool muted, const LedSettings& settings);
    void onBeat(int step, const LedSettings& settings);

private:
    LedManager();

    void setLedColor(Rgb8 color, uint8_t brightness);
    bool publishPulse_(const LedPulseEvent& event);
    bool consumePulse_(LedPulseEvent& event);

    // 0 = empty, 1 = ready for consumer, 2 = producer/consumer owns payload.
    // The payload is accessed only by the thread that successfully changes
    // the state to 2, which prevents concurrent non-atomic struct access.
    std::atomic<uint8_t> ledPulseState_{0};
    LedPulseEvent ledPulse_{};

    uint32_t pulseEndMs_ = 0;
    bool isPulsing_ = false;
    bool muteStateDirty_ = false;
    bool lastMuteActive_ = false;
    LedSettings lastSettings_;

    uint32_t lastUpdateMs_ = 0;
};
