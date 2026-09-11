#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/dsp/miniacid_engine.h"
CPP = ROOT / "src/dsp/miniacid_engine.cpp"


def replace_exact(text: str, old: str, new: str, expected: int, label: str) -> str:
    count = text.count(old)
    if count == 0 and text.count(new) == expected:
        print(f"Task3 already applied: {label}")
        return text
    if count != expected:
        raise RuntimeError(
            f"Task3 patch guard failed for {label}: expected {expected} old occurrence(s), found {count}"
        )
    return text.replace(old, new)


def main() -> int:
    header = HEADER.read_text()
    cpp = CPP.read_text()

    header = replace_exact(
        header,
        '#include "src/state/material_slot.h"\n',
        '#include "src/state/material_slot.h"\n#include "src/state/working_material_storage.h"\n',
        1,
        "working storage include",
    )
    header = replace_exact(
        header,
        '  PhraseRuntime::RuntimeSynthEventBuffer currentPhrase_[NUM_303_VOICES]{};\n',
        '  GroovePuterMaterial::WorkingMaterialStorage workingMaterial_[NUM_303_VOICES]{};\n',
        1,
        "physical member replacement",
    )

    replacements = [
        (
            '  const uint16_t length = currentPhrase_[clamp303Voice(voiceIndex)].lengthTicks;\n',
            '  const uint16_t length =\n      workingMaterial_[clamp303Voice(voiceIndex)].melody().lengthTicks;\n',
            1,
            "phraseRelativeTick",
        ),
        (
            '  const PhraseRuntime::RuntimeSynthEventBuffer& phrase = currentPhrase_[idx];\n',
            '  const PhraseRuntime::RuntimeSynthEventBuffer& phrase =\n      workingMaterial_[idx].melody();\n',
            1,
            "phraseEventAt",
        ),
        (
            '      currentPhrase_[voice] = *pending.melody;\n',
            '      workingMaterial_[voice].storeMelody(*pending.melody);\n',
            1,
            "pending activation copy",
        ),
        (
            '  const uint16_t lengthTicks = currentPhrase_[voiceIndex].lengthTicks;\n',
            '  const uint16_t lengthTicks = workingMaterial_[voiceIndex].melody().lengthTicks;\n',
            1,
            "currentPhrasePlayTick",
        ),
        (
            '  currentPhrase_[voiceIndex] = candidate;\n',
            '  workingMaterial_[voiceIndex].storeMelody(candidate);\n',
            1,
            "makePhrase commit",
        ),
        (
            '  auto candidate = currentPhrase_[voiceIndex];\n',
            '  auto candidate = workingMaterial_[voiceIndex].melody();\n',
            1,
            "setPhraseLength prepare",
        ),
        (
            '  return RuntimePhraseEdit::commit(currentPhrase_[voiceIndex], candidate);\n',
            '  return RuntimePhraseEdit::commit(\n      workingMaterial_[voiceIndex].melody(), candidate);\n',
            1,
            "setPhraseLength commit",
        ),
        (
            '  return currentPhrase_[clamp303Voice(voiceIndex)];\n',
            '  return workingMaterial_[clamp303Voice(voiceIndex)].melody();\n',
            2,
            "currentPhraseBuffer compatibility accessors",
        ),
    ]

    for old, new, expected, label in replacements:
        cpp = replace_exact(cpp, old, new, expected, label)

    if "currentPhrase_" in header or "currentPhrase_" in cpp:
        raise RuntimeError("Task3 patch incomplete: currentPhrase_ still exists in engine source")
    if header.count("workingMaterial_[NUM_303_VOICES]") != 1:
        raise RuntimeError("Task3 patch invariant failed: expected one WorkingMaterialStorage array")

    HEADER.write_text(header)
    CPP.write_text(cpp)
    print("Task3 patch: currentPhrase_ physically replaced by WorkingMaterialStorage")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Task3 patch failed: {exc}", file=sys.stderr)
        raise
