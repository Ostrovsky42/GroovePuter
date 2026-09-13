#!/usr/bin/env python3
from pathlib import Path
import subprocess

DONOR_WORKING = "4500f351b978f8f853d32595c95640e461827268"
DONOR_PROMOTION = "e22990ea31adf268cff85ecf220d63b8cbd86118"
DONOR_RESOLUTION = "730daa7d23f081123f34e122d6f85cfa17ee3158"


def show(commit: str, path: str) -> str:
    return subprocess.check_output(["git", "show", f"{commit}:{path}"], text=True)


def once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"C1 transplant anchor {label}: expected 1, got {count}")
    return text.replace(old, new, 1)


def write_donors() -> None:
    donor_files = {
        "src/state/working_material_storage.h": (DONOR_WORKING, "src/state/working_material_storage.h"),
        "src/state/melody_promotion.h": (DONOR_PROMOTION, "src/state/melody_promotion.h"),
        "src/state/material_version.h": (DONOR_RESOLUTION, "src/state/material_version.h"),
        "src/state/material_resolution.h": (DONOR_RESOLUTION, "src/state/material_resolution.h"),
    }
    for target, (commit, source) in donor_files.items():
        Path(target).write_text(show(commit, source))


def patch_header() -> None:
    path = Path("src/dsp/miniacid_engine.h")
    text = path.read_text()

    if '#include "src/state/working_material_storage.h"' not in text:
        text = once(
            text,
            '#include "src/state/material_slot.h"\n',
            '#include "src/state/material_slot.h"\n'
            '#include "src/state/material_slot_access.h"\n'
            '#include "src/state/working_material_storage.h"\n',
            "engine includes",
        )
    elif '#include "src/state/material_slot_access.h"' not in text:
        text = once(
            text,
            '#include "src/state/material_slot.h"\n',
            '#include "src/state/material_slot.h"\n'
            '#include "src/state/material_slot_access.h"\n',
            "slot access include",
        )

    working_decl = (
        '  // 0.9.11 C1: session Working is bound to stable MaterialReference.\n'
        '  bool adjustWorking303StepNote(int voiceIndex, int stepIndex,\n'
        '                                int semitoneDelta);\n'
        '  const SynthPattern* currentWorking303Pattern(int voiceIndex) const;\n'
        '  bool hasModifiedWorking303Pattern(int voiceIndex) const;\n'
        '  bool tryManual303TargetSwitch(int voiceIndex, int bankIndex, int patternIndex);\n'
        '  bool tryManualPageSwitch(int pageIndex);\n\n'
    )
    if 'bool adjustWorking303StepNote(' not in text:
        text = once(
            text,
            '  void adjust303StepOctave(int voiceIndex, int stepIndex, int octaveDelta);\n\n',
            '  void adjust303StepOctave(int voiceIndex, int stepIndex, int octaveDelta);\n\n' + working_decl,
            "Working public API",
        )

    if 'bool current303MaterialReference_(' not in text:
        text = once(
            text,
            '  const SynthPattern& synthPattern(int synthIndex) const;\n',
            '  bool current303MaterialReference_(\n'
            '      int voiceIndex, GroovePuterMaterial::MaterialReference& out) const;\n'
            '  const SynthPattern& synthPattern(int synthIndex) const;\n',
            "MaterialReference helper declaration",
        )

    phrase_owner = '  PhraseRuntime::RuntimeSynthEventBuffer currentPhrase_[NUM_303_VOICES]{};\n'
    working_owner = '  GroovePuterMaterial::WorkingMaterialStorage workingMaterial_[NUM_303_VOICES]{};\n'
    if phrase_owner in text:
        text = once(text, phrase_owner, working_owner, "unified Working storage")
    elif working_owner not in text:
        raise SystemExit("C1 transplant: neither currentPhrase_ nor unified Working storage found")

    if 'inline bool MiniAcid::current303MaterialReference_(' not in text:
        donor = show(DONOR_WORKING, "src/dsp/miniacid_engine.h")
        start = donor.index('inline bool MiniAcid::current303MaterialReference_(')
        end = donor.rindex('#endif // MINIACID_ENGINE_H')
        block = donor[start:end].rstrip()
        retarget = (
            '\n\ninline bool MiniAcid::tryManual303TargetSwitch(\n'
            '    int voiceIndex, int bankIndex, int patternIndex) {\n'
            '  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;\n'
            '  if (bankIndex < 0 || bankIndex >= kBankCount || patternIndex < 0 ||\n'
            '      patternIndex >= Bank<SynthPattern>::kPatterns) return false;\n'
            '  const int idx = clamp303Voice(voiceIndex);\n'
            '  if (current303BankIndex(idx) == bankIndex &&\n'
            '      display303LocalPatternIndex(idx) == patternIndex) {\n'
            '    return true;\n'
            '  }\n'
            '  if (hasModifiedWorking303Pattern(idx)) return false;\n'
            '  set303BankIndex(idx, bankIndex);\n'
            '  set303PatternIndex(idx, patternIndex);\n'
            '  return current303BankIndex(idx) == bankIndex &&\n'
            '         display303LocalPatternIndex(idx) == patternIndex;\n'
            '}\n\n'
            'inline bool MiniAcid::tryManualPageSwitch(int pageIndex) {\n'
            '  if (pageIndex < 0 || pageIndex >= kMaxPages) return false;\n'
            '  if (pageIndex == currentPageIndex()) return true;\n'
            '  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {\n'
            '    if (hasModifiedWorking303Pattern(voice)) return false;\n'
            '  }\n'
            '  requestPageSwitch(pageIndex);\n'
            '  return targetPageIndex() == pageIndex || currentPageIndex() == pageIndex;\n'
            '}\n'
        )
        text = once(
            text,
            '#endif // MINIACID_ENGINE_H',
            block + retarget + '\n#endif // MINIACID_ENGINE_H',
            "Working inline implementation",
        )

    path.write_text(text)


