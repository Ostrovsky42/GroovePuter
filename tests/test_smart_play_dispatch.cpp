#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"
#include "src/generation/harmony/harmonic_snapshot.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    int count(MusicalEventType type, uint8_t note) const {
        int result = 0;
        for (const MusicalEvent& event : events)
            result += event.type == type && event.note == note ? 1 : 0;
        return result;
    }
    std::vector<MusicalEvent> events;
};

PerformanceInputIdentity midi(uint8_t note, uint8_t source = 1) {
    return PerformanceInputIdentity{1, source, 0, note, 0};
}

GroovePuterPerformance::PerformanceContext chord(uint8_t root,
                                                GroovePuterRhythm::ChordQuality quality) {
    GroovePuterPerformance::PerformanceContext context{};
    context.hasHarmony = true;
    context.rootMidi = root;
    context.minNote = 12;
    context.maxNote = 95;
    context.scalePitchClasses = 0x0ab5u; // C major
    context.harmony = GroovePuterRhythm::HarmonicEvent{0, quality, 0};
    context.allowChromatic = false;
    return context;
}
}

int main() {
    using namespace GroovePuterPerformance;

    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    PerformanceKeyboard keyboard(router);
    keyboard.setVoiceMode(PerformanceVoiceMode::Poly);

    // RAW is an exact pass-through at both ends of the MIDI range.
    for (uint8_t note : {uint8_t{0}, uint8_t{11}, uint8_t{12}, uint8_t{64},
                         uint8_t{95}, uint8_t{96}, uint8_t{127}}) {
        auto owner = midi(note);
        assert(keyboard.dispatchIntent(makeNotePerformanceIntent(
            PerformanceIntentKind::PlayNote, owner, note, 100)));
        assert(sink.count(MusicalEventType::NoteOn, note) == 1);
        assert(keyboard.dispatchIntent(makeNotePerformanceIntent(
            PerformanceIntentKind::ReleaseNote, owner, note)));
        assert(sink.count(MusicalEventType::NoteOff, note) == 1);
        sink.events.clear();
    }

    // A LEAD note is interpreted once; release uses the emitted pitch even if
    // the live role/harmony changes while the key is held.
    keyboard.setPerformanceContext(chord(65, GroovePuterRhythm::ChordQuality::Major7));
    assert(keyboard.setPerformanceRole(PerformanceRole::Lead));
    assert(keyboard.setLeadMode(LeadMode::VoiceLead));
    PerformanceInputIdentity lead = midi(64);
    assert(keyboard.dispatchIntent(makeNotePerformanceIntent(
        PerformanceIntentKind::PlayNote, lead, 64, 100)));
    assert(sink.count(MusicalEventType::NoteOn, 64) == 1);
    keyboard.clearPerformanceHarmonyContext();
    assert(keyboard.setPerformanceRole(PerformanceRole::Raw));
    assert(keyboard.dispatchIntent(makeNotePerformanceIntent(
        PerformanceIntentKind::ReleaseNote, lead, 64)));
    assert(sink.count(MusicalEventType::NoteOff, 64) == 1);

    // BASS root follows the current event (C -> A -> F -> G).
    keyboard.setPerformanceRole(PerformanceRole::Bass);
    const uint8_t roots[] = {60, 69, 65, 67};
    const uint8_t inputNotes[] = {60, 57, 53, 55};
    for (std::size_t i = 0; i < 4; ++i) {
        keyboard.setPerformanceContext(chord(roots[i], GroovePuterRhythm::ChordQuality::Triad));
        auto owner = midi(inputNotes[i]);
        assert(keyboard.dispatchIntent(makeNotePerformanceIntent(
            PerformanceIntentKind::PlayNote, owner, inputNotes[i], 90)));
        assert(sink.count(MusicalEventType::NoteOn, roots[i]) == 1);
        assert(keyboard.dispatchIntent(makeNotePerformanceIntent(
            PerformanceIntentKind::ReleaseNote, owner, inputNotes[i])));
        assert(sink.count(MusicalEventType::NoteOff, roots[i]) == 1);
        sink.events.clear();
    }
    PerformanceContext m9 = chord(60, GroovePuterRhythm::ChordQuality::Minor9);
    GroovePuterPerformance::BassFunction function{};
    assert(!bassFunctionForInputNote(m9, 62, function)); // 9th is not the chordal third.

    keyboard.setPerformanceRole(PerformanceRole::Bass);
    PerformanceContext scaleFallback{};
    scaleFallback.scalePitchClasses = 0x0ab5u;
    keyboard.setPerformanceContext(scaleFallback);
    auto fallbackOwner = midi(61);
    assert(keyboard.externalNoteOn(61, 90, fallbackOwner));
    assert(sink.count(MusicalEventType::NoteOn, 60) == 1);
    assert(keyboard.lastNotePlanStatus() == NotePlanStatus::FallbackNoHarmony);
    assert(keyboard.externalNoteOff(61, fallbackOwner));
    sink.events.clear();

    MusicalEventRouter voiceRouter;
    RecordingSink voiceSink;
    assert(voiceRouter.addSink(voiceSink));
    PerformanceKeyboard voiceKeyboard(voiceRouter);
    voiceKeyboard.setPerformanceRole(PerformanceRole::Lead);
    assert(voiceKeyboard.setLeadMode(LeadMode::VoiceLead));
    PerformanceContext firstVoice = chord(65, GroovePuterRhythm::ChordQuality::Major7);
    firstVoice.allowChromatic = true;
    voiceKeyboard.setPerformanceContext(firstVoice);
    auto firstVoiceOwner = midi(65);
    assert(voiceKeyboard.externalNoteOn(65, 90, firstVoiceOwner));
    assert(voiceSink.count(MusicalEventType::NoteOn, 65) == 1);
    assert(voiceKeyboard.externalNoteOff(65, firstVoiceOwner));
    PerformanceContext nextVoice = chord(67, GroovePuterRhythm::ChordQuality::Major7);
    nextVoice.allowChromatic = true;
    voiceKeyboard.setPerformanceContext(nextVoice);
    auto nextVoiceOwner = midi(72, 2);
    assert(voiceKeyboard.externalNoteOn(72, 90, nextVoiceOwner));
    assert(voiceSink.count(MusicalEventType::NoteOn, 66) == 1); // nearest common voice: F#.

    // Cardputer and external MIDI use the same interpreter: 'a' maps to C2,
    // while BASS expresses the current C root at the register chosen by context.
    keyboard.setPerformanceContext(chord(60, GroovePuterRhythm::ChordQuality::Triad));
    assert(keyboard.keyDown('a', 100));
    auto cardputerEvents = sink.events;
    assert(sink.count(MusicalEventType::NoteOn, 60) == 1);
    assert(keyboard.keyUp('a'));
    sink.events.clear();
    auto owner = midi(60);
    assert(keyboard.externalNoteOn(60, 100, owner));
    assert(sink.count(MusicalEventType::NoteOn, 60) == 1);
    assert(keyboard.externalNoteOff(60, owner));
    assert(sink.count(MusicalEventType::NoteOff, 60) == 1);
    assert(!cardputerEvents.empty());

    // Lead SCALE never emits pitch classes outside the current tonal mask.
    keyboard.setPerformanceRole(PerformanceRole::Lead);
    assert(keyboard.setLeadMode(LeadMode::Scale));
    keyboard.setPerformanceContext(chord(60, GroovePuterRhythm::ChordQuality::Triad));
    const uint16_t allowed = 0x0ab5u;
    for (uint8_t note = 60; note < 72; ++note) {
        auto leadOwner = midi(note);
        assert(keyboard.externalNoteOn(note, 90, leadOwner));
        const MusicalEvent* attack = nullptr;
        for (const MusicalEvent& event : sink.events)
            if (event.type == MusicalEventType::NoteOn) attack = &event;
        assert(attack != nullptr);
        assert((allowed & static_cast<uint16_t>(1u << (attack->note % 12u))) != 0);
        assert(keyboard.externalNoteOff(note, leadOwner));
        sink.events.clear();
    }
    PerformanceContext emptyScale = chord(60, GroovePuterRhythm::ChordQuality::Triad);
    emptyScale.scalePitchClasses = 0;
    keyboard.setPerformanceContext(emptyScale);
    auto refusedOwner = midi(61);
    assert(!keyboard.externalNoteOn(61, 90, refusedOwner));
    assert(keyboard.lastNotePlanStatus() == NotePlanStatus::InvalidContext);
    assert(sink.events.empty());
    keyboard.setPerformanceContext(chord(60, GroovePuterRhythm::ChordQuality::Triad));

    // CHORD role delegates expansion to the existing PerformanceKeyboard chord
    // performer; it does not create a second chord realization path.
    keyboard.setPerformanceRole(PerformanceRole::Chord);
    keyboard.setPerformanceContext(chord(60, GroovePuterRhythm::ChordQuality::Dominant7));
    keyboard.setChordMode(PerformanceChordMode::Dominant7);
    auto chordOwner = midi(60);
    assert(keyboard.externalNoteOn(60, 90, chordOwner));
    int chordAttacks = 0;
    for (const MusicalEvent& event : sink.events) {
        chordAttacks += event.type == MusicalEventType::NoteOn;
        assert(event.type != MusicalEventType::NoteOn || event.note % 12u != 10u);
    }
    assert(chordAttacks == 3); // C7 is constrained to the current C-major contract.
    assert(keyboard.externalNoteOff(60, chordOwner));
    sink.events.clear();

    // The validated HGS snapshot is copied to the bounded performer context.
    GroovePuterHarmony::HarmonySnapshot snapshot{};
    snapshot.key.materialId = 1;
    snapshot.key.publicationGeneration = 1;
    snapshot.durationSteps = 16;
    snapshot.status = GroovePuterHarmony::HarmonyStatus::Ready;
    snapshot.base.qualityId = GroovePuterHarmony::kQualityMajor7;
    snapshot.base.rootPc = 5;
    snapshot.base.pitchMask = GroovePuterHarmony::transposePitchSet(
        GroovePuterHarmony::qualityDefinition(snapshot.base.qualityId)->intervalMask, 5);
    snapshot.tonal.scaleId = 1;
    snapshot.tonal.homeScaleId = 1;
    snapshot.tonal.tonicPc = 0;
    snapshot.tonal.homeTonicPc = 0;
    snapshot.tonal.scaleMask = GroovePuterHarmony::transposePitchSet(
        GroovePuterHarmony::scaleDefinition(1)->intervalMask, 0);
    snapshot.tonal.homeScaleMask = snapshot.tonal.scaleMask;
    snapshot.onsetStep = 24; // Absolute project step: performer context stores step-in-bar.
    for (auto& policy : snapshot.roles) {
        policy.allowed = snapshot.tonal.scaleMask;
        policy.preferred = snapshot.tonal.scaleMask;
    }
    assert(keyboard.setPerformanceRole(PerformanceRole::Lead));
    assert(keyboard.setLeadMode(LeadMode::Chord));
    assert(keyboard.setHarmonySnapshot(snapshot));
    assert(keyboard.performanceContextSnapshot().eventOnset == 8);
    auto snapshotOwner = midi(64);
    assert(keyboard.externalNoteOn(64, 90, snapshotOwner));
    assert(sink.count(MusicalEventType::NoteOn, 64) == 1); // E is a Fmaj7 tone.
    assert(keyboard.externalNoteOff(64, snapshotOwner));
    snapshot.roles[static_cast<uint8_t>(GroovePuterHarmony::HarmonicRole::Bass)] = {};
    assert(keyboard.setPerformanceRole(PerformanceRole::Bass));
    assert(keyboard.setHarmonySnapshot(snapshot));
    auto forbiddenBass = midi(65);
    assert(!keyboard.externalNoteOn(65, 90, forbiddenBass));
    assert(keyboard.lastNotePlanStatus() == NotePlanStatus::InvalidContext);

    // A valid HGS quality outside the legacy enum must be rejected instead
    // of inheriting Triad voicing semantics through the compatibility bridge.
    snapshot.base.qualityId = GroovePuterHarmony::kQualityAugmented;
    snapshot.base.rootPc = 0;
    snapshot.base.pitchMask = GroovePuterHarmony::transposePitchSet(
        GroovePuterHarmony::qualityDefinition(snapshot.base.qualityId)->intervalMask, 0);
    snapshot.tonal.scaleId = 9;
    snapshot.tonal.homeScaleId = 9;
    snapshot.tonal.scaleMask = GroovePuterHarmony::kPitchSet12Mask;
    snapshot.tonal.homeScaleMask = GroovePuterHarmony::kPitchSet12Mask;
    const uint16_t restrictedChord = static_cast<uint16_t>(
        (1u << 0) | (1u << 4) | (1u << 8));
    for (auto& policy : snapshot.roles) {
        policy.allowed = GroovePuterHarmony::kPitchSet12Mask;
        policy.preferred = GroovePuterHarmony::kPitchSet12Mask;
    }
    snapshot.roles[static_cast<uint8_t>(GroovePuterHarmony::HarmonicRole::Bass)] = {};
    snapshot.roles[static_cast<uint8_t>(GroovePuterHarmony::HarmonicRole::Chord)] = {
        restrictedChord, restrictedChord};
    const PerformanceContext contextBeforeAugmented = keyboard.performanceContextSnapshot();
    assert(!keyboard.setHarmonySnapshot(snapshot));
    const PerformanceContext contextAfterAugmented = keyboard.performanceContextSnapshot();
    assert(contextAfterAugmented.hasHarmony == contextBeforeAugmented.hasHarmony);
    assert(contextAfterAugmented.sourceOrdinal == contextBeforeAugmented.sourceOrdinal);
    assert(contextAfterAugmented.harmonyPitchClasses ==
           contextBeforeAugmented.harmonyPitchClasses);

    // Minor is also unrepresentable by the legacy quality enum.
    GroovePuterHarmony::HarmonySnapshot unsupportedQuality = snapshot;
    unsupportedQuality.base.qualityId = GroovePuterHarmony::kQualityMinor;
    unsupportedQuality.base.rootPc = 0;
    unsupportedQuality.base.pitchMask = GroovePuterHarmony::transposePitchSet(
        GroovePuterHarmony::qualityDefinition(unsupportedQuality.base.qualityId)->intervalMask, 0);
    for (auto& policy : unsupportedQuality.roles) {
        policy.allowed = unsupportedQuality.tonal.scaleMask;
        policy.preferred = unsupportedQuality.tonal.scaleMask;
    }
    const PerformanceContext contextBeforeUnsupported = keyboard.performanceContextSnapshot();
    assert(!keyboard.setHarmonySnapshot(unsupportedQuality));
    const PerformanceContext contextAfterUnsupported = keyboard.performanceContextSnapshot();
    assert(contextAfterUnsupported.hasHarmony == contextBeforeUnsupported.hasHarmony);
    assert(contextAfterUnsupported.sourceOrdinal == contextBeforeUnsupported.sourceOrdinal);
    assert(contextAfterUnsupported.harmonyPitchClasses ==
           contextBeforeUnsupported.harmonyPitchClasses);

    // Enabling arp from RAW moves the visible performance role to ARP.
    assert(keyboard.setPerformanceRole(PerformanceRole::Raw));
    PerformanceIntent arpIntent{};
    arpIntent.kind = PerformanceIntentKind::ToggleArp;
    arpIntent.value = 1;
    assert(keyboard.dispatchIntent(arpIntent));
    assert(keyboard.performanceRole() == PerformanceRole::Arp);
    assert(keyboard.arpeggiatorEnabled());
    assert(keyboard.setPerformanceRole(PerformanceRole::Raw));
    assert(!keyboard.arpeggiatorEnabled());

    // Selecting a role cannot mutate sequenced material: no material API is
    // exposed by the performer, and only a newly dispatched note is affected.
    return 0;
}
