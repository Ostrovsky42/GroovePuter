#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "src/output/output_ownership.h"

using GroovePuterOutput::Mode;
using GroovePuterOutput::SourceClass;
using GroovePuterOutput::Track;

namespace {

void expectModeNames() {
    assert(std::strcmp(GroovePuterOutput::modeName(Mode::Internal), "INTERNAL") == 0);
    assert(std::strcmp(GroovePuterOutput::modeName(Mode::Midi), "MIDI") == 0);
    assert(std::strcmp(GroovePuterOutput::modeName(Mode::Layer), "LAYER") == 0);
    assert(std::strcmp(GroovePuterOutput::modeShortName(Mode::Internal), "INT") == 0);
    assert(std::strcmp(GroovePuterOutput::modeShortName(Mode::Midi), "MIDI") == 0);
    assert(std::strcmp(GroovePuterOutput::modeShortName(Mode::Layer), "LAYER") == 0);
}

void expectLegacyCompatibility() {
    for (Track track : {Track::SynthA, Track::SynthB, Track::Drums}) {
        GroovePuterOutput::restoreLegacyCompatibility(track);
        assert(!GroovePuterOutput::hasExplicitMode(track));

        assert(GroovePuterOutput::allowsInternal(track, SourceClass::Pattern));
        assert(GroovePuterOutput::allowsMidi(track, SourceClass::Pattern));

        assert(!GroovePuterOutput::allowsInternal(track, SourceClass::Performance));
        assert(GroovePuterOutput::allowsMidi(track, SourceClass::Performance));

        assert(GroovePuterOutput::allowsInternal(track, SourceClass::Preview));
        assert(!GroovePuterOutput::allowsMidi(track, SourceClass::Preview));
    }
}

void expectExplicitTruthTable(Track track) {
    uint8_t disableEpoch = GroovePuterOutput::midiDisableEpoch(track);

    assert(GroovePuterOutput::setMode(track, Mode::Internal));
    assert(GroovePuterOutput::hasExplicitMode(track));
    assert(GroovePuterOutput::mode(track) == Mode::Internal);
    assert(GroovePuterOutput::allowsInternal(track, SourceClass::Pattern));
    assert(GroovePuterOutput::allowsInternal(track, SourceClass::Performance));
    assert(!GroovePuterOutput::allowsMidi(track, SourceClass::Pattern));
    assert(!GroovePuterOutput::allowsMidi(track, SourceClass::Performance));
    assert(static_cast<uint8_t>(disableEpoch + 1u) ==
           GroovePuterOutput::midiDisableEpoch(track));

    disableEpoch = GroovePuterOutput::midiDisableEpoch(track);
    assert(GroovePuterOutput::setMode(track, Mode::Midi));
    assert(!GroovePuterOutput::allowsInternal(track, SourceClass::Pattern));
    assert(!GroovePuterOutput::allowsInternal(track, SourceClass::Performance));
    assert(GroovePuterOutput::allowsMidi(track, SourceClass::Pattern));
    assert(GroovePuterOutput::allowsMidi(track, SourceClass::Performance));
    assert(disableEpoch == GroovePuterOutput::midiDisableEpoch(track));

    assert(GroovePuterOutput::setMode(track, Mode::Layer));
    assert(GroovePuterOutput::allowsInternal(track, SourceClass::Pattern));
    assert(GroovePuterOutput::allowsInternal(track, SourceClass::Performance));
    assert(GroovePuterOutput::allowsMidi(track, SourceClass::Pattern));
    assert(GroovePuterOutput::allowsMidi(track, SourceClass::Performance));

    disableEpoch = GroovePuterOutput::midiDisableEpoch(track);
    assert(GroovePuterOutput::setMode(track, Mode::Internal));
    assert(static_cast<uint8_t>(disableEpoch + 1u) ==
           GroovePuterOutput::midiDisableEpoch(track));

    // Re-applying the same mode is a no-op and cannot create a cleanup epoch.
    disableEpoch = GroovePuterOutput::midiDisableEpoch(track);
    assert(!GroovePuterOutput::setMode(track, Mode::Internal));
    assert(disableEpoch == GroovePuterOutput::midiDisableEpoch(track));
}

void expectTargetMapping() {
    Track track = Track::Drums;
    assert(GroovePuterOutput::trackForTarget(MusicalEventTarget::SynthA, track));
    assert(track == Track::SynthA);
    assert(GroovePuterOutput::trackForTarget(MusicalEventTarget::SynthB, track));
    assert(track == Track::SynthB);
    assert(GroovePuterOutput::trackForTarget(MusicalEventTarget::Drums, track));
    assert(track == Track::Drums);
    assert(!GroovePuterOutput::trackForTarget(MusicalEventTarget::Dx, track));

    // DX is intentionally outside 0.9.6 groove-track ownership and keeps its
    // accepted external route.
    assert(GroovePuterOutput::allowsMidi(
        MusicalEventTarget::Dx, SourceClass::Performance));
    assert(!GroovePuterOutput::allowsInternal(
        MusicalEventTarget::Dx, SourceClass::Performance));
}

void expectCycleOrder() {
    assert(GroovePuterOutput::cycleMode(Mode::Internal) == Mode::Midi);
    assert(GroovePuterOutput::cycleMode(Mode::Midi) == Mode::Layer);
    assert(GroovePuterOutput::cycleMode(Mode::Layer) == Mode::Internal);
}

}  // namespace

int main() {
    expectModeNames();
    expectLegacyCompatibility();
    expectExplicitTruthTable(Track::SynthA);
    expectExplicitTruthTable(Track::SynthB);
    expectExplicitTruthTable(Track::Drums);
    expectTargetMapping();
    expectCycleOrder();

    std::cout << "Output ownership contract tests: PASS\n";
    return 0;
}
