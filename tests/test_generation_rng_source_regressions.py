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

    require("mutable uint32_t generationSeed_;" in header,
            "GrooveboxModeManager must own exactly one 32-bit generation seed")
    require("void setGenerationSeed(uint32_t seed)" in header,
            "tests and explicit lifecycle wiring need a deterministic seed boundary")
    require(manager.count("::rand()") == 1,
            "global libc RNG may only be sampled once at lazy boot-seed capture")
    require(re.search(r"(?<!:)\brand\(\)", manager) is None,
            "mode_manager.cpp still contains an unscoped global rand() call")
    require(manager.count("DeterministicRng rng = makeGenerationRng") == 4,
            "Synth A, Synth B, full drums and drum-voice generation need local RNG boundaries")
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

    print("Generation RNG source regressions passed")


if __name__ == "__main__":
    main()
