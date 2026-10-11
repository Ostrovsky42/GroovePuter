#pragma once
#ifndef GROOVEPUTER_CARDPUTER_PERFORMANCE_SCENE_STORE_H
#define GROOVEPUTER_CARDPUTER_PERFORMANCE_SCENE_STORE_H

#include "src/input/performance_scene.h"
#include "src/input/performance_keyboard.h"

namespace GroovePuterPlatform {

#ifdef ARDUINO
bool loadCardputerPerformanceScenes(GroovePuterPerformance::PerformanceSceneBank& bank);
bool saveCardputerPerformanceScenes(const GroovePuterPerformance::PerformanceSceneBank& bank);
bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig& config);
bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig& config);
#else
inline bool loadCardputerPerformanceScenes(GroovePuterPerformance::PerformanceSceneBank&) {
    return false;
}
inline bool saveCardputerPerformanceScenes(const GroovePuterPerformance::PerformanceSceneBank&) {
    return true;
}
inline bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig&) { return false; }
inline bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig&) { return true; }
#endif

} // namespace GroovePuterPlatform

#endif // GROOVEPUTER_CARDPUTER_PERFORMANCE_SCENE_STORE_H
