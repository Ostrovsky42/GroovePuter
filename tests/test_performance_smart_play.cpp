#include <cassert>
#include <cstdint>

#include "src/input/performance_smart_play.h"

int main() {
    using namespace GroovePuterRhythm;
    using namespace GroovePuterPerformance;

    PerformanceContext context{};
    context.rootMidi = 60;
    context.scalePitchClasses = 0x0ab5u; // C D E F G A B
    context.rootMidi = 65;
    context.harmony = HarmonicEvent{0, ChordQuality::Major7, 0};
    context.hasHarmony = true;
    context.previousNote = 64;
    context.hasPreviousNote = true;
    context.minNote = 48;
    context.maxNote = 84;

    PerformanceGesture gesture{69, 91, BassFunction::Root};
    NotePlan plan = interpretNote(PerformanceRole::Raw, LeadMode::VoiceLead, context, gesture);
    assert(plan.status == NotePlanStatus::Ok && plan.count == 1);
    assert(plan.notes[0].note == 69 && plan.notes[0].velocity == 91);

    plan = interpretNote(PerformanceRole::Raw, LeadMode::Raw, context,
                         PerformanceGesture{128, 91, BassFunction::Root});
    assert(plan.status == NotePlanStatus::InvalidGesture && plan.count == 0);

    plan = interpretNote(PerformanceRole::Lead, LeadMode::Scale, context,
                         PerformanceGesture{65, 77, BassFunction::Root});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 65); // F is in C major.

    context.rootMidi = 60;
    plan = interpretNote(PerformanceRole::Lead, LeadMode::Chord, context,
                         PerformanceGesture{66, 80, BassFunction::Root});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 67); // nearest Cmaj7 tone.

    context.harmony = HarmonicEvent{0, ChordQuality::Dominant7, 0};
    context.rootMidi = 60;
    context.allowChromatic = true;
    plan = interpretNote(PerformanceRole::Lead, LeadMode::Chord, context,
                         PerformanceGesture{70, 80, BassFunction::Root});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 70);
    context.allowChromatic = false;
    plan = interpretNote(PerformanceRole::Lead, LeadMode::Chord, context,
                         PerformanceGesture{70, 80, BassFunction::Root});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 72);
    plan = interpretNote(PerformanceRole::Bass, LeadMode::Raw, context,
                         PerformanceGesture{60, 80, BassFunction::Seventh});
    assert(plan.status == NotePlanStatus::InvalidContext); // Bb is outside genre scale.

    PerformanceContext triadContext{};
    triadContext.hasHarmony = true;
    triadContext.rootMidi = 60;
    triadContext.harmony = HarmonicEvent{0, ChordQuality::Triad, 0};
    plan = interpretNote(PerformanceRole::Bass, LeadMode::Raw, triadContext,
                         PerformanceGesture{60, 80, BassFunction::Seventh});
    assert(plan.status == NotePlanStatus::InvalidContext && plan.count == 0);

    triadContext.harmony.quality = ChordQuality::Sus4;
    plan = interpretNote(PerformanceRole::Bass, LeadMode::Raw, triadContext,
                         PerformanceGesture{60, 80, BassFunction::Third});
    assert(plan.status == NotePlanStatus::InvalidContext && plan.count == 0);

    context.rootMidi = 69;
    plan = interpretNote(PerformanceRole::Bass, LeadMode::Raw, context,
                         PerformanceGesture{44, 100, BassFunction::Root});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 69);

    context.rootMidi = 67;
    plan = interpretNote(PerformanceRole::Bass, LeadMode::Raw, context,
                         PerformanceGesture{67, 88, BassFunction::Approach});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 65);

    context.rootMidi = 65;
    context.harmony = HarmonicEvent{0, ChordQuality::Major7, 0};
    plan = interpretNote(PerformanceRole::Lead, LeadMode::VoiceLead, context,
                         PerformanceGesture{72, 100, BassFunction::Root});
    assert(plan.status == NotePlanStatus::Ok && plan.notes[0].note == 64); // E remains M7 over Fmaj7 below.

    uint8_t voice = 0;
    const uint16_t cMajor = static_cast<uint16_t>(0x0ab5u);
    assert(voiceLeadNote(61, cMajor, 60, 72,
                         VoiceLeadingDirection::Nearest, voice) && voice == 60);
    assert(voiceLeadNote(61, cMajor, 60, 72,
                         VoiceLeadingDirection::NearestUp, voice) && voice == 62);
    assert(voiceLeadNote(61, cMajor, 60, 72,
                         VoiceLeadingDirection::NearestDown, voice) && voice == 60);
    assert(!voiceLeadNote(72, cMajor, 60, 71,
                          VoiceLeadingDirection::NearestUp, voice));
    assert(!voiceLeadNote(65, 0, 60, 72,
                          VoiceLeadingDirection::Nearest, voice));
    return 0;
}
