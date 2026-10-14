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
enum class NotePlanStatus : uint8_t {
    Ok = 0, InvalidContext, InvalidGesture, FallbackNoHarmony, Count
};
enum class VoiceLeadingDirection : uint8_t { Nearest = 0, NearestUp, NearestDown, Count };

struct PerformanceGesture {
    uint8_t note{60};
    uint8_t velocity{100};
    BassFunction bassFunction{BassFunction::Root};
};

struct PerformanceContext {
    GroovePuterRhythm::HarmonicEvent harmony{};
    // Snapshot event identity is an absolute event ordinal. Keep the full
    // domain so repeated equal chords remain distinct past one byte.
    uint32_t sourceOrdinal{0};
    uint16_t scalePitchClasses{0x0fffu};
    uint16_t harmonyPitchClasses{0};
    uint16_t harmonyTriadPitchClasses{0};
    uint8_t rootMidi{60};
    uint8_t previousNote{60};
    uint8_t minNote{12};
    uint8_t maxNote{95};
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
    if (context.hasHarmony && context.harmonyPitchClasses != 0) {
        valid = (context.harmonyPitchClasses & 0xf000u) == 0;
        return valid ? context.harmonyPitchClasses : 0;
    }
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

inline bool voiceLeadNote(uint8_t reference,
                          uint16_t pitchClasses,
                          uint8_t minimum,
                          uint8_t maximum,
                          VoiceLeadingDirection direction,
                          uint8_t& result) {
    if (minimum > maximum || maximum > 127u || (pitchClasses & 0xf000u) != 0 ||
        direction >= VoiceLeadingDirection::Count) return false;
    uint8_t best = 0;
    uint16_t bestDistance = 256;
    for (uint16_t candidate = minimum; candidate <= maximum; ++candidate) {
        const uint8_t note = static_cast<uint8_t>(candidate);
        if ((pitchClasses & static_cast<uint16_t>(1u << pitchClass(note))) == 0) continue;
        if (direction == VoiceLeadingDirection::NearestUp && note < reference) continue;
        if (direction == VoiceLeadingDirection::NearestDown && note > reference) continue;
        const uint16_t distance = note > reference ? note - reference : reference - note;
        if (distance < bestDistance ||
            (distance == bestDistance &&
             (direction == VoiceLeadingDirection::NearestDown
                  ? note > best : direction == VoiceLeadingDirection::NearestUp
                      ? note < best : note < best))) {
            best = note;
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

inline bool voiceLeadNote(uint8_t reference,
                          uint16_t pitchClasses,
                          uint8_t minimum,
                          uint8_t maximum,
                          VoiceLeadingDirection direction,
                          uint8_t& result) {
    return detail::voiceLeadNote(reference, pitchClasses, minimum, maximum,
                                 direction, result);
}

// External MIDI pitches express BASS functions by matching pitch classes in
// the current chord: root, third, fifth, seventh, then the semitone below the
// root as APPROACH. A pitch outside that vocabulary is rejected instead of
// being silently reinterpreted as ROOT.
inline bool bassFunctionForInputNote(const PerformanceContext& context,
                                     uint8_t note,
                                     BassFunction& function) {
    if (!context.hasHarmony || note > 127u || context.rootMidi > 127u) return false;
    const uint8_t relativePitchClass = static_cast<uint8_t>(
        (detail::pitchClass(note) + 12u - detail::pitchClass(context.rootMidi)) % 12u);
    if (context.harmonyPitchClasses != 0) {
        if (relativePitchClass == 0u) {
            function = BassFunction::Root;
            return true;
        }
        const uint16_t triad = context.harmonyTriadPitchClasses;
        if ((relativePitchClass == 3u || relativePitchClass == 4u) &&
            (triad & static_cast<uint16_t>(1u << detail::pitchClass(note))) != 0u) {
            function = BassFunction::Third;
            return true;
        }
        if ((relativePitchClass == 6u || relativePitchClass == 7u ||
             relativePitchClass == 8u) &&
            (triad & static_cast<uint16_t>(1u << detail::pitchClass(note))) != 0u) {
            function = BassFunction::Fifth;
            return true;
        }
        const bool chordTone = (context.harmonyPitchClasses & static_cast<uint16_t>(
            1u << detail::pitchClass(note))) != 0u;
        if (relativePitchClass == 10u && chordTone) {
            function = BassFunction::Seventh;
            return true;
        }
        if (relativePitchClass == 11u) {
            function = chordTone ? BassFunction::Seventh : BassFunction::Approach;
            return true;
        }
        return false;
    }

    uint8_t intervals[5]{};
    uint8_t intervalCount = 0;
    if (!GroovePuterRhythm::chordQualityIntervals(
            context.harmony.quality, intervals, 5, intervalCount)) return false;
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
    bool noHarmonyFallback = false;
    if (role == PerformanceRole::Bass) {
        if (!context.hasHarmony) {
            // Harmony is optional; keep standalone BASS playable and report
            // that the scale-only fallback is active.
            if (context.scalePitchClasses == 0 ||
                !voiceLeadNote(gesture.note, context.scalePitchClasses,
                               context.minNote, context.maxNote,
                               VoiceLeadingDirection::Nearest, interpreted)) {
                plan.status = NotePlanStatus::InvalidContext;
                return plan;
            }
            noHarmonyFallback = true;
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
            if (context.harmonyPitchClasses != 0) {
                const uint8_t rootPc = detail::pitchClass(context.rootMidi);
                uint8_t relative = 0;
                bool found = gesture.bassFunction == BassFunction::Root;
                const uint16_t triad = context.harmonyTriadPitchClasses;
                const uint8_t first = gesture.bassFunction == BassFunction::Third ? 3u
                    : gesture.bassFunction == BassFunction::Fifth ? 6u
                    : gesture.bassFunction == BassFunction::Seventh ? 10u : 0u;
                const uint8_t last = gesture.bassFunction == BassFunction::Third ? 4u
                    : gesture.bassFunction == BassFunction::Fifth ? 8u
                    : gesture.bassFunction == BassFunction::Seventh ? 11u : 0u;
                const uint16_t sourceMask = gesture.bassFunction == BassFunction::Seventh
                    ? context.harmonyPitchClasses : triad;
                for (uint8_t interval = first; !found && interval <= last; ++interval) {
                    const uint8_t pc = static_cast<uint8_t>((rootPc + interval) % 12u);
                    if ((sourceMask & static_cast<uint16_t>(1u << pc)) != 0u) {
                        relative = interval;
                        found = true;
                    }
                }
                if (gesture.bassFunction == BassFunction::Root) relative = 0;
                const uint8_t targetPc = static_cast<uint8_t>((rootPc + relative) % 12u);
                if (!found || (!context.allowChromatic &&
                    (context.scalePitchClasses & static_cast<uint16_t>(1u << targetPc)) == 0u) ||
                    !detail::nearestNoteForPitchClass(context.rootMidi, targetPc,
                        context.minNote, context.maxNote, interpreted)) {
                    plan.status = NotePlanStatus::InvalidContext;
                    return plan;
                }
                plan.status = NotePlanStatus::Ok;
                plan.count = 1;
                plan.notes[0] = PlannedNote{interpreted, gesture.velocity};
                return plan;
            }
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
        if (!voiceLeadNote(reference, allowed, context.minNote, context.maxNote,
                           VoiceLeadingDirection::Nearest, interpreted)) {
            plan.status = NotePlanStatus::InvalidContext;
            return plan;
        }
    }

    plan.status = noHarmonyFallback ? NotePlanStatus::FallbackNoHarmony
                                    : NotePlanStatus::Ok;
    plan.count = 1;
    plan.notes[0] = PlannedNote{interpreted, gesture.velocity};
    return plan;
}

static_assert(sizeof(NotePlan) <= 16, "Smart note plan must remain bounded");

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_SMART_PLAY_H
