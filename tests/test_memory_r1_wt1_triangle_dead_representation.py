#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

SYMBOLS = ("lookupTriangle", "triangleTable_")
ALLOWED_CENSUS_FILES = {
    "src/dsp/audio_wavetables.h",
    "src/dsp/audio_wavetables.cpp",
}
REQUIRED_SURVIVORS = ("sawTable_", "squareTable_")


def source_files():
    suffixes = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ino"}
    for path in sorted(SRC.rglob("*")):
        if path.is_file() and path.suffix in suffixes:
            yield path


def collect_occurrences():
    occurrences = []
    for path in source_files():
        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(text.splitlines(), start=1):
            for symbol in SYMBOLS:
                if symbol in line:
                    occurrences.append((rel, line_no, symbol, line.strip()))
    return occurrences


def prove_caller_census(occurrences):
    unexpected_files = sorted({rel for rel, _, _, _ in occurrences} - ALLOWED_CENSUS_FILES)
    if unexpected_files:
        raise AssertionError(
            "WT1 caller census found production consumers outside wavetable ownership files: "
            + ", ".join(unexpected_files)
        )

    lookup_occurrences = [item for item in occurrences if item[2] == "lookupTriangle"]
    if len(lookup_occurrences) != 1:
        raise AssertionError(
            f"WT1 caller census expected exactly one lookupTriangle definition, found {len(lookup_occurrences)}: "
            f"{lookup_occurrences}"
        )
    rel, _, _, line = lookup_occurrences[0]
    if rel != "src/dsp/audio_wavetables.h" or "static inline float lookupTriangle" not in line:
        raise AssertionError(
            "WT1 caller census found lookupTriangle use that is not the inline accessor definition: "
            f"{lookup_occurrences[0]}"
        )

    triangle_occurrences = [item for item in occurrences if item[2] == "triangleTable_"]
    if not triangle_occurrences:
        raise AssertionError("WT1 caller census unexpectedly found no triangleTable_ representation")

    print("WT1 caller census: PASS")
    for rel, line_no, symbol, line in occurrences:
        print(f"  {rel}:{line_no}: {symbol}: {line}")


def prove_dead_representation_removed(occurrences):
    if occurrences:
        rendered = "\n".join(
            f"  {rel}:{line_no}: {symbol}: {line}"
            for rel, line_no, symbol, line in occurrences
        )
        raise AssertionError(
            "WT1 RED: dead triangle wavetable representation is still present:\n" + rendered
        )

    header = (SRC / "dsp" / "audio_wavetables.h").read_text(encoding="utf-8")
    impl = (SRC / "dsp" / "audio_wavetables.cpp").read_text(encoding="utf-8")
    combined = header + "\n" + impl
    for survivor in REQUIRED_SURVIVORS:
        if survivor not in combined:
            raise AssertionError(f"WT1 scope violation: required survivor {survivor} was removed")

    print("WT1 dead triangle representation guard: PASS")


def main():
    occurrences = collect_occurrences()
    prove_caller_census(occurrences)
    prove_dead_representation_removed(occurrences)


if __name__ == "__main__":
    main()
