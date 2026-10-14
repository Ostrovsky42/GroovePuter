#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "src/input/performance_keyboard.h"
#include "src/midi/project_transport_timeline.h"

using namespace GroovePuterPerformance;

namespace {
class TraceSink final : public IMusicalEventSink {
public:
    struct Entry { uint32_t observedAt; uint64_t pulseOrdinal; MusicalEvent event; };
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(Entry{now, 0, event});
    }
    uint32_t now{0};
    std::vector<Entry> events;
};

PerformanceContext harmony(uint8_t root, GroovePuterRhythm::ChordQuality quality,
                           uint8_t ordinal) {
    PerformanceContext context{};
    context.hasHarmony = true;
    context.rootMidi = root;
    context.harmony = GroovePuterRhythm::HarmonicEvent{0, quality, 0};
    context.sourceOrdinal = ordinal;
    context.minNote = 48;
    context.maxNote = 84;
    context.scalePitchClasses = 0x0fffu;
    context.allowChromatic = true;
    return context;
}

GroovePuterHarmony::HarmonySnapshot snapshot(uint8_t rootPc, uint32_t ordinal,
                                              uint16_t roleMask) {
    GroovePuterHarmony::HarmonySnapshot value{};
    value.key.materialId = 1;
    value.key.publicationGeneration = 1;
    value.key.eventOrdinal = ordinal;
    value.durationSteps = 16;
    value.status = GroovePuterHarmony::HarmonyStatus::Ready;
    value.base.qualityId = GroovePuterHarmony::kQualityMajor7;
    value.base.rootPc = rootPc;
    value.base.pitchMask = GroovePuterHarmony::transposePitchSet(
        GroovePuterHarmony::qualityDefinition(value.base.qualityId)->intervalMask, rootPc);
    value.tonal.scaleId = 9;
    value.tonal.homeScaleId = 9;
    value.tonal.scaleMask = GroovePuterHarmony::kPitchSet12Mask;
    value.tonal.homeScaleMask = GroovePuterHarmony::kPitchSet12Mask;
    value.tonal.tonicPc = 0;
    value.tonal.homeTonicPc = 0;
    for (auto& role : value.roles) {
        role.allowed = GroovePuterHarmony::kPitchSet12Mask;
        role.preferred = GroovePuterHarmony::kPitchSet12Mask;
    }
    const auto chordRole = static_cast<uint8_t>(GroovePuterHarmony::HarmonicRole::Chord);
    value.roles[chordRole] = {roleMask, roleMask};
    return value;
}

void serviceThrough(PerformanceKeyboard& keyboard, uint32_t from, uint32_t through,
                    uint32_t interval = 1000) {
    for (uint32_t now = from; now <= through; now += interval) keyboard.service(now);
}

void serviceAt(PerformanceKeyboard& keyboard, TraceSink& sink, uint32_t now) {
    sink.now = now;
    const std::size_t firstNew = sink.events.size();
    keyboard.service(now);
    for (std::size_t i = firstNew; i < sink.events.size(); ++i)
        sink.events[i].pulseOrdinal = keyboard.musicalPulseOrdinal();
}

std::vector<uint8_t> noteOns(const TraceSink& sink) {
    std::vector<uint8_t> result;
    for (const auto& entry : sink.events) {
        if (entry.event.type == MusicalEventType::NoteOn &&
            entry.event.source == MusicalEventSource::Arpeggiator)
            result.push_back(entry.event.note);
    }
    return result;
}
}

