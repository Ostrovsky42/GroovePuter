#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_SMART_PLAY_H
#define GROOVEPUTER_PERFORMANCE_SMART_PLAY_H

#include <cstdint>

#include "../generation/roles/chord_progression.h"

namespace GroovePuterPerformance {

enum class PerformanceRole : uint8_t { Raw = 0, Bass, Lead, Chord, Arp, Count };
enum class LeadMode : uint8_t { Raw = 0, Scale, Chord, VoiceLead, Count };
enum class BassFunction : uint8_t { Root = 0, Third, Fifth, Seventh, Approach, Count };
enum class ArpPitchSource : uint8_t { Held = 0, Memory, Harmony, Role, Count };
enum class HarmonyChangePolicy : uint8_t { Follow = 0, Hold, Restart, Count };
enum class NotePlanStatus : uint8_t { Ok = 0, InvalidContext, InvalidGesture, Count };

struct PerformanceGesture {
    uint8_t note{60};
    uint8_t velocity{100};
    BassFunction bassFunction{BassFunction::Root};
};

struct PerformanceContext {
    GroovePuterRhythm::HarmonicEvent harmony{};
    uint16_t scalePitchClasses{0x0fffu};
    uint8_t rootMidi{60};
    uint8_t previousNote{60};
    uint8_t minNote{12};
    uint8_t maxNote{95};
    uint8_t sourceOrdinal{0};
    uint8_t eventOnset{0};
    uint8_t eventDuration{0};
    bool allowChromatic{true};
    bool hasHarmony{false};
    bool hasPreviousNote{false};
};
static_assert(sizeof(PerformanceContext) <= 24,
              "performance harmony context must remain a bounded snapshot");

struct PlannedNote {
    uint8_t note{0};
    uint8_t velocity{0};
};

struct NotePlan {
    static constexpr uint8_t kCapacity = 4;
    NotePlanStatus status{NotePlanStatus::InvalidGesture};
    uint8_t count{0};
    PlannedNote notes[kCapacity]{};
};

namespace detail {

inline uint8_t pitchClass(uint8_t note) { return static_cast<uint8_t>(note % 12u); }

inline bool nearestNoteForPitchClass(uint8_t reference,
                                     uint8_t wantedPitchClass,
                                     uint8_t minimum,
                                     uint8_t maximum,
                                     uint8_t& result) {
    uint8_t best = minimum;
    uint16_t bestDistance = 256;
    for (uint16_t candidate = minimum; candidate <= maximum; ++candidate) {
        if (pitchClass(static_cast<uint8_t>(candidate)) != wantedPitchClass) continue;
        const uint16_t distance = candidate > reference
            ? candidate - reference : reference - candidate;
        if (distance < bestDistance || (distance == bestDistance && candidate < best)) {
            best = static_cast<uint8_t>(candidate);
            bestDistance = distance;
        }
    }
    if (bestDistance == 256) return false;
    result = best;
    return true;
}

inline uint16_t chordPitchClassMask(const PerformanceContext& context, bool& valid) {
    uint8_t intervals[5]{};
    uint8_t count = 0;
    valid = context.hasHarmony && GroovePuterRhythm::chordQualityIntervals(
        context.harmony.quality, intervals, 5, count);
    if (!valid) return 0;
    uint16_t mask = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t pc = static_cast<uint8_t>((pitchClass(context.rootMidi) + intervals[i]) % 12u);
        mask = static_cast<uint16_t>(mask | (1u << pc));
    }
    return mask;
}

inline bool nearestNoteInMask(uint8_t reference,
                              uint16_t pitchClasses,
                              uint8_t minimum,
                              uint8_t maximum,
                              uint8_t& result) {
    uint8_t best = minimum;
    uint16_t bestDistance = 256;
    for (uint16_t candidate = minimum; candidate <= maximum; ++candidate) {
        const uint8_t pc = pitchClass(static_cast<uint8_t>(candidate));
        if ((pitchClasses & static_cast<uint16_t>(1u << pc)) == 0) continue;
        const uint16_t distance = candidate > reference
            ? candidate - reference : reference - candidate;
        if (distance < bestDistance || (distance == bestDistance && candidate < best)) {
            best = static_cast<uint8_t>(candidate);
            bestDistance = distance;
        }
    }
    if (bestDistance == 256) return false;
    result = best;
    return true;
}

inline bool nearestAllowedBelow(uint8_t reference,
                                uint16_t pitchClasses,
                                uint8_t minimum,
                                uint8_t maximum,
                                uint8_t& result) {
    if (reference <= minimum) return false;
    const uint16_t upper = reference < maximum ? reference : maximum;
    for (int candidate = static_cast<int>(upper) - 1;
         candidate >= static_cast<int>(minimum); --candidate) {
        const uint8_t pc = pitchClass(static_cast<uint8_t>(candidate));
        if ((pitchClasses & static_cast<uint16_t>(1u << pc)) != 0) {
            result = static_cast<uint8_t>(candidate);
            return true;
        }
    }
    return false;
}

}  // namespace detail

