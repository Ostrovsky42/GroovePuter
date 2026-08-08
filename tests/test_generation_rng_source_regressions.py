#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    header = (ROOT / "src/dsp/mode_manager.h").read_text(encoding="utf-8")
    manager = (ROOT / "src/dsp/mode_manager.cpp").read_text(encoding="utf-8")
    advanced_h = (ROOT / "src/dsp/advanced_pattern_generator.h").read_text(encoding="utf-8")
    advanced_cpp = (ROOT / "src/dsp/advanced_pattern_generator.cpp").read_text(encoding="utf-8")
    song_page = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    materializer = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")
    bar_header = (ROOT / "src/dsp/bar_material_commit.h").read_text(encoding="utf-8")
    bar_commit = (ROOT / "src/dsp/bar_material_commit.cpp").read_text(encoding="utf-8")
    genre_header = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    synth_page = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
    drum_page = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")

    require("mutable uint32_t generationSeed_;" in header,
            "GrooveboxModeManager must own exactly one 32-bit generation seed")
    require("void setGenerationSeed(uint32_t seed)" in header,
            "tests and explicit lifecycle wiring need a deterministic seed boundary")
    require("uint32_t generationSeed() const" in header,
            "Song materialization needs the existing generation seed owner")
    require(manager.count("::rand()") == 1,
            "global libc RNG may only be sampled once at lazy boot-seed capture")
    require(re.search(r"(?<!:)\brand\(\)", manager) is None,
            "mode_manager.cpp still contains an unscoped global rand() call")
    require(manager.count("DeterministicRng rng = makeGenerationRng") == 4,
            "Synth A, Synth B, full drums and drum-voice generation need local RNG boundaries")
    require("generationRandom" not in manager and
            "generationRandom" not in advanced_cpp,
            "migrated generation must not reintroduce masked next() modulo sampling")
    require("boundedRandom(" in manager and
            "boundedRandom(" in advanced_cpp,
            "all migrated variable-range draws must use DeterministicRng::bounded()")
    require("GenerationDomain::SynthA" in manager and
            "GenerationDomain::SynthB" in manager and
            "GenerationDomain::Drums" in manager,
            "generation domains must remain explicit and reviewable")
    require("hashSynthPattern(pattern)" in manager and
            "hashDrumPatternSet(patternSet)" in manager,
            "domain seed must evolve from only that domain's current material")
    require("DeterministicRng& rng" in advanced_h,
            "DrumPatternGenerator must accept an injected RNG")
    drum_marker = advanced_cpp.index("// DrumPatternGenerator Implementation")
    require("rand()" not in advanced_cpp[drum_marker:],
            "drum generation still consumes libc RNG")
    require("randomTimingOffset(params.microTimingAmount, rng)" in advanced_cpp,
            "drum microtiming must use the injected RNG")

    song_generation = song_page[song_page.index(
        "SongPatternMaterializer::Result SongPage::materializeSongTracks") :]
    song_generation = song_generation[:song_generation.index(
        "bool SongPage::generateEntireRow()") + 2000]
    require(re.search(r"\b(?:s?rand)\s*\(", song_generation) is None,
            "Song generation path still consumes libc RNG")
    require("SongPatternMaterializer::generate" in song_generation,
            "Song Page must use the transactional materialization helper")
    require("GrooveboxModeManager generator(mini_acid_)" in song_generation,
            "Song materialization must use the production groovebox generator")
    require("setGenerationSeed(seed)" in song_generation,
            "Song materialization must inject the action seed")
    require("DeterministicRng rng(seed)" in materializer,
            "Song action seed derivation must use DeterministicRng")
    require("markSceneMutated()" in materializer,
            "successful materialization must own one dirty revision mutation")

    # Bar-bound musical-material commit contract. Generation remains on the
    # control side; only bounded activation is reachable from audio BAR_START.
    require("MaterialAction::Variation" in bar_header and
            "MaterialAction::Phrase" in bar_header and
            "MaterialAction::SongMaterialization" in bar_header and
            "MaterialAction::RhythmArchetype" in bar_header,
            "pending transaction vocabulary must cover future material operations")
    bar_code = "\n".join(
        line.split("//", 1)[0] for line in bar_commit.splitlines()
    )
    require("std::vector" not in bar_code and
            re.search(r"\bnew\s+[A-Za-z_:]", bar_code) is None and
            "malloc(" not in bar_code,
            "BAR_START pending state must remain fixed-size and allocation-free")
    require("AtlasRuntime::applyRecipe" in bar_commit and
            "getCompiledGenerativeParams" in bar_commit,
            "local generation must be Atlas-first with procedural genre fallback")
    require("if (!engine.isPlaying())" in bar_commit and
            "MaterialQueueResult::CommittedNow" in bar_commit,
            "stopped transport must commit generated material immediately")
    require("g_pendingValid.store(true" in bar_commit and
            "PendingNextBar" in bar_commit,
            "playing transport must publish only a complete pending candidate")
    require("currentPageIndex() != g_pending.page" in bar_commit and
            "CancelledPageMismatch" in bar_commit,
            "pending material must not spill into another pattern page")
    require("atlasVariationForLane" in bar_commit and
            "return g_pending.atlasVariation" in bar_commit,
            "compatible Atlas lanes must share one pending variation identity")

    bar_block_start = engine.index("if (barTick == 0)")
    bar_block_end = engine.index("} else if (barTick % 24 == 0)", bar_block_start)
    bar_block = engine[bar_block_start:bar_block_end]
    require("commitPendingRecipe()" in bar_block,
            "pending material activation must remain inside the real audio BAR_START")
    require("commitPendingMaterialAtBarStart(scenes_)" in genre_header,
            "the retained BAR_START adapter must dispatch the material commit")
    require("return false;" in genre_header[genre_header.index(
                "bool commitPendingRecipe()") : genre_header.index(
                "void applyGenreTimbre")],
            "bar material commit must not trigger legacy genre regeneration")

    synth_g = synth_page.index("lowerKey == 'g'")
    synth_tail = synth_page[synth_g:synth_g + 1200]
    require("queueSynthGenerationForBar" in synth_tail and
            "GEN -> NEXT BAR" in synth_tail,
            "Synth G must stage through the bar-bound path and expose pending state")

    drum_g = drum_page.index("const bool keyG")
    drum_tail = drum_page[drum_g:drum_g + 2200]
    require("queueDrumGenerationForBar" in drum_tail and
            "queueDrumVoiceGenerationForBar" in drum_tail and
            "queueDrumChaosForBar" in drum_tail,
            "Drum G/Ctrl+G/Alt+G must share the bar-bound material path")
    require("-> NEXT BAR" in drum_tail,
            "Drum pending generation must be visible to the player")

    print("Generation RNG and bar material commit source regressions passed")


if __name__ == "__main__":
    main()
