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


def replace_random_modulo_calls(text: str, label: str) -> str:
    before = text.count("generationRandom(rng)")
    if before == 0:
        raise RuntimeError(f"{label}: no generationRandom(rng) calls found")

    # Handle parenthesized arithmetic bounds first, then simple identifiers,
    # member expressions and integer literals. All migrated call sites have a
    # positive upper bound by construction.
    text, parenthesized = re.subn(
        r"generationRandom\(rng\)\s*%\s*\(([^()\n]+)\)",
        r"boundedRandom(rng, \1)",
        text,
    )
    text, simple = re.subn(
        r"generationRandom\(rng\)\s*%\s*([A-Za-z_][A-Za-z0-9_.]*|[0-9]+)",
        r"boundedRandom(rng, \1)",
        text,
    )

    remaining = text.count("generationRandom(rng)")
    if remaining != 0:
        raise RuntimeError(
            f"{label}: {remaining} generationRandom(rng) calls were not converted"
        )
    if parenthesized + simple != before:
        raise RuntimeError(
            f"{label}: converted {parenthesized + simple} of {before} calls"
        )
    return text


def patch_mode_manager() -> None:
    path = "src/dsp/mode_manager.cpp"
    text = read(path)
    text = replace_once(
        text,
        "int generationRandom(DeterministicRng& rng) {\n"
        "    return static_cast<int>(rng.next() & 0x7FFFFFFFu);\n"
        "}\n",
        "int boundedRandom(DeterministicRng& rng, uint32_t upperExclusive) {\n"
        "    return static_cast<int>(rng.bounded(upperExclusive));\n"
        "}\n",
        "mode_manager helper",
    )
    text = replace_random_modulo_calls(text, "mode_manager.cpp")
    if "generationRandom" in text:
        raise RuntimeError("mode_manager.cpp still mentions generationRandom")
    write(path, text)


def patch_advanced_generator() -> None:
    path = "src/dsp/advanced_pattern_generator.cpp"
    text = read(path)
    text = replace_once(
        text,
        "static inline int generationRandom(DeterministicRng& rng) {\n"
        "    return static_cast<int>(rng.next() & 0x7FFFFFFFu);\n"
        "}\n",
        "static inline int boundedRandom(DeterministicRng& rng, uint32_t upperExclusive) {\n"
        "    return static_cast<int>(rng.bounded(upperExclusive));\n"
        "}\n",
        "advanced generator helper",
    )
    text = replace_random_modulo_calls(text, "advanced_pattern_generator.cpp")
    if "generationRandom" in text:
        raise RuntimeError("advanced_pattern_generator.cpp still mentions generationRandom")
    write(path, text)


def patch_source_regression() -> None:
    path = "tests/test_generation_rng_source_regressions.py"
    text = read(path)
    anchor = (
        "    require(manager.count(\"DeterministicRng rng = makeGenerationRng\") == 4,\n"
        "            \"Synth A, Synth B, full drums and drum-voice generation need local RNG boundaries\")\n"
    )
    addition = anchor + (
        "    require(\"generationRandom\" not in manager and\n"
        "            \"generationRandom\" not in advanced_cpp,\n"
        "            \"migrated generation must not reintroduce masked next() modulo sampling\")\n"
        "    require(\"boundedRandom(\" in manager and\n"
        "            \"boundedRandom(\" in advanced_cpp,\n"
        "            \"all migrated variable-range draws must use DeterministicRng::bounded()\")\n"
    )
    text = replace_once(text, anchor, addition, "source regression bounded assertions")
    write(path, text)


def main() -> None:
    patch_mode_manager()
    patch_advanced_generator()
    patch_source_regression()


if __name__ == "__main__":
    main()
