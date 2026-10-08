#pragma once
#ifndef GROOVEPUTER_SRC_PLATFORM_BATTERY_STATUS_H
#define GROOVEPUTER_SRC_PLATFORM_BATTERY_STATUS_H

#include <cstdint>

#if defined(ARDUINO)
#include <M5Cardputer.h>
#else
#include "platform_sdl/arduino_compat.h"
#endif

// Battery for PROJECT -> DEVICE. On the Cardputer the level is M5Unified's
// estimate from the battery voltage (ADC on GPIO10, divider x2): it moves a
// few percent with load (audio, screen), so the voltage is shown beside it.
// There is no charge-status signal on this board (isCharging() is unknown),
// so no charging icon is claimed. Read at most every kRefreshMs.
namespace GroovePuterPlatform {

struct BatteryStatus {
    bool available{false};
    int8_t percent{0};        // 0..100
    uint16_t millivolts{0};
};

constexpr uint32_t kBatteryRefreshMs = 5000;

inline BatteryStatus readBatteryStatusNow() {
    BatteryStatus status{};
#if defined(ARDUINO)
    const int32_t level = M5.Power.getBatteryLevel();
    const int16_t millivolts = M5.Power.getBatteryVoltage();
    // No battery / no reading: the ADC reports (near) zero volts.
    if (level < 0 || millivolts < 2500) return status;
    status.available = true;
    status.percent = static_cast<int8_t>(level > 100 ? 100 : level);
    status.millivolts = static_cast<uint16_t>(millivolts);
#endif
    return status;
}

// UI thread only: cached so drawing every frame costs no ADC reads.
inline BatteryStatus batteryStatus() {
    static BatteryStatus cached{};
    static uint32_t lastReadMs = 0;
    static bool haveRead = false;
    const uint32_t now = millis();
    if (!haveRead || now - lastReadMs >= kBatteryRefreshMs) {
        cached = readBatteryStatusNow();
        lastReadMs = now;
        haveRead = true;
    }
    return cached;
}

}  // namespace GroovePuterPlatform

#endif  // GROOVEPUTER_SRC_PLATFORM_BATTERY_STATUS_H
