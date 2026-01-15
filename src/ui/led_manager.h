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

    // Triggered from Audio/DSP logic (lock-free)
    void onVoiceTriggered(VoiceId v, const LedSettings& settings);
    
    // Triggered from UI/Logic
    void onMuteChanged(bool muted, const LedSettings& settings);
    void onBeat(int step, const LedSettings& settings);

private:
    LedManager();

    void setLedColor(Rgb8 color, uint8_t brightness);
    
    std::atomic<bool> ledPulsePending_{false};
    LedPulseEvent ledPulse_;
    
    uint32_t pulseEndMs_ = 0;
    bool isPulsing_ = false;
    bool muteStateDirty_ = false;
    bool lastMuteActive_ = false;
    LedSettings lastSettings_;
    
    uint32_t lastUpdateMs_ = 0;
};
