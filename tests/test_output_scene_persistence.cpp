#include <cassert>
#include <iostream>
#include <string>

#include "src/output/output_scene_persistence.h"

using GroovePuterOutput::Mode;
using GroovePuterOutput::PersistedOutputModes;
using GroovePuterOutput::Track;

namespace {

void legacyAll() {
    GroovePuterOutput::restoreLegacyCompatibility(Track::SynthA);
    GroovePuterOutput::restoreLegacyCompatibility(Track::SynthB);
    GroovePuterOutput::restoreLegacyCompatibility(Track::Drums);
}

void expectRoundTrip() {
    legacyAll();
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    assert(GroovePuterOutput::setMode(Track::SynthB, Mode::Midi));
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Layer));

    const std::string base =
        "{\"name\":\"scene\",\"nested\":{\"out\":[3,3,3]},"
        "\"text\":\"literal out token\"}";
    std::string persisted;
    assert(GroovePuterOutput::injectOutputModesIntoScene(base, persisted));
    assert(persisted.find("\"out\":[1,2,3]") != std::string::npos);

    PersistedOutputModes captured{};
    assert(GroovePuterOutput::captureOutputModesFromScene(persisted, captured));
    assert(captured.present);
    assert(captured.values[0] == 1);
    assert(captured.values[1] == 2);
    assert(captured.values[2] == 3);

    // Capture is transactional and cannot mutate runtime ownership by itself.
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Midi));
    assert(GroovePuterOutput::mode(Track::SynthA) == Mode::Midi);
    assert(captured.commit());
    assert(GroovePuterOutput::mode(Track::SynthA) == Mode::Internal);
    assert(GroovePuterOutput::mode(Track::SynthB) == Mode::Midi);
    assert(GroovePuterOutput::mode(Track::Drums) == Mode::Layer);
}

void expectMissingFieldRestoresLegacy() {
    legacyAll();
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    assert(GroovePuterOutput::setMode(Track::SynthB, Mode::Internal));
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Internal));

    PersistedOutputModes captured{};
    assert(GroovePuterOutput::captureOutputModesFromScene(
        "{\"name\":\"old\",\"nested\":{\"out\":[1,2,3]}}", captured));
    assert(!captured.present);
    assert(captured.commit());
    assert(!GroovePuterOutput::hasExplicitMode(Track::SynthA));
    assert(!GroovePuterOutput::hasExplicitMode(Track::SynthB));
    assert(!GroovePuterOutput::hasExplicitMode(Track::Drums));
}

void expectRawZeroCanPersistCompatibility() {
    legacyAll();
    assert(GroovePuterOutput::setMode(Track::SynthB, Mode::Midi));
    std::string persisted;
    assert(GroovePuterOutput::injectOutputModesIntoScene("{\"x\":1}", persisted));
    assert(persisted.find("\"out\":[0,2,0]") != std::string::npos);

    PersistedOutputModes captured{};
    assert(GroovePuterOutput::captureOutputModesFromScene(persisted, captured));
    assert(captured.present);
    assert(captured.values[0] == 0);
    assert(captured.values[1] == 2);
    assert(captured.values[2] == 0);
}

void expectMalformedFailsClosed() {
    for (const char* json : {
             "{\"x\":1,\"out\":[1,9,3]}",
             "{\"x\":1,\"out\":[1,2]}",
             "{\"x\":1,\"out\":[1,2,3,1]}",
             "{\"out\":null}",
             "{\"out\":[1,2,3],\"out\":[1,2,3]}",
         }) {
        PersistedOutputModes captured{};
        assert(!GroovePuterOutput::captureOutputModesFromScene(json, captured));
    }
}

void expectStringsDoNotSpoofRootKey() {
    const std::string json =
        "{\"text\":\"\\\"out\\\":[1,2,3]\","
        "\"object\":{\"out\":[3,2,1]}}";
    PersistedOutputModes captured{};
    assert(GroovePuterOutput::captureOutputModesFromScene(json, captured));
    assert(!captured.present);
}

}  // namespace

int main() {
    expectRoundTrip();
    expectMissingFieldRestoresLegacy();
    expectRawZeroCanPersistCompatibility();
    expectMalformedFailsClosed();
    expectStringsDoNotSpoofRootKey();
    std::cout << "Output Scene persistence tests: PASS\n";
    return 0;
}
