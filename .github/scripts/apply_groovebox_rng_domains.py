#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return updated


def patch_mode_manager_header() -> None:
    path = "src/dsp/mode_manager.h"
    text = read(path)
    text = replace_once(
        text,
        '#include "src/dsp/genre_manager.h"\n',
        '#include "src/dsp/genre_manager.h"\n#include "src/dsp/deterministic_rng.h"\n',
        "mode_manager include",
    )
    text = replace_once(
        text,
        "GrooveboxModeManager(MiniAcid& engine) : engine_(engine), currentMode_(GrooveboxMode::Minimal), currentFlavor_(0) {}",
        "GrooveboxModeManager(MiniAcid& engine) : engine_(engine), currentMode_(GrooveboxMode::Minimal), currentFlavor_(0), generationSeed_(0) {}",
        "mode_manager constructor",
    )
    text = replace_once(
        text,
        "    int flavorCount() const { return 5; }\n",
        "    int flavorCount() const { return 5; }\n"
        "    void setGenerationSeed(uint32_t seed) {\n"
        "        generationSeed_ = seed == 0 ? kFallbackGenerationSeed : seed;\n"
        "    }\n",
        "generation seed setter",
    )
    text = replace_once(
        text,
        "private:\n    MiniAcid& engine_;\n    GrooveboxMode currentMode_;\n    int currentFlavor_;\n};\n",
        "private:\n"
        "    enum class GenerationDomain : uint32_t {\n"
        "        SynthA = 0x13579BDFu,\n"
        "        SynthB = 0x2468ACE1u,\n"
        "        Drums = 0xD12F00D5u,\n"
        "        DrumVoiceBase = 0xD0000000u,\n"
        "    };\n"
        "\n"
        "    static constexpr uint32_t kFallbackGenerationSeed = 0x6D2B79F5u;\n"
        "    uint32_t ensureGenerationSeed() const;\n"
        "    DeterministicRng makeGenerationRng(GenerationDomain domain,\n"
        "                                       uint32_t contentHash) const;\n"
        "\n"
        "    MiniAcid& engine_;\n"
        "    GrooveboxMode currentMode_;\n"
        "    int currentFlavor_;\n"
        "    mutable uint32_t generationSeed_;\n"
        "};\n",
        "mode_manager private fields",
    )
    write(path, text)