// External MIDI pitches express BASS functions by matching pitch classes in
// the current chord: root, third, fifth, seventh, then the semitone below the
// root as APPROACH. A pitch outside that vocabulary is rejected instead of
// being silently reinterpreted as ROOT.
inline bool bassFunctionForInputNote(const PerformanceContext& context,
                                     uint8_t note,
                                     BassFunction& function) {
    if (!context.hasHarmony || note > 127u || context.rootMidi > 127u) return false;
    uint8_t intervals[5]{};
    uint8_t intervalCount = 0;
    if (!GroovePuterRhythm::chordQualityIntervals(
            context.harmony.quality, intervals, 5, intervalCount)) return false;

    const uint8_t relativePitchClass = static_cast<uint8_t>(
        (detail::pitchClass(note) + 12u - detail::pitchClass(context.rootMidi)) % 12u);
    for (uint8_t i = 0; i < intervalCount; ++i) {
        if (relativePitchClass != intervals[i] % 12u) continue;
        if (i == 0u) function = BassFunction::Root;
        else if (i == 1u &&
                 context.harmony.quality != GroovePuterRhythm::ChordQuality::Sus4)
            function = BassFunction::Third;
        else if (i == 2u) function = BassFunction::Fifth;
        else if (i == 3u) function = BassFunction::Seventh;
        else return false;
        return true;
    }

    if (relativePitchClass == 11u) {
        function = BassFunction::Approach;
        return true;
    }
    return false;
}

