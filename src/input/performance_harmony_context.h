#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_HARMONY_CONTEXT_H
#define GROOVEPUTER_PERFORMANCE_HARMONY_CONTEXT_H

#include "performance_smart_play.h"
#include "../state/generated_synth_a_origin.h"
#include "../state/generated_phrase_recipe.h"
#include "../pattern/pattern_address.h"
#include "../generation/tonal/scale_catalog.h"

namespace GroovePuterPerformance {

// Build a read-only snapshot from the exact P1R phrase owner that is sounding
// in Song. Stale recipes, replaced Pattern slots and unrelated Song rows fail
// closed so Smart Play never borrows harmony from a different piece of material.
inline bool performanceContextAtGeneratedPhrase(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    int activePage,
    int playbackSongSlot,
    int songRow,
    int activeSynthAPatternGlobal,
    uint32_t activeSynthAMaterialId,
    uint8_t stepInBar,
    PerformanceContext& out,
    bool allowChromatic = true) {
    out = PerformanceContext{};
    if (!origin.valid() || !recipe.valid || recipe.bars == 0 ||
        recipe.bars != origin.common.barCount || recipe.pageIndex != activePage ||
        recipe.songSlot != playbackSongSlot || recipe.songStart < 0 ||
        songRow < recipe.songStart ||
        songRow >= recipe.songStart + recipe.bars || stepInBar >= 16u ||
        origin.common.rootPitchClass > 11u ||
        !GroovePuterRhythm::isCatalogScaleTypeValue(origin.common.scaleTypeValue)) {
        return false;
    }

    const uint8_t barOrdinal = static_cast<uint8_t>(songRow - recipe.songStart);
    const GroovePuterMaterial::GeneratedSynthABarOrigin& bar = origin.bars[barOrdinal];
    if (bar.phraseBarOrdinal != barOrdinal || bar.material.address.voice != 0u ||
        bar.material.address.globalSlot != activeSynthAPatternGlobal ||
        !GroovePuterMaterial::materialReferenceMatches(
            bar.material, GroovePuterMaterial::MaterialAddress{
                              0, static_cast<uint8_t>(activeSynthAPatternGlobal)},
            GroovePuterMaterial::MaterialId{activeSynthAMaterialId}) ||
        activeSynthAMaterialId == 0u ||
        songPatternPage(activeSynthAPatternGlobal) != activePage) {
        return false;
    }

    const GroovePuterRhythm::HarmonicRhythmPlan& rhythm = bar.harmonicRhythm;
    if (rhythm.phraseBarOrdinal != barOrdinal || rhythm.eventCount == 0u ||
        rhythm.eventCount > GroovePuterRhythm::kMaxHarmonicEvents ||
        GroovePuterRhythm::phraseHarmonicPositionCount(rhythm.onsets) !=
            rhythm.eventCount) {
        return false;
    }

    uint8_t eventInBar = 0;
    uint8_t eventOnset = 0;
    uint8_t nextEventOnset = GroovePuterRhythm::kStepsPerBar;
    bool found = false;
    uint8_t ordinal = 0;
    for (uint8_t step = 0; step < GroovePuterRhythm::kStepsPerBar; ++step) {
        if ((rhythm.onsets & GroovePuterRhythm::stepBit(step)) == 0u) continue;
        if (step <= stepInBar) {
            eventInBar = ordinal;
            eventOnset = step;
            found = true;
        } else {
            nextEventOnset = step;
            break;
        }
        ++ordinal;
    }
    if (!found) return false;

    const uint16_t globalOrdinal = static_cast<uint16_t>(
        rhythm.phraseHarmonicPosition) + eventInBar;
    if (globalOrdinal >= GroovePuterRhythm::kMaxPhraseHarmonicEventPositions) return false;
    const GroovePuterRhythm::ChordProgressionEventResult selected =
        GroovePuterRhythm::chordProgressionEventAt(
            origin.common.progressionSource, globalOrdinal);
    if (selected.status != GroovePuterRhythm::ChordProgressionStatus::Ok &&
        selected.status != GroovePuterRhythm::ChordProgressionStatus::ValidButStatic) {
        return false;
    }

    const GroovePuterRhythm::ScaleDefinitionView scale =
        GroovePuterRhythm::scaleDefinitionFor(origin.common.scaleTypeValue);
    if (scale.intervals == nullptr || selected.event.degree >= scale.count) return false;
    uint8_t chordIntervals[5]{};
    uint8_t chordCount = 0;
    if (!GroovePuterRhythm::chordQualityIntervals(
            selected.event.quality, chordIntervals, 5, chordCount)) return false;

    const int rootMidi = 36 + origin.common.rootPitchClass +
        GroovePuterRhythm::scaleDegreeToSemitone(
            origin.common.scaleTypeValue, selected.event.degree) +
        selected.event.rootOffsetSemitones;
    if (rootMidi < 0 || rootMidi > 127) return false;

    uint16_t scalePitchClasses = 0;
    for (uint8_t degree = 0; degree < scale.count; ++degree) {
        const uint8_t pitchClass = static_cast<uint8_t>(
            (origin.common.rootPitchClass + scale.intervals[degree]) % 12);
        scalePitchClasses = static_cast<uint16_t>(
            scalePitchClasses | static_cast<uint16_t>(1u << pitchClass));
    }

    out.harmony = selected.event;
    out.scalePitchClasses = scalePitchClasses;
    out.rootMidi = static_cast<uint8_t>(rootMidi);
    out.minNote = 12;
    out.maxNote = 95;
    out.sourceOrdinal = static_cast<uint8_t>(globalOrdinal);
    out.eventOnset = eventOnset;
    out.eventDuration = static_cast<uint8_t>(nextEventOnset - eventOnset);
    out.allowChromatic = allowChromatic;
    out.hasHarmony = true;
    return true;
}

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_HARMONY_CONTEXT_H
