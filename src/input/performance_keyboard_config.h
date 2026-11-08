#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_KEYBOARD_CONFIG_H
#define GROOVEPUTER_PERFORMANCE_KEYBOARD_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "musical_event.h"
#include "performance_instrument_types.h"
#include "performance_pulse.h"
#include "performance_smart_play.h"

// Stable, value-only configuration shared by the Cardputer, MIDI and desktop
// adapters. It intentionally excludes held notes, harmony snapshots and
// project material.
struct PerformanceKeyboardConfig {
    GroovePuterPerformance::PerformanceRole role{
        GroovePuterPerformance::PerformanceRole::Raw};
    GroovePuterPerformance::LeadMode leadMode{
        GroovePuterPerformance::LeadMode::Raw};
    GroovePuterPerformance::ArpPitchSource arpPitchSource{
        GroovePuterPerformance::ArpPitchSource::Held};
    GroovePuterPerformance::HarmonyChangePolicy harmonyChangePolicy{
        GroovePuterPerformance::HarmonyChangePolicy::Follow};
    PerformanceArpDirection arpDirection{PerformanceArpDirection::Up};
    PerformanceRate arpRate{PerformanceRate::Sixteenth};
    uint8_t arpOctaves{1};
    uint8_t gatePercent{60};
    PerformanceChordMode chordMode{PerformanceChordMode::Off};
    uint8_t chordInversion{0};
    PerformanceSpread chordSpread{PerformanceSpread::Close};
    PerformanceVoiceLeading voiceLeading{PerformanceVoiceLeading::Off};
    uint8_t rootPitchClass{0};
    PerformanceScale scale{PerformanceScale::NaturalMinor};
    uint8_t velocity{100};
    int8_t octaveShift{0};
    bool arpEnabled{false};
    bool latchEnabled{false};
    uint8_t ratchetCount{1};
    uint8_t euclideanLength{16};
    uint8_t euclideanPulses{0};
    uint8_t euclideanRotation{0};
    uint8_t strumMs{0};
    PerformanceStrumDirection strumDirection{PerformanceStrumDirection::LowToHigh};
    PerformanceVoiceMode voiceMode{PerformanceVoiceMode::Mono};
    MusicalEventTarget target{MusicalEventTarget::SynthA};
    uint32_t mutationSeed{0x47505631u};
    uint8_t mutationSkipPercent{0};
    uint8_t mutationOctaveJumpPercent{0};
    uint8_t mutationDeviatePercent{0};
};

static_assert(std::is_trivially_copyable<PerformanceKeyboardConfig>::value,
              "performance keyboard config is a bounded value type");

inline bool performanceKeyboardConfigEqual(const PerformanceKeyboardConfig& a,
                                           const PerformanceKeyboardConfig& b) {
    return a.role == b.role && a.leadMode == b.leadMode &&
        a.arpPitchSource == b.arpPitchSource &&
        a.harmonyChangePolicy == b.harmonyChangePolicy &&
        a.arpDirection == b.arpDirection && a.arpRate == b.arpRate &&
        a.arpOctaves == b.arpOctaves && a.gatePercent == b.gatePercent &&
        a.chordMode == b.chordMode && a.chordInversion == b.chordInversion &&
        a.chordSpread == b.chordSpread && a.voiceLeading == b.voiceLeading &&
        a.rootPitchClass == b.rootPitchClass && a.scale == b.scale &&
        a.velocity == b.velocity && a.octaveShift == b.octaveShift &&
        a.arpEnabled == b.arpEnabled && a.latchEnabled == b.latchEnabled &&
        a.ratchetCount == b.ratchetCount &&
        a.euclideanLength == b.euclideanLength &&
        a.euclideanPulses == b.euclideanPulses &&
        a.euclideanRotation == b.euclideanRotation && a.strumMs == b.strumMs &&
        a.strumDirection == b.strumDirection && a.voiceMode == b.voiceMode &&
        a.target == b.target && a.mutationSeed == b.mutationSeed &&
        a.mutationSkipPercent == b.mutationSkipPercent &&
        a.mutationOctaveJumpPercent == b.mutationOctaveJumpPercent &&
        a.mutationDeviatePercent == b.mutationDeviatePercent;
}

inline bool validPerformanceKeyboardConfig(const PerformanceKeyboardConfig& c) {
    return c.role < GroovePuterPerformance::PerformanceRole::Count &&
        c.leadMode < GroovePuterPerformance::LeadMode::Count &&
        c.arpPitchSource < GroovePuterPerformance::ArpPitchSource::Count &&
        c.harmonyChangePolicy < GroovePuterPerformance::HarmonyChangePolicy::Count &&
        c.arpDirection < PerformanceArpDirection::Count &&
        c.arpRate < PerformanceRate::Count && c.arpOctaves >= 1 &&
        c.arpOctaves <= 4 && c.gatePercent >= 1 && c.gatePercent <= 100 &&
        c.chordMode < PerformanceChordMode::Count && c.chordInversion <= 3 &&
        c.chordSpread < PerformanceSpread::Count &&
        c.voiceLeading < PerformanceVoiceLeading::Count &&
        c.rootPitchClass < 12 && c.scale < PerformanceScale::Count &&
        c.velocity >= 10 && c.velocity <= 120 && c.octaveShift >= -2 &&
        c.octaveShift <= 2 && c.ratchetCount >= 1 && c.ratchetCount <= 4 &&
        c.euclideanLength >= 1 && c.euclideanLength <= 16 &&
        c.euclideanPulses <= c.euclideanLength &&
        c.euclideanRotation < c.euclideanLength && c.strumMs <= 48 &&
        c.strumDirection < PerformanceStrumDirection::Count &&
        c.voiceMode < PerformanceVoiceMode::Count &&
        c.target <= MusicalEventTarget::Dx && c.mutationSeed != 0 &&
        c.mutationSkipPercent <= 100 && c.mutationOctaveJumpPercent <= 100 &&
        c.mutationDeviatePercent <= 100 && (!c.latchEnabled || c.arpEnabled) &&
        !(c.voiceLeading == PerformanceVoiceLeading::Nearest && c.arpEnabled) &&
        !(c.strumMs > 0 && c.ratchetCount > 1) &&
        (c.strumMs == 0 || c.strumMs == 8 || c.strumMs == 16 ||
         c.strumMs == 24 || c.strumMs == 36);
}

class PerformanceKeyboardConfigCodec {
public:
    static constexpr std::size_t kLegacyRecordSize = 18;
    static constexpr std::size_t kLegacyV1RecordSize = 21;
    static constexpr std::size_t kCurrentRecordSize = 36;
    static constexpr uint8_t kCurrentVersion = 2;

    static bool encode(const PerformanceKeyboardConfig& config,
                       uint8_t* out, std::size_t capacity);
    static bool decode(const uint8_t* bytes, std::size_t size,
                       PerformanceKeyboardConfig& out);
};

#endif  // GROOVEPUTER_PERFORMANCE_KEYBOARD_CONFIG_H
