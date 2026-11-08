#pragma once
#ifndef GROOVEPUTER_CARDPUTER_PERFORMANCE_CONFIG_STORE_H
#define GROOVEPUTER_CARDPUTER_PERFORMANCE_CONFIG_STORE_H

#include <cstdint>

#include "src/input/performance_keyboard.h"

namespace GroovePuterPlatform {

#ifdef ARDUINO
bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig& config);
bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig& config);
void initializeCardputerPerformanceConfig(PerformanceKeyboard& keyboard);
void serviceCardputerPerformanceConfig(PerformanceKeyboard& keyboard,
                                      uint32_t nowMs);
#else
inline bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig&) {
    return false;
}
inline bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig&) {
    return true;
}
inline void initializeCardputerPerformanceConfig(PerformanceKeyboard&) {}
inline void serviceCardputerPerformanceConfig(PerformanceKeyboard&, uint32_t) {}
#endif

}  // namespace GroovePuterPlatform

#endif  // GROOVEPUTER_CARDPUTER_PERFORMANCE_CONFIG_STORE_H