inline NotePlan interpretNote(PerformanceRole role,
                              LeadMode leadMode,
                              const PerformanceContext& context,
                              const PerformanceGesture& gesture) {
    NotePlan plan{};
    if (role >= PerformanceRole::Count || leadMode >= LeadMode::Count ||
        gesture.note > 127u || gesture.velocity > 127u ||
        gesture.bassFunction >= BassFunction::Count) {
        plan.status = NotePlanStatus::InvalidGesture;
        return plan;
    }

    if (role == PerformanceRole::Raw ||
        (role == PerformanceRole::Lead && leadMode == LeadMode::Raw) ||
        role == PerformanceRole::Chord || role == PerformanceRole::Arp) {
        plan.status = NotePlanStatus::Ok;
        plan.count = 1;
        plan.notes[0] = PlannedNote{gesture.note, gesture.velocity};
        return plan;
    }

    if (context.minNote > context.maxNote || context.maxNote > 127u ||
        context.rootMidi > 127u || (context.scalePitchClasses & 0xf000u) != 0) {
        plan.status = NotePlanStatus::InvalidContext;
        return plan;
    }

    uint8_t interpreted = gesture.note;
    if (role == PerformanceRole::Bass) {
        if (!context.hasHarmony) {
            // Harmony is an optional read-only context. In standalone mode the
            // bass role remains playable and passes its scale-mapped gesture.
            interpreted = gesture.note;
        } else if (gesture.bassFunction == BassFunction::Approach) {
            if (context.scalePitchClasses == 0 ||
                !detail::nearestAllowedBelow(context.rootMidi,
                                             context.scalePitchClasses,
                                             context.minNote, context.maxNote,
                                             interpreted)) {
                plan.status = NotePlanStatus::InvalidContext;
                return plan;
            }
        } else {
            uint8_t intervals[5]{};
            uint8_t intervalCount = 0;
            if (!context.hasHarmony || !GroovePuterRhythm::chordQualityIntervals(
                    context.harmony.quality, intervals, 5, intervalCount)) {
                plan.status = NotePlanStatus::InvalidContext;
                return plan;
            }
            uint8_t intervalIndex = 0;
            switch (gesture.bassFunction) {
                case BassFunction::Root: intervalIndex = 0; break;
                case BassFunction::Third:
                    if (context.harmony.quality == GroovePuterRhythm::ChordQuality::Sus4 ||
                        intervalCount <= 1u) {
                        plan.status = NotePlanStatus::InvalidContext;
                        return plan;
                    }
                    intervalIndex = 1;
                    break;
                case BassFunction::Fifth:
                    if (intervalCount <= 2u) {
                        plan.status = NotePlanStatus::InvalidContext;
                        return plan;
                    }
                    intervalIndex = 2;
                    break;
                case BassFunction::Seventh:
                    if (intervalCount <= 3u) {
                        plan.status = NotePlanStatus::InvalidContext;
                        return plan;
                    }
                    intervalIndex = 3;
                    break;
                case BassFunction::Approach:
                case BassFunction::Count: break;
            }
            const uint16_t target = static_cast<uint16_t>(context.rootMidi) + intervals[intervalIndex];
            interpreted = target > 127u ? 127u : static_cast<uint8_t>(target);
            if (!context.allowChromatic &&
                (context.scalePitchClasses & static_cast<uint16_t>(
                    1u << detail::pitchClass(interpreted))) == 0u) {
                plan.status = NotePlanStatus::InvalidContext;
                return plan;
            }
        }
        if (!detail::nearestNoteForPitchClass(interpreted,
                                              detail::pitchClass(interpreted),
                                              context.minNote, context.maxNote,
                                              interpreted)) {
            plan.status = NotePlanStatus::InvalidContext;
            return plan;
        }
    } else {
        bool chordValid = false;
        const uint16_t chordMask = detail::chordPitchClassMask(context, chordValid);
        uint16_t allowed = context.scalePitchClasses;
        uint8_t reference = gesture.note;
        if (leadMode == LeadMode::Chord || leadMode == LeadMode::VoiceLead) {
            if (context.hasHarmony && !chordValid) {
                plan.status = NotePlanStatus::InvalidContext;
                return plan;
            }
            if (context.hasHarmony) {
                allowed = chordMask;
                if (!context.allowChromatic) {
                    allowed = static_cast<uint16_t>(allowed & context.scalePitchClasses);
                }
            }
        }
        if (leadMode == LeadMode::VoiceLead && context.hasPreviousNote) {
            reference = context.previousNote;
        }
        if (allowed == 0) {
            plan.status = NotePlanStatus::InvalidContext;
            return plan;
        }
        if (!detail::nearestNoteInMask(reference, allowed,
                                       context.minNote, context.maxNote,
                                       interpreted)) {
            plan.status = NotePlanStatus::InvalidContext;
            return plan;
        }
    }

    plan.status = NotePlanStatus::Ok;
    plan.count = 1;
    plan.notes[0] = PlannedNote{interpreted, gesture.velocity};
    return plan;
}

static_assert(sizeof(NotePlan) <= 16, "Smart note plan must remain bounded");

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_SMART_PLAY_H
