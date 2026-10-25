// SPDX-License-Identifier: MIT
// DUAL OSC (the experimental ANALOG engine, felucca_analog_voice.h): names of its Preset parameter. Index 0 is INIT
// (one saw); 1..12 are the 12 presets of Felucca ANALOG (eng_analog.c ANALOG_PRESETS), in their order.
#pragma once

inline constexpr int kDualOscPresetCount = 13;
inline constexpr const char* kDualOscPresetNames[kDualOscPresetCount] = {
    "INIT", "SAW LEAD", "SOFT PAD", "SQR BASS", "PWM STR", "ACID", "SINE KEY",
    "RAVE", "SUB BASS", "PLUCK", "BRASS", "WIND", "STRINGS"};
