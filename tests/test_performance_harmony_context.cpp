#include <cassert>
#include <cstdint>

#include "src/input/performance_harmony_context.h"

int main() {
    using namespace GroovePuterPerformance;
    GroovePuterMaterial::GeneratedSynthAOrigin origin{};
    origin.common.barCount = 2;
    origin.common.rootPitchClass = 0;
    origin.common.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
    GroovePuterRhythm::ChordProgressionSourceRequest progressionRequest{};
    progressionRequest.requestedId = GroovePuterRhythm::ProgressionId::PopCycle;
    progressionRequest.phraseBars = 2;
    const auto progression =
        GroovePuterRhythm::realizeChordProgressionSource(progressionRequest);
    assert(progression.status == GroovePuterRhythm::ChordProgressionStatus::Ok);
    assert(progression.source.period >= 3);
    origin.common.progressionSource = progression.source;
    for (uint8_t bar = 0; bar < 2; ++bar) {
        origin.bars[bar].phraseBarOrdinal = bar;
        origin.bars[bar].material.address = {0, static_cast<uint8_t>(4 + bar)};
        origin.bars[bar].material.id = {static_cast<uint32_t>(100 + bar)};
        origin.bars[bar].originPatternVersion.low = 1;
        origin.bars[bar].harmonicRhythm.phraseBarOrdinal = bar;
        origin.bars[bar].harmonicRhythm.phraseHarmonicPosition =
            static_cast<uint8_t>(bar * 2);
        origin.bars[bar].harmonicRhythm.eventCount = 2;
        origin.bars[bar].harmonicRhythm.onsets =
            GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8);
    }

    GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
    recipe.pageIndex = 0;
    recipe.songSlot = 0;
    recipe.songStart = 10;
    recipe.bars = 2;
    recipe.valid = true;

    PerformanceContext context{};
    const bool found = performanceContextAtGeneratedPhrase(
        origin, recipe, 0, 0, 11, 5, 101, 9, context);
    assert(found && context.hasHarmony);
    const auto expectedSecondEvent = GroovePuterRhythm::chordProgressionEventAt(
        progression.source, 3);
    assert(expectedSecondEvent.status == GroovePuterRhythm::ChordProgressionStatus::Ok);
    assert(context.harmony.degree == expectedSecondEvent.event.degree);
    assert(context.harmony.quality == expectedSecondEvent.event.quality);
    assert(context.rootMidi == static_cast<uint8_t>(36 +
        GroovePuterRhythm::scaleDegreeToSemitone(
            GroovePuterRhythm::kScaleMajor, expectedSecondEvent.event.degree) +
        expectedSecondEvent.event.rootOffsetSemitones));
    assert(context.sourceOrdinal == 3);
    assert(context.eventOnset == 8 && context.eventDuration == 8);
    assert((context.scalePitchClasses & (1u << 9u)) != 0);

    PerformanceContext rejected{};
    assert(!performanceContextAtGeneratedPhrase(
        origin, recipe, 0, 0, 11, 5, 999, 9, rejected)); // replaced slot id
    assert(!rejected.hasHarmony);
    assert(!performanceContextAtGeneratedPhrase(
        origin, recipe, 0, 1, 11, 5, 101, 9, rejected)); // wrong playback slot
    assert(!performanceContextAtGeneratedPhrase(
        origin, recipe, 0, 0, 11, 6, 101, 9, rejected)); // wrong Song row pattern

    const bool secondBar = performanceContextAtGeneratedPhrase(
        origin, recipe, 0, 0, 11, 5, 101, 2, context);
    assert(secondBar && context.sourceOrdinal == 2);
    const auto expectedThirdEvent = GroovePuterRhythm::chordProgressionEventAt(
        progression.source, 2);
    assert(expectedThirdEvent.status == GroovePuterRhythm::ChordProgressionStatus::Ok);
    assert(context.harmony.degree == expectedThirdEvent.event.degree);
    assert(context.harmony.quality == expectedThirdEvent.event.quality);
    assert(context.eventOnset == 0 && context.eventDuration == 8);

    GroovePuterRhythm::ChordProgressionSourceRequest staticRequest{};
    staticRequest.requestedId = GroovePuterRhythm::ProgressionId::StaticModal;
    staticRequest.phraseBars = 2;
    const auto staticProgression =
        GroovePuterRhythm::realizeChordProgressionSource(staticRequest);
    assert(staticProgression.status ==
           GroovePuterRhythm::ChordProgressionStatus::ValidButStatic);
    auto staticOrigin = origin;
    staticOrigin.common.progressionSource = staticProgression.source;
    assert(performanceContextAtGeneratedPhrase(
        staticOrigin, recipe, 0, 0, 10, 4, 100, 4, context));
    assert(context.hasHarmony && context.sourceOrdinal == 0);
    assert(performanceContextAtGeneratedPhrase(
        staticOrigin, recipe, 0, 0, 10, 4, 100, 4, context, false));
    assert(context.hasHarmony && !context.allowChromatic);
    return 0;
}
