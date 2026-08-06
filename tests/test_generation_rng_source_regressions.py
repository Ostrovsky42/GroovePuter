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

    print("Generation RNG source regressions passed")


if __name__ == "__main__":
    main()
