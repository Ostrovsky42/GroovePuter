#include "led_manager.h"
#include <Arduino.h>
#include <M5Cardputer.h>

LedManager& LedManager::instance() {
    static LedManager inst;
    return inst;
}

LedManager::LedManager() {
}

void LedManager::init() {
    // M5Cardputer.begin() already initializes RGB if configured
    setLedColor({0, 0, 0}, 0);
}

void LedManager::setLedColor(Rgb8 color, uint8_t brightness) {
    // M5Cardputer v1.1.1 doesn't expose RGB LED API
    // TODO: Use M5.Display.setBrightness() or custom GPIO control if needed
    // uint8_t r = (color.r * brightness) >> 8;
    // uint8_t g = (color.g * brightness) >> 8;
    // uint8_t b = (color.b * brightness) >> 8;
    // M5Cardputer.Rgb.setPixelColor(0, M5Cardputer.Rgb.Color(r, g, b));
    // M5Cardputer.Rgb.show();
    (void)color;
    (void)brightness;
}

void LedManager::onVoiceTriggered(VoiceId v, const LedSettings& settings) {
    if (settings.mode != LedMode::StepTrig) return;
    if (static_cast<uint8_t>(settings.source) != static_cast<uint8_t>(v)) return;

    LedPulseEvent e{ (uint32_t)millis(), settings.color, settings.brightness, settings.flashMs };

    // Single-slot aggregation
    if (!ledPulsePending_.load(std::memory_order_acquire)) {
        ledPulse_ = e;
        ledPulsePending_.store(true, std::memory_order_release);
    } else {
        // Boost existing pulse
        ledPulse_.brightness = std::max(ledPulse_.brightness, e.brightness);
        ledPulse_.durationMs = std::max(ledPulse_.durationMs, e.durationMs);
    }
}

void LedManager::onMuteChanged(bool muted, const LedSettings& settings) {
    if (settings.mode != LedMode::MuteState) return;
    lastMuteActive_ = !muted; // active if not muted
    lastSettings_ = settings;
    muteStateDirty_ = true;
}

void LedManager::onBeat(int step, const LedSettings& settings) {
    if (settings.mode != LedMode::Beat) return;
    
    // Pulse on every step or just downbeats? User said 1/5/9/13 or just every step.
    // Let's do a short pulse on every step for now.
    LedPulseEvent e{ (uint32_t)millis(), settings.color, settings.brightness, 20 };

    if (!ledPulsePending_.load(std::memory_order_acquire)) {
        ledPulse_ = e;
        ledPulsePending_.store(true, std::memory_order_release);
    }
}

void LedManager::update() {
    uint32_t now = millis();
    
    // Handle new pulse triggers
    if (ledPulsePending_.exchange(false)) {
        setLedColor(ledPulse_.color, ledPulse_.brightness);
        pulseEndMs_ = now + ledPulse_.durationMs;
        isPulsing_ = true;
        return;
    }

    // Handle pulse expiration
    if (isPulsing_ && now >= pulseEndMs_) {
        isPulsing_ = false;
        muteStateDirty_ = true; // Refresh mute state if that's the mode
    }

    // Handle MuteState or Idle (Off)
    if (!isPulsing_) {
        // If mode is MuteState, keep it dim
        // We only update if dirty or if settings changed (detected via dirty flag usually)
        if (muteStateDirty_ || (now - lastUpdateMs_ > 500)) {
            if (lastSettings_.mode == LedMode::MuteState) {
                if (lastMuteActive_) {
                    // Dimly lit if active
                    setLedColor(lastSettings_.color, lastSettings_.brightness / 4);
                } else {
                    setLedColor({0, 0, 0}, 0);
                }
            } else if (lastSettings_.mode == LedMode::Off) {
                setLedColor({0, 0, 0}, 0);
            } else {
                // StepTrig or Beat mode but not pulsing -> Off
                setLedColor({0, 0, 0}, 0);
            }
            muteStateDirty_ = false;
            lastUpdateMs_ = now;
        }
    }
}