int main() {
    // HOLD pins the source pool at the first harmony transition. Later updates
    // replace only the pending destination and cannot restart the cycle timer.
    for (uint8_t transitionPoint = 1; transitionPoint <= 4; ++transitionPoint) {
        MusicalEventRouter router;
        TraceSink sink;
        assert(router.addSink(sink));
        PerformanceKeyboard held(router);
        held.setTempoBpm(120.0f);
        assert(held.setArpPitchSource(ArpPitchSource::Harmony));
        assert(held.setHarmonyChangePolicy(HarmonyChangePolicy::Hold));
        held.setPerformanceContext(harmony(60, GroovePuterRhythm::ChordQuality::Major7, 1));
        held.setArpeggiatorEnabled(true);
        held.service(1000); // first pulse: C
        for (uint8_t pulse = 1; pulse < transitionPoint; ++pulse)
            held.service(1000u + static_cast<uint32_t>(pulse) * 125000u);
        const uint32_t updateAt = 2000u +
            static_cast<uint32_t>(transitionPoint - 1u) * 125000u;
        held.setPerformanceContext(harmony(62, GroovePuterRhythm::ChordQuality::Major7, 2));
        held.setPerformanceContext(harmony(64, GroovePuterRhythm::ChordQuality::Major7, 3));
        held.setPerformanceContext(harmony(65, GroovePuterRhythm::ChordQuality::Major7, 4));
        serviceThrough(held, updateAt, 510000u);
        const std::vector<uint8_t> heldTrace = noteOns(sink);
        assert(heldTrace.size() >= 5);
        assert(heldTrace[0] == 60 && heldTrace[1] == 64 && heldTrace[2] == 67 &&
               heldTrace[3] == 71 && heldTrace[4] == 65);
    }

    // HOLD also freezes the role mask that belonged to the old snapshot.
    // Repeated updates replace the pending snapshot without changing either
    // the active pool or the original boundary.
    MusicalEventRouter roleHoldRouter;
    TraceSink roleHoldSink;
    assert(roleHoldRouter.addSink(roleHoldSink));
    PerformanceKeyboard roleHold(roleHoldRouter);
    roleHold.setPerformanceRole(PerformanceRole::Chord);
    assert(roleHold.setArpPitchSource(ArpPitchSource::Role));
    assert(roleHold.setHarmonyChangePolicy(HarmonyChangePolicy::Hold));
    const uint16_t cMajor7 = static_cast<uint16_t>((1u << 0) | (1u << 4) |
                                                   (1u << 7) | (1u << 11));
    assert(roleHold.setHarmonySnapshot(snapshot(0, 1, cMajor7)));
    roleHold.setArpeggiatorEnabled(true);
    serviceAt(roleHold, roleHoldSink, 1000);
    assert(roleHold.setHarmonySnapshot(snapshot(2, 2, 0x0246u)));
    uint8_t heldPool[16]{};
    uint8_t heldVelocity[16]{};
    std::size_t heldCount = roleHold.collectArpeggioPool(heldPool, heldVelocity, 16);
    assert(heldCount == 4 && heldPool[0] == 60 && heldPool[1] == 64 &&
           heldPool[2] == 67 && heldPool[3] == 71);
    assert(roleHold.setHarmonySnapshot(snapshot(4, 3, 0x0918u)));
    assert(roleHold.setHarmonySnapshot(snapshot(5, 4, 0x0231u)));
    serviceThrough(roleHold, 2000, 510000);
    const auto roleHoldTrace = noteOns(roleHoldSink);
    assert(roleHoldTrace.size() >= 5 && roleHoldTrace[0] == 60 &&
           roleHoldTrace[1] == 64 && roleHoldTrace[2] == 67 &&
           roleHoldTrace[3] == 71 && roleHoldTrace[4] == 65);

    // FOLLOW keeps contour position modulo the new pool. RESTART changes the
    // next scheduled pulse to the first note and never emits synchronously.
    MusicalEventRouter followRouter;
    TraceSink followSink;
    assert(followRouter.addSink(followSink));
    PerformanceKeyboard follow(followRouter);
    follow.setArpeggiatorEnabled(true);
    assert(follow.setArpPitchSource(ArpPitchSource::Harmony));
    follow.setPerformanceContext(harmony(60, GroovePuterRhythm::ChordQuality::Major7, 10));
    followSink.now = 1000;
    follow.service(1000);
    follow.setPerformanceContext(harmony(62, GroovePuterRhythm::ChordQuality::Triad, 11));
    assert(noteOns(followSink).size() == 1);
    serviceThrough(follow, 2000, 130000);
    const auto followNotes = noteOns(followSink);
    assert(followNotes.size() >= 2 && followNotes[0] == 60 && followNotes[1] == 66);

    MusicalEventRouter restartRouter;
    TraceSink restartSink;
    assert(restartRouter.addSink(restartSink));
    PerformanceKeyboard restart(restartRouter);
    assert(restart.setArpPitchSource(ArpPitchSource::Harmony));
    assert(restart.setHarmonyChangePolicy(HarmonyChangePolicy::Restart));
    restart.setPerformanceContext(harmony(60, GroovePuterRhythm::ChordQuality::Major7, 20));
    restart.setArpeggiatorEnabled(true);
    restart.service(1000);
    restart.setPerformanceContext(harmony(62, GroovePuterRhythm::ChordQuality::Triad, 21));
    assert(noteOns(restartSink).size() == 1); // no immediate extra pulse
    serviceThrough(restart, 2000, 130000);
    const auto restartNotes = noteOns(restartSink);
    assert(restartNotes.size() >= 2 && restartNotes[0] == 60 && restartNotes[1] == 62);

    // Same event redelivery is inert under RESTART; a new event with the same
    // chord restarts at the next pulse.
    MusicalEventRouter identityRouter;
    TraceSink identitySink;
    assert(identityRouter.addSink(identitySink));
    PerformanceKeyboard identity(identityRouter);
    assert(identity.setArpPitchSource(ArpPitchSource::Harmony));
    assert(identity.setHarmonyChangePolicy(HarmonyChangePolicy::Restart));
    const auto sameEvent = harmony(60, GroovePuterRhythm::ChordQuality::Major7, 30);
    identity.setPerformanceContext(sameEvent);
    identity.setArpeggiatorEnabled(true);
    identity.service(1000);
    identity.setPerformanceContext(sameEvent);
    serviceThrough(identity, 2000, 130000);
    auto identityNotes = noteOns(identitySink);
    assert(identityNotes.size() >= 2 && identityNotes[0] == 60 && identityNotes[1] == 64);
    identity.setPerformanceContext(harmony(60, GroovePuterRhythm::ChordQuality::Major7, 31));
    serviceThrough(identity, 131000, 260000);
    identityNotes = noteOns(identitySink);
    assert(identityNotes.size() >= 3 && identityNotes[2] == 60);

    MusicalEventRouter snapshotRouter;
    TraceSink snapshotSink;
    assert(snapshotRouter.addSink(snapshotSink));
    PerformanceKeyboard snapshotArp(snapshotRouter);
    snapshotArp.setPerformanceRole(PerformanceRole::Chord);
    assert(snapshotArp.setArpPitchSource(ArpPitchSource::Role));
    assert(snapshotArp.setHarmonyChangePolicy(HarmonyChangePolicy::Restart));
    // The source event identity stays distinct after the old uint8_t
    // saturation point: 256 and 272 are separate same-quality events.
    const auto firstSnapshot = snapshot(0, 256, 0x0891u);
    assert(snapshotArp.setHarmonySnapshot(firstSnapshot));
    snapshotArp.setArpeggiatorEnabled(true);
    serviceAt(snapshotArp, snapshotSink, 1000);
    assert(snapshotArp.setHarmonySnapshot(firstSnapshot));
    serviceThrough(snapshotArp, 2000, 130000);
    auto snapshotNotes = noteOns(snapshotSink);
    assert(snapshotNotes.size() >= 2 && snapshotNotes[0] == 60 && snapshotNotes[1] == 64);
    assert(snapshotArp.setHarmonySnapshot(snapshot(0, 272, 0x0891u)));
    serviceThrough(snapshotArp, 131000, 260000);
    snapshotNotes = noteOns(snapshotSink);
    assert(snapshotNotes.size() >= 3 && snapshotNotes[2] == 60);

    // Equal pitches/ordinal still belong to a new event when any source-key
    // field changes. Republishing the identical full key must continue phase.
    for (unsigned field = 0; field < 3; ++field) {
        MusicalEventRouter keyRouter;
        TraceSink keySink;
        assert(keyRouter.addSink(keySink));
        PerformanceKeyboard keyed(keyRouter);
        keyed.setPerformanceRole(PerformanceRole::Chord);
        assert(keyed.setArpPitchSource(ArpPitchSource::Role));
        assert(keyed.setHarmonyChangePolicy(HarmonyChangePolicy::Restart));
        auto source = snapshot(0, 300, 0x0891u);
        assert(keyed.setHarmonySnapshot(source));
        keyed.setArpeggiatorEnabled(true);
        serviceAt(keyed, keySink, 1000);
        assert(keyed.setHarmonySnapshot(source));
        serviceThrough(keyed, 2000, 130000);
        auto notes = noteOns(keySink);
        assert(notes.size() >= 2 && notes[0] == 60 && notes[1] == 64);
        if (field == 0) ++source.key.materialId;
        if (field == 1) ++source.key.publicationGeneration;
        if (field == 2) ++source.key.transportEpoch;
        assert(keyed.setHarmonySnapshot(source));
        serviceThrough(keyed, 131000, 260000);
        notes = noteOns(keySink);
        assert(notes.size() >= 3 && notes[2] == 60);
    }

    // Context loss clears HOLD immediately and releases only generated notes.
    MusicalEventRouter lossRouter;
    TraceSink lossSink;
    assert(lossRouter.addSink(lossSink));
    PerformanceKeyboard loss(lossRouter);
    assert(loss.setArpPitchSource(ArpPitchSource::Harmony));
    assert(loss.setHarmonyChangePolicy(HarmonyChangePolicy::Hold));
    loss.setPerformanceContext(harmony(60, GroovePuterRhythm::ChordQuality::Major7, 40));
    loss.setArpeggiatorEnabled(true);
    loss.service(1000);
    loss.setPerformanceContext(harmony(62, GroovePuterRhythm::ChordQuality::Major7, 41));
    loss.clearPerformanceHarmonyContext();
    int generatedOffs = 0;
    for (const auto& entry : lossSink.events)
        generatedOffs += entry.event.type == MusicalEventType::NoteOff &&
            entry.event.source == MusicalEventSource::Arpeggiator && entry.event.note == 60;
    assert(generatedOffs == 1);
    serviceThrough(loss, 2000, 510000);
    assert(noteOns(lossSink).size() == 1);

    // MEMORY uses its interval shape only while a root is held. It does not
    // invent a root or fall back to HELD when its provider is absent.
    MusicalEventRouter memoryRouter;
    TraceSink memorySink;
    assert(memoryRouter.addSink(memorySink));
    PerformanceKeyboard memory(memoryRouter);
    assert(memory.externalNoteOn(60, 70));
    assert(memory.externalNoteOn(64, 90));
    assert(memory.externalNoteOn(67, 110));
    assert(memory.captureChordMemory());
    assert(memory.setArpPitchSource(ArpPitchSource::Memory));
    uint8_t pool[16]{};
    uint8_t velocities[16]{};
    std::size_t poolCount = memory.collectArpeggioPool(pool, velocities, 16);
    assert(poolCount >= 3 && pool[0] == 60 && pool[1] == 64 && pool[2] == 67);
    assert(velocities[0] == 70 && velocities[1] == 70 && velocities[2] == 70);
    memory.releaseAllExternalNotes();
    assert(memory.collectArpeggioPool(pool, velocities, 16) == 0);
    memorySink.events.clear();
    assert(memory.externalNoteOn(60, 70));
    memory.setArpeggiatorEnabled(true);
    serviceAt(memory, memorySink, 1000);
    serviceThrough(memory, 2000, 260000);
    const auto memoryTrace = noteOns(memorySink);
    assert(memoryTrace.size() >= 3 && memoryTrace[0] == 60 &&
           memoryTrace[1] == 64 && memoryTrace[2] == 67);
    PerformanceKeyboard emptyMemory(memoryRouter);
    assert(emptyMemory.setArpPitchSource(ArpPitchSource::Memory));
    assert(emptyMemory.externalNoteOn(60, 100));
    assert(emptyMemory.collectArpeggioPool(pool, velocities, 16) == 0);

    // ROLE is harmony-backed and can run without a held keyboard note.
    MusicalEventRouter roleRouter;
    TraceSink roleSink;
    assert(roleRouter.addSink(roleSink));
    PerformanceKeyboard role(roleRouter);
    role.setPerformanceRole(PerformanceRole::Lead);
    PerformanceContext roleContext = harmony(60, GroovePuterRhythm::ChordQuality::Major7, 50);
    roleContext.scalePitchClasses = 0x0ab5u;
    roleContext.minNote = 48;
    roleContext.maxNote = 84;
    role.setPerformanceContext(roleContext);
    assert(role.setArpPitchSource(ArpPitchSource::Role));
    role.setArpeggiatorEnabled(true);
    role.service(1000);
    serviceThrough(role, 2000, 260000);
    const auto roleTrace = noteOns(roleSink);
    assert(roleTrace.size() >= 3 && roleTrace[0] == 48 &&
           roleTrace[1] == 50 && roleTrace[2] == 52);

    // Octave expansion remains bounded from one through the configured max.
    for (uint8_t octaves = 1; octaves <= 4; ++octaves) {
        MusicalEventRouter octaveRouter;
        TraceSink octaveSink;
        assert(octaveRouter.addSink(octaveSink));
        PerformanceKeyboard octave(octaveRouter);
        assert(octave.externalNoteOn(48, 100));
        octave.setArpOctaves(octaves);
        octave.setArpeggiatorEnabled(true);
        octave.service(1000);
        poolCount = octave.collectArpeggioPool(pool, velocities, 16);
        assert(poolCount == octaves);
        for (uint8_t i = 0; i < octaves; ++i)
            assert(pool[i] == static_cast<uint8_t>(48 + i * 12));
    }

    MusicalEventRouter capacityRouter;
    TraceSink capacitySink;
    assert(capacityRouter.addSink(capacitySink));
    PerformanceKeyboard capacityKeyboard(capacityRouter);
    for (uint8_t note = 48; note < 56; ++note)
        assert(capacityKeyboard.externalNoteOn(note, static_cast<uint8_t>(60 + note)));
    capacityKeyboard.setArpOctaves(4);
    capacityKeyboard.setArpeggiatorEnabled(true);
    capacityKeyboard.service(1000);
    poolCount = capacityKeyboard.collectArpeggioPool(pool, velocities, 16);
    assert(poolCount == PerformanceKeyboard::kMaxPolyChordNotes);
    assert(capacityKeyboard.collectArpeggioPool(pool, velocities, 1) == 1);

    MusicalEventRouter clippedRouter;
    TraceSink clippedSink;
    assert(clippedRouter.addSink(clippedSink));
    PerformanceKeyboard clipped(clippedRouter);
    assert(clipped.externalNoteOn(127, 100));
    poolCount = clipped.collectArpeggioPool(pool, velocities, 16);
    assert(poolCount == 1 && pool[0] <= PerformanceKeyboard::kMaxNote);
    assert(clipped.externalNoteOff(127));

    // All six rates remain independent of contour. Observed onset intervals
    // follow the existing local fallback clock within one polling interval.
    for (uint8_t rateIndex = 0; rateIndex < static_cast<uint8_t>(PerformanceRate::Count);
         ++rateIndex) {
        MusicalEventRouter rateRouter;
        TraceSink rateSink;
        assert(rateRouter.addSink(rateSink));
        PerformanceKeyboard rateKeyboard(rateRouter);
        assert(rateKeyboard.externalNoteOn(60, 70));
        assert(rateKeyboard.externalNoteOn(64, 90));
        assert(rateKeyboard.externalNoteOn(67, 110));
        const auto rate = static_cast<PerformanceRate>(rateIndex);
        rateKeyboard.setArpRate(rate);
        rateKeyboard.setArpeggiatorEnabled(true);
        serviceAt(rateKeyboard, rateSink, 1000);
        const uint32_t duration = static_cast<uint32_t>(
            performancePulseSteps(rate) * 125000.0 + 0.5);
        const uint32_t poll = 1000;
        uint32_t now = 1000;
        while (noteOns(rateSink).size() < 4 && now < 1000 + duration * 5) {
            now += poll;
            serviceAt(rateKeyboard, rateSink, now);
        }
        std::vector<uint32_t> onsetTimes;
        for (const auto& entry : rateSink.events)
            if (entry.event.type == MusicalEventType::NoteOn &&
                entry.event.source == MusicalEventSource::Arpeggiator)
                onsetTimes.push_back(entry.observedAt);
        assert(onsetTimes.size() >= 4);
        for (std::size_t i = 1; i < 4; ++i) {
            const uint32_t delta = onsetTimes[i] - onsetTimes[i - 1];
            const uint32_t error = delta > duration ? delta - duration : duration - delta;
            assert(error <= poll * 2);
        }
    }

    // Every contour has a stable bounded trace, including the legacy DOWNUP.
    const uint8_t expected[][6] = {
        {60, 64, 67, 60, 64, 67}, // UP
        {67, 64, 60, 67, 64, 60}, // DOWN
        {60, 64, 67, 64, 60, 64}, // UPDOWN
        {67, 64, 60, 64, 67, 64}, // DOWNUP
        {67, 60, 64, 67, 60, 64}, // AS PLAYED, insertion order preserved
        {0, 0, 0, 0, 0, 0},       // RANDOM, compared across equivalent runs
    };
    auto runContour = [&](uint8_t directionIndex, uint32_t pollMicros = 1000,
                          bool repeatMissingContextClear = false) {
        MusicalEventRouter contourRouter;
        TraceSink contourSink;
        assert(contourRouter.addSink(contourSink));
        PerformanceKeyboard contour(contourRouter);
        contour.setTempoBpm(120.0f);
        contour.setArpRate(PerformanceRate::ThirtySecond);
        for (uint8_t i = 0; i < directionIndex; ++i) contour.cycleArpDirection();
        const uint8_t inputNotes[] = {60, 64, 67};
        if (directionIndex == 4) {
            assert(contour.externalNoteOn(67, 110));
            assert(contour.externalNoteOn(60, 70));
            assert(contour.externalNoteOn(64, 90));
        } else {
            assert(contour.externalNoteOn(inputNotes[0], 70));
            assert(contour.externalNoteOn(inputNotes[1], 90));
            assert(contour.externalNoteOn(inputNotes[2], 110));
        }
        contour.setArpeggiatorEnabled(true);
        uint32_t now = 1000;
        if (repeatMissingContextClear) contour.clearPerformanceHarmonyContext();
        serviceAt(contour, contourSink, now);
        const uint32_t duration = 62500;
        while (noteOns(contourSink).size() < 6 && now < 1000 + duration * 8) {
            now += pollMicros;
            if (repeatMissingContextClear) contour.clearPerformanceHarmonyContext();
            serviceAt(contour, contourSink, now);
        }
        for (uint32_t releaseAt = now + pollMicros;
             releaseAt <= now + 50000u; releaseAt += pollMicros) {
            if (repeatMissingContextClear) contour.clearPerformanceHarmonyContext();
            serviceAt(contour, contourSink, releaseAt);
        }
        std::vector<uint8_t> result;
        uint64_t previousPulse = 0;
        std::size_t arpNoteOns = 0;
        std::size_t arpNoteOffs = 0;
        for (const auto& entry : contourSink.events)
            if (entry.event.source == MusicalEventSource::Arpeggiator &&
                entry.event.type == MusicalEventType::NoteOn) {
                assert(entry.pulseOrdinal >= previousPulse);
                previousPulse = entry.pulseOrdinal;
                ++arpNoteOns;
                result.push_back(entry.event.note);
            } else if (entry.event.source == MusicalEventSource::Arpeggiator &&
                       entry.event.type == MusicalEventType::NoteOff) {
                ++arpNoteOffs;
            }
        assert(arpNoteOns == arpNoteOffs);
        return result;
    };
    for (uint8_t direction = 0; direction < 5; ++direction) {
        const auto trace = runContour(direction);
        assert(trace.size() >= 6);
        for (std::size_t i = 0; i < 6; ++i) assert(trace[i] == expected[direction][i]);
    }
    MusicalEventRouter velocityRouter;
    TraceSink velocitySink;
    assert(velocityRouter.addSink(velocitySink));
    PerformanceKeyboard velocityKeyboard(velocityRouter);
    assert(velocityKeyboard.externalNoteOn(64, 90));
    assert(velocityKeyboard.externalNoteOn(67, 110));
    assert(velocityKeyboard.externalNoteOn(60, 70));
    velocityKeyboard.setArpeggiatorEnabled(true);
    velocityKeyboard.service(1000);
    serviceThrough(velocityKeyboard, 2000, 300000);
    bool sawCVelocity = false;
    bool sawEVelocity = false;
    bool sawGVelocity = false;
    for (const auto& entry : velocitySink.events) {
        if (entry.event.type != MusicalEventType::NoteOn ||
            entry.event.source != MusicalEventSource::Arpeggiator) continue;
        if (entry.event.note == 60) {
            assert(entry.event.velocity == 70);
            sawCVelocity = true;
        }
        if (entry.event.note == 64) {
            assert(entry.event.velocity == 90);
            sawEVelocity = true;
        }
        if (entry.event.note == 67) {
            assert(entry.event.velocity == 110);
            sawGVelocity = true;
        }
    }
    assert(sawCVelocity && sawEVelocity && sawGVelocity);
    const auto randomTraceA = runContour(5, 1000);
    const auto randomTraceB = runContour(5, 5000);
    const auto randomTraceWithNoContextClear = runContour(5, 1000, true);
    assert(randomTraceA.size() >= 6 && randomTraceA == randomTraceB &&
           randomTraceA == randomTraceWithNoContextClear);

    auto runTransportRandom = [](uint32_t pollEveryBlocks) {
        auto& timeline = GroovePuterMidi::projectTransportTimeline();
        timeline.resetPublisher();
        constexpr uint16_t frames = 128;
        constexpr uint32_t sampleRate = 22050;
        constexpr uint32_t blockMicros = (1000000u * frames) / sampleRate;
        const double stepsPerBlock = 8.0 * frames / sampleRate;
        timeline.publishBlock(0, frames, 3.0f, 120.0f,
                              static_cast<float>(sampleRate), false);
        timeline.publishBlock(1, frames, 3.0f, 120.0f,
                              static_cast<float>(sampleRate), true);
        MusicalEventRouter transportRouter;
        TraceSink transportSink;
        assert(transportRouter.addSink(transportSink));
        PerformanceKeyboard transport(transportRouter);
        assert(transport.externalNoteOn(60, 70));
        assert(transport.externalNoteOn(64, 90));
        assert(transport.externalNoteOn(67, 110));
        transport.setArpRate(PerformanceRate::Sixteenth);
        for (uint8_t i = 0; i < 5; ++i) transport.cycleArpDirection();
        transport.setMutationSeed(0x12345678u);
        transport.setArpeggiatorEnabled(true);
        transport.setTransportPlaying(true);
        for (uint32_t block = 1; block <= 500; ++block) {
            double phase = 3.0 + static_cast<double>(block) * stepsPerBlock;
            phase = std::fmod(phase, 16.0);
            timeline.publishBlock(block + 1u, frames, static_cast<float>(phase),
                                  120.0f, static_cast<float>(sampleRate), true);
            if (block % pollEveryBlocks == 0) {
                const uint32_t now = 2000000u + block * blockMicros;
                serviceAt(transport, transportSink, now);
            }
        }
        const auto trace = noteOns(transportSink);
        transport.setTransportPlaying(false);
        return trace;
    };
    const auto transportTraceFrequent = runTransportRandom(1);
    const auto transportTraceSparse = runTransportRandom(2);
    assert(transportTraceFrequent.size() >= 8 &&
           transportTraceFrequent == transportTraceSparse);

    // A late service call follows the existing bounded catch-up policy. It
    // cannot dump a burst of stale NoteOns, and the already emitted note still
    // receives scoped generated-output cleanup when its source changes.
    MusicalEventRouter lateRouter;
    TraceSink lateSink;
    assert(lateRouter.addSink(lateSink));
    PerformanceKeyboard late(lateRouter);
    assert(late.externalNoteOn(60, 100));
    late.setArpeggiatorEnabled(true);
    serviceAt(late, lateSink, 1000);
    const std::size_t initialArpAttacks = noteOns(lateSink).size();
    serviceAt(late, lateSink, 10000000);
    assert(noteOns(lateSink).size() == initialArpAttacks);
    assert(late.staleGeneratedNoteOnDrops() > 0);
    late.setArpPitchSource(ArpPitchSource::Memory);
    std::size_t emittedArpOffs = 0;
    for (const auto& entry : lateSink.events)
        emittedArpOffs += entry.event.type == MusicalEventType::NoteOff &&
            entry.event.source == MusicalEventSource::Arpeggiator;
    assert(emittedArpOffs >= initialArpAttacks);

    return 0;
}
