#!/usr/bin/env python3
"""Generate deterministic C++ bootstrap vocabulary from append-only JSON registries."""

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
QUALITY_PATH = ROOT / "tools/harmony/quality_registry.json"
SCALE_PATH = ROOT / "tools/harmony/scale_registry.json"


def mask(intervals):
    if not intervals or any(value < 0 or value > 11 for value in intervals):
        raise ValueError("pitch-class intervals must be nonempty values in 0..11")
    return sum(1 << value for value in set(intervals))


def validate(rows, key):
    ids = [row["id"] for row in rows]
    if ids != list(range(len(rows))):
        raise ValueError(f"{key} IDs must be contiguous and append-only from zero")
    for row in rows:
        mask(row["intervals"])


def render():
    qualities = json.loads(QUALITY_PATH.read_text())["qualities"]
    scales = json.loads(SCALE_PATH.read_text())["scales"]
    validate(qualities, "quality")
    validate(scales, "scale")
    lines = [
        "#ifndef GROOVEPUTER_GENERATION_HARMONY_GENERATED_HARMONIC_VOCABULARY_H",
        "#define GROOVEPUTER_GENERATION_HARMONY_GENERATED_HARMONIC_VOCABULARY_H",
        "",
        "namespace GroovePuterHarmony {",
        "",
        "inline constexpr ChordQualityDefinition kBootstrapQualityDefinitions[] = {",
    ]
    for row in qualities:
        skeleton = mask(row["skeleton"])
        lines.append(
            f'    {{{row["id"]}, 0x{mask(row["intervals"]):04x}u, '
            f'{len(set(row["intervals"]))}, QualityFamily::{row["family"]}, '
            f'0x{skeleton:04x}u, {row["complexity"]}}},'
        )
    lines += ["};", "", "inline constexpr ScaleDefinition kBootstrapScaleDefinitions[] = {"]
    for row in scales:
        lines.append(
            f'    {{{row["id"]}, 0x{mask(row["intervals"]):04x}u, '
            f'{len(set(row["intervals"]))}, ScaleFamily::{row["family"]}, '
            f'{row["mode"]}, {row["parent"]}}},'
        )
    lines += [
        "};",
        "",
        "}  // namespace GroovePuterHarmony",
        "",
        "#endif  // GROOVEPUTER_GENERATION_HARMONY_GENERATED_HARMONIC_VOCABULARY_H",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    destination = Path(args.out)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(render())


if __name__ == "__main__":
    main()