def patch_mode_manager_cpp() -> None:
    path = "src/dsp/mode_manager.cpp"
    text = read(path)
    original_rand_calls = text.count("rand()")
    if original_rand_calls < 20:
        raise RuntimeError(f"mode_manager rand baseline unexpectedly low: {original_rand_calls}")

    # Existing generation calls all live in functions that receive a local RNG below.
    text = text.replace("rand()", "generationRandom(rng)")

    anchor = "\n\nvoid GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm) const {\n"
    helper = r'''

namespace {

uint32_t mixGenerationSeed(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

uint32_t hashGenerationByte(uint32_t hash, uint8_t value) {
    return (hash ^ value) * 16777619u;
}

uint32_t hashSynthPattern(const SynthPattern& pattern) {
    uint32_t hash = 2166136261u;
    for (const SynthStep& step : pattern.steps) {
        hash = hashGenerationByte(hash, static_cast<uint8_t>(step.note));
        hash = hashGenerationByte(hash, step.slide ? 1u : 0u);
        hash = hashGenerationByte(hash, step.accent ? 1u : 0u);
        hash = hashGenerationByte(hash, step.ghost ? 1u : 0u);
        hash = hashGenerationByte(hash, step.velocity);
        hash = hashGenerationByte(hash, static_cast<uint8_t>(step.timing));
        hash = hashGenerationByte(hash, step.fx);
        hash = hashGenerationByte(hash, step.fxParam);
        hash = hashGenerationByte(hash, step.probability);
    }
    return hash;
}

uint32_t hashDrumPattern(const DrumPattern& pattern) {
    uint32_t hash = 2166136261u;
    for (const DrumStep& step : pattern.steps) {
        hash = hashGenerationByte(hash, step.hit ? 1u : 0u);
        hash = hashGenerationByte(hash, step.accent ? 1u : 0u);
        hash = hashGenerationByte(hash, step.velocity);
        hash = hashGenerationByte(hash, static_cast<uint8_t>(step.timing));
        hash = hashGenerationByte(hash, step.fx);
        hash = hashGenerationByte(hash, step.fxParam);
        hash = hashGenerationByte(hash, step.probability);
    }
    return hash;
}

uint32_t hashDrumPatternSet(const DrumPatternSet& patternSet) {
    uint32_t hash = 2166136261u;
    for (const DrumPattern& voice : patternSet.voices) {
        hash ^= mixGenerationSeed(hashDrumPattern(voice));
        hash *= 16777619u;
    }
    return hash;
}

int generationRandom(DeterministicRng& rng) {
    return static_cast<int>(rng.next() & 0x7FFFFFFFu);
}

}  // namespace

uint32_t GrooveboxModeManager::ensureGenerationSeed() const {
    if (generationSeed_ == 0) {
        // Capture boot-seeded libc entropy once. Pattern generation never
        // consumes the global stream again after this boundary.
        uint32_t seed = static_cast<uint32_t>(::rand()) ^ 0xA511E9B3u;
        generationSeed_ = seed == 0 ? kFallbackGenerationSeed : seed;
    }
    return generationSeed_;
}

DeterministicRng GrooveboxModeManager::makeGenerationRng(
        GenerationDomain domain, uint32_t contentHash) const {
    uint32_t seed = ensureGenerationSeed();
    seed ^= static_cast<uint32_t>(domain);
    seed ^= mixGenerationSeed(contentHash);
    seed ^= static_cast<uint32_t>(currentMode_) * 0x9E3779B9u;
    seed ^= static_cast<uint32_t>(currentFlavor_ + 1) * 0x85EBCA6Bu;
    return DeterministicRng(mixGenerationSeed(seed));
}
'''
    if anchor not in text:
        raise RuntimeError("legacy synth generator anchor not found")
    text = text.replace(anchor, helper + anchor, 1)

    text = replace_once(
        text,
        "void GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm) const {\n    const ModeConfig& cfg = config();",
        "void GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm) const {\n"
        "    DeterministicRng rng = makeGenerationRng(\n"
        "        GenerationDomain::SynthA, hashSynthPattern(pattern));\n"
        "    const ModeConfig& cfg = config();",
        "legacy synth local rng",
    )

    text = regex_once(
        text,
        r"(void GrooveboxModeManager::generatePattern\(SynthPattern& pattern, float bpm,\s*\n\s*const GenerativeParams& params,\s*\n\s*const GenreBehavior& behavior,\s*\n\s*int voiceIndex\) const \{)",
        r"\1\n    const GenerationDomain domain = voiceIndex == 0\n        ? GenerationDomain::SynthA\n        : GenerationDomain::SynthB;\n    DeterministicRng rng = makeGenerationRng(domain, hashSynthPattern(pattern));",
        "genre synth local rng",
    )

    text = regex_once(
        text,
        r"(void GrooveboxModeManager::generateDrumPattern\(DrumPatternSet& patternSet,\s*\n\s*const GenerativeParams& params,\s*\n\s*const GenreBehavior& behavior\) const \{\s*\n\s*// Structural behavior[^\n]*\n\s*// DrumGenreTemplate table; keep the parameter for API compatibility\.\s*\n\s*\(void\)behavior;)",
        r"\1\n    DeterministicRng rng = makeGenerationRng(\n        GenerationDomain::Drums, hashDrumPatternSet(patternSet));",
        "drum set local rng",
    )

    text = replace_once(
        text,
        "        engine_.genreManager().generativeMode(),\n        engine_.genreManager().drumTemplateOverride());",
        "        engine_.genreManager().generativeMode(),\n        rng,\n        engine_.genreManager().drumTemplateOverride());",
        "drum generator rng argument",
    )

    text = regex_once(
        text,
        r"(void GrooveboxModeManager::generateDrumVoice\(DrumPattern& pattern, int voiceIndex,\s*\n\s*const GenerativeParams& params,\s*\n\s*const GenreBehavior& behavior\) const \{)",
        r"\1\n    const auto domain = static_cast<GenerationDomain>(\n        static_cast<uint32_t>(GenerationDomain::DrumVoiceBase) +\n        static_cast<uint32_t>(voiceIndex & 0xFF));\n    DeterministicRng rng = makeGenerationRng(domain, hashDrumPattern(pattern));",
        "drum voice local rng",
    )

    if "rand()" in text.replace("::rand()", ""):
        raise RuntimeError("unmigrated mode_manager rand() call remains")
    if text.count("DeterministicRng rng = makeGenerationRng") != 4:
        raise RuntimeError("expected exactly four generation RNG boundaries")
    write(path, text)