def patch_cpp() -> None:
    path = Path("src/dsp/miniacid_engine.cpp")
    text = path.read_text()
    replacements = [
        ('currentPhrase_[clamp303Voice(voiceIndex)].lengthTicks',
         'workingMaterial_[clamp303Voice(voiceIndex)].melody().lengthTicks', 1,
         'phrase relative length'),
        ('const PhraseRuntime::RuntimeSynthEventBuffer& phrase = currentPhrase_[idx];',
         'const PhraseRuntime::RuntimeSynthEventBuffer& phrase =\n      workingMaterial_[idx].melody();', 1,
         'phrase event source'),
        ('currentPhrase_[voice] = *pending.melody;',
         'workingMaterial_[voice].storeMelody(*pending.melody);', 1,
         'pending melody activation'),
        ('currentPhrase_[voiceIndex].lengthTicks',
         'workingMaterial_[voiceIndex].melody().lengthTicks', 1,
         'phrase play tick'),
        ('currentPhrase_[voiceIndex] = candidate;',
         'workingMaterial_[voiceIndex].storeMelody(candidate);', 1,
         'make phrase commit'),
        ('auto candidate = currentPhrase_[voiceIndex];',
         'auto candidate = workingMaterial_[voiceIndex].melody();', 1,
         'phrase length prepare'),
        ('RuntimePhraseEdit::commit(currentPhrase_[voiceIndex], candidate)',
         'RuntimePhraseEdit::commit(\n      workingMaterial_[voiceIndex].melody(), candidate)', 1,
         'phrase length commit'),
        ('return currentPhrase_[clamp303Voice(voiceIndex)];',
         'return workingMaterial_[clamp303Voice(voiceIndex)].melody();', 2,
         'phrase buffer accessors'),
    ]
    for old, new, expected, label in replacements:
        count = text.count(old)
        if count != expected:
            raise SystemExit(f"C1 transplant cpp anchor {label}: expected {expected}, got {count}")
        text = text.replace(old, new)
    if 'currentPhrase_[' in text:
        raise SystemExit("C1 transplant left currentPhrase_ references in engine cpp")
    path.write_text(text)


def main() -> None:
    write_donors()
    patch_header()
    patch_cpp()


if __name__ == "__main__":
    main()
