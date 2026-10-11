#pragma once
#ifndef GROOVEPUTER_CARDPUTER_PERFORMANCE_CONFIG_OWNER_H
#define GROOVEPUTER_CARDPUTER_PERFORMANCE_CONFIG_OWNER_H

#include <cstdint>

class PerformanceKeyboard;

namespace GroovePuterPlatform {

// Boot: restore the saved PERFORM keyboard record before any input can change it, so a MIDI CC
// that arrives before the PERFORM page is opened cannot save defaults over the user's record.
void restoreCardputerPerformanceConfig(PerformanceKeyboard& keyboard);

// Loop: save the keyboard record once a change has been quiet for the debounce window. Every input
// (PERFORM page, MIDI CC, future) goes through here, so none of them depends on a page being open.
void servicePerformanceConfigSave(PerformanceKeyboard& keyboard, uint32_t nowMs);

}  // namespace GroovePuterPlatform

#endif  // GROOVEPUTER_CARDPUTER_PERFORMANCE_CONFIG_OWNER_H
