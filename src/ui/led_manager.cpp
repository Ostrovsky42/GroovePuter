#include "led_manager.h"
#include "../platform/cardputer_adv_hardware.h"

#include <algorithm>

#if defined(ARDUINO)
#include <Arduino.h>
#include <M5Cardputer.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

LedManager& LedManager::instance() {
    static LedManager inst;
    return inst;
}

LedManager::LedManager() {
}

void LedManager::init() {
    setLedColor({0, 0, 0}, 0);
}

void LedManager::setLedColor(Rgb8 color, uint8_t brightness) {
    const uint8_t r = (uint16_t(color.r) * brightness) >> 8;
    const uint8_t g = (uint16_t(color.g) * brightness) >> 8;
    const uint8_t b = (uint16_t(color.b) * brightness) >> 8;

#if defined(ESP32) && GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN >= 0
    neopixelWrite(GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN, r, g, b);
#else
    // Cardputer ADV uses GPIO21 as PA_EN. RGB output is deliberately disabled
    // until a distinct LED data pin is verified for this hardware profile.
    (void)r;
    (void)g;
    (void)b;
#endif
}

bool LedManager::publishPulse_(const LedPulseEvent& event) {
    uint8_t expected = 0;
    if (!ledPulseState_.compare_exchange_strong(
            expected, 2, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        return false;
    }

    ledPulse_ = event;
    ledPulseState_.store(1, std::memory_order_release);
    return true;
}

bool LedManager::consumePulse_(LedPulseEvent& event) {
    uint8_t expected = 1;
    if (!ledPulseState_.compare_exchange_strong(
            expected, 2, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        return false;
    }

    event = ledPulse_;
    ledPulseState_.store(0, std::memory_order_release);
    return true;
}

void LedManager::onVoiceTriggered(VoiceId v, const LedSettings& settings) {
    if (settings.mode != LedMode::StepTrig) return;
    if (static_cast<uint8_t>(settings.source) != static_cast<uint8_t>(v)) return;

    const LedPulseEvent event{
        static_cast<uint32_t>(millis()),
        settings.color,
        settings.brightness,
        settings.flashMs,
    };
    publishPulse_(event);
}

void LedManager::onMuteChanged(bool muted, const LedSettings& settings) {
    if (settings.mode != LedMode::MuteState) return;
    lastMuteActive_ = !muted;
    lastSettings_ = settings;
    muteStateDirty_ = true;
}

void LedManager::onBeat(int step, const LedSettings& settings) {
    (void)step;
    if (settings.mode != LedMode::Beat) return;

    const LedPulseEvent event{
        static_cast<uint32_t>(millis()),
        settings.color,
        settings.brightness,
        20,
    };
    publishPulse_(event);
}

void LedManager::update() {
    const uint32_t now = millis();

    LedPulseEvent event{};
    if (consumePulse_(event)) {
        setLedColor(event.color, event.brightness);
        pulseEndMs_ = now + event.durationMs;
        isPulsing_ = true;
        return;
    }

    if (isPulsing_ && now >= pulseEndMs_) {
        isPulsing_ = false;
        muteStateDirty_ = true;
    }

    if (!isPulsing_) {
        if (muteStateDirty_ || (now - lastUpdateMs_ > 500)) {
            if (lastSettings_.mode == LedMode::MuteState) {
                if (lastMuteActive_) {
                    setLedColor(lastSettings_.color, lastSettings_.brightness / 4);
                } else {
                    setLedColor({0, 0, 0}, 0);
                }
            } else {
                setLedColor({0, 0, 0}, 0);
            }
            muteStateDirty_ = false;
            lastUpdateMs_ = now;
        }
    }
}
