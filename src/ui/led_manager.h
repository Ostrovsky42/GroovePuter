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

    static uint8_t peakBrightness_(uint8_t configuredBrightness,
                                   uint8_t sourceBoost = 0);
    static uint16_t decayDuration_(uint16_t configuredFlashMs);
    void setLedColor(Rgb8 color, uint8_t brightness);
    bool publishPulse_(const LedPulseEvent& event);
    bool consumePulse_(LedPulseEvent& event);
    void startPulse_(const LedPulseEvent& event, uint32_t nowMs);
    void renderPulse_(uint32_t nowMs);
    void renderRestingState_();

    // 0 = empty, 1 = ready for consumer, 2 = producer/consumer owns payload.
    // The payload is accessed only by the thread that successfully changes
    // the state to 2, which prevents concurrent non-atomic struct access.
    std::atomic<uint8_t> ledPulseState_{0};
    LedPulseEvent ledPulse_{};

    Rgb8 pulseColor_{0, 0, 0};
    uint8_t pulsePeakBrightness_ = 0;
    uint32_t pulseStartedMs_ = 0;
    uint16_t pulseDurationMs_ = 0;
    bool isPulsing_ = false;
    bool muteStateDirty_ = false;
    bool lastMuteActive_ = false;
    LedSettings lastSettings_;

    uint32_t lastUpdateMs_ = 0;
    uint32_t lastPulseRenderMs_ = 0;
};