def patch_advanced_generator_header() -> None:
    path = "src/dsp/advanced_pattern_generator.h"
    text = read(path)
    text = replace_once(
        text,
        '#include "../../scenes.h" // For pattern structs\n',
        '#include "../../scenes.h" // For pattern structs\n#include "src/dsp/deterministic_rng.h"\n',
        "advanced generator rng include",
    )
    text = replace_once(
        text,
        "                                    GenerativeMode mode,\n                                    const DrumGenreTemplate* templateOverride = nullptr);",
        "                                    GenerativeMode mode,\n                                    DeterministicRng& rng,\n                                    const DrumGenreTemplate* templateOverride = nullptr);",
        "advanced generator signature",
    )
    write(path, text)


def patch_advanced_generator_cpp() -> None:
    path = "src/dsp/advanced_pattern_generator.cpp"
    text = read(path)
    marker = "// DrumPatternGenerator Implementation"
    marker_pos = text.index(marker)
    prefix = text[:marker_pos]
    suffix = text[marker_pos:]
    if suffix.count("rand()") < 10:
        raise RuntimeError("drum generator rand baseline unexpectedly low")
    suffix = suffix.replace("rand()", "generationRandom(rng)")
    text = prefix + suffix

    text = replace_once(
        text,
        "namespace {\n\nstatic inline bool stepInMask",
        "namespace {\n\n"
        "static inline int generationRandom(DeterministicRng& rng) {\n"
        "    return static_cast<int>(rng.next() & 0x7FFFFFFFu);\n"
        "}\n\n"
        "static inline bool stepInMask",
        "advanced generationRandom helper",
    )
    text = replace_once(
        text,
        "static inline int8_t randomTimingOffset(float microTimingAmount) {",
        "static inline int8_t randomTimingOffset(float microTimingAmount, DeterministicRng& rng) {",
        "drum timing rng parameter",
    )
    text = replace_once(
        text,
        "                                               GenerativeMode mode,\n                                               const DrumGenreTemplate* templateOverride) {",
        "                                               GenerativeMode mode,\n                                               DeterministicRng& rng,\n                                               const DrumGenreTemplate* templateOverride) {",
        "drum implementation signature",
    )
    text = replace_once(
        text,
        "        st.timing = randomTimingOffset(params.microTimingAmount);",
        "        st.timing = randomTimingOffset(params.microTimingAmount, rng);",
        "drum timing rng call",
    )
    drum_section = text[text.index(marker):]
    if "rand()" in drum_section:
        raise RuntimeError("unmigrated drum generator rand() call remains")
    write(path, text)


def patch_pattern_rng_boundary_comment() -> None:
    path = "tests/test_pattern_generator_rng.cpp"
    text = read(path)
    text = text.replace(
        "    // PR1 boundary: GrooveboxModeManager still consumes the global libc RNG.\n"
        "    // These three blocks model the Generate A -> B -> Drums call order that\n"
        "    // PR2 will isolate. This test intentionally compares state continuity,\n"
        "    // not platform-specific rand() golden values.\n",
        "    // Historical boundary smoke test: constructing Song Page must not reset\n"
        "    // libc RNG. GrooveboxModeManager now consumes only one lazy boot seed and\n"
        "    // then runs its own deterministic generation domains.\n",
    )
    text = text.replace(
        '"opening Song Page reset the boot-seeded global RNG sequence consumed by GrooveboxModeManager"',
        '"opening Song Page reset the boot-seeded global RNG sequence"',
    )
    write(path, text)


def add_source_regression() -> None:
    path = ROOT / "tests/test_generation_rng_source_regressions.py"
    path.write_text(
        r'''#!/usr/bin/env python3
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
''',
        encoding="utf-8",
    )


def wire_host_test() -> None:
    path = "tests/run_host_tests.sh"
    text = read(path)
    anchor = 'python3 "${ROOT_DIR}/tests/test_source_regressions.py"\n'
    addition = anchor + 'python3 "${ROOT_DIR}/tests/test_generation_rng_source_regressions.py"\n'
    text = replace_once(text, anchor, addition, "host test wiring")
    write(path, text)


def main() -> None:
    patch_mode_manager_header()
    patch_mode_manager_cpp()
    patch_advanced_generator_header()
    patch_advanced_generator_cpp()
    patch_pattern_rng_boundary_comment()
    add_source_regression()
    wire_host_test()


if __name__ == "__main__":
    main()
