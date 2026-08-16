#include "led_manager.h"
#include "../platform/cardputer_adv_hardware.h"

#include <algorithm>

#if defined(ARDUINO)
#include <Arduino.h>
#include <M5Cardputer.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

namespace {
constexpr uint32_t kPulseRenderPeriodMs = 10;
constexpr uint8_t kMaximumPeakBrightness = 220;
constexpr uint16_t kMinimumDecayMs = 40;
constexpr uint16_t kMaximumDecayMs = 180;
}

LedManager& LedManager::instance() {
    static LedManager inst;
    return inst;
}

LedManager::LedManager() {
}

void LedManager::init() {
    setLedColor({0, 0, 0}, 0);
}

uint8_t LedManager::peakBrightness_(uint8_t configuredBrightness,
                                    uint8_t sourceBoost) {
    const uint16_t boosted = static_cast<uint16_t>(configuredBrightness) +
                             static_cast<uint16_t>(sourceBoost);
    const uint16_t peak = boosted * 2u;
    return static_cast<uint8_t>(
        std::min<uint16_t>(peak, kMaximumPeakBrightness));
}

uint16_t LedManager::decayDuration_(uint16_t configuredFlashMs) {
    uint32_t duration = static_cast<uint32_t>(configuredFlashMs) * 2u;
    duration = std::max<uint32_t>(duration, kMinimumDecayMs);
    duration = std::min<uint32_t>(duration, kMaximumDecayMs);
    return static_cast<uint16_t>(duration);
}

void LedManager::setLedColor(Rgb8 color, uint8_t brightness) {
    const uint8_t r = (uint16_t(color.r) * brightness) >> 8;
    const uint8_t g = (uint16_t(color.g) * brightness) >> 8;
    const uint8_t b = (uint16_t(color.b) * brightness) >> 8;

#if defined(ESP32)
    if (GroovePuterHardware::kRgbLedEnabled) {
        neopixelWrite(GroovePuterHardware::kRgbLedDataPin, r, g, b);
    }
#else
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

void LedManager::startPulse_(const LedPulseEvent& event, uint32_t nowMs) {
    pulseColor_ = event.color;
    pulsePeakBrightness_ = event.brightness;
    pulseStartedMs_ = nowMs;
    pulseDurationMs_ = event.durationMs;
    isPulsing_ = pulsePeakBrightness_ > 0 && pulseDurationMs_ > 0;
    lastPulseRenderMs_ = nowMs;

    if (isPulsing_) {
        setLedColor(pulseColor_, pulsePeakBrightness_);
    }
}

void LedManager::renderPulse_(uint32_t nowMs) {
    if (!isPulsing_) return;

    const uint32_t elapsed = nowMs - pulseStartedMs_;
    if (elapsed >= pulseDurationMs_) {
        isPulsing_ = false;
        setLedColor({0, 0, 0}, 0);
        lastUpdateMs_ = nowMs;
        return;
    }

    if ((nowMs - lastPulseRenderMs_) < kPulseRenderPeriodMs) return;
    lastPulseRenderMs_ = nowMs;

    const uint32_t remaining = pulseDurationMs_ - elapsed;
    const uint32_t numerator =
        static_cast<uint32_t>(pulsePeakBrightness_) * remaining * remaining;
    const uint32_t denominator =
        static_cast<uint32_t>(pulseDurationMs_) * pulseDurationMs_;
    const uint8_t brightness = denominator > 0
        ? static_cast<uint8_t>(numerator / denominator)
        : 0;

    setLedColor(pulseColor_, brightness);
}

void LedManager::renderRestingState_() {
    if (lastSettings_.mode == LedMode::MuteState && lastMuteActive_) {
        setLedColor(lastSettings_.color, lastSettings_.brightness / 4);
    } else {
        setLedColor({0, 0, 0}, 0);
    }
    muteStateDirty_ = false;
    lastUpdateMs_ = millis();
}

void LedManager::onVoiceTriggered(VoiceId v, const LedSettings& settings) {
    if (settings.mode != LedMode::StepTrig) return;
    if (static_cast<uint8_t>(settings.source) != static_cast<uint8_t>(v)) return;

    uint8_t sourceBoost = 0;
    switch (v) {
        case VoiceId::DrumKick: sourceBoost = 20; break;
        case VoiceId::DrumSnare: sourceBoost = 10; break;
        case VoiceId::DrumClap: sourceBoost = 8; break;
        default: break;
    }

    const LedPulseEvent event{
        static_cast<uint32_t>(millis()),
        settings.color,
        peakBrightness_(settings.brightness, sourceBoost),
        decayDuration_(settings.flashMs),
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
        peakBrightness_(settings.brightness, 12),
        decayDuration_(settings.flashMs),
    };
    publishPulse_(event);
}

void LedManager::update() {
    const uint32_t now = millis();

    LedPulseEvent event{};
    if (consumePulse_(event)) {
        startPulse_(event, now);
    }

    if (isPulsing_) {
        renderPulse_(now);
        return;
    }

    if (muteStateDirty_ || (now - lastUpdateMs_ > 500)) {
        renderRestingState_();
    }
}
