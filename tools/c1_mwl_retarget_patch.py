#!/usr/bin/env python3
from pathlib import Path


def once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"MW-L anchor {label}: expected 1, got {count}")
    return text.replace(old, new, 1)


def patch_engine_header() -> None:
    path = Path("src/dsp/miniacid_engine.h")
    text = path.read_text()

    if "bool tryManual303TargetSwitch(int voiceIndex, int bankIndex, int patternIndex);" not in text:
        anchor = "  bool hasModifiedWorking303Pattern(int voiceIndex) const;\n"
        api = (
            anchor
            + "  bool tryManual303TargetSwitch(int voiceIndex, int bankIndex, int patternIndex);\n"
            + "  bool tryManual303TargetSwitch(int voiceIndex, int patternIndex);\n"
            + "  bool tryManual303BankSwitch(int voiceIndex, int bankIndex);\n"
            + "  bool tryManualPageSwitch(int pageIndex);\n"
        )
        text = once(text, anchor, api, "guard declarations")

    if "inline bool MiniAcid::tryManual303TargetSwitch(" not in text:
        impl = r'''
inline bool MiniAcid::tryManual303TargetSwitch(
    int voiceIndex, int bankIndex, int patternIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  if (bankIndex < 0 || bankIndex >= kBankCount || patternIndex < 0 ||
      patternIndex >= Bank<SynthPattern>::kPatterns) return false;
  const int idx = clamp303Voice(voiceIndex);
  if (current303BankIndex(idx) == bankIndex &&
      display303LocalPatternIndex(idx) == patternIndex) {
    return true;
  }
  if (hasModifiedWorking303Pattern(idx)) return false;
  set303BankIndex(idx, bankIndex);
  set303PatternIndex(idx, patternIndex);
  return current303BankIndex(idx) == bankIndex &&
         display303LocalPatternIndex(idx) == patternIndex;
}

inline bool MiniAcid::tryManual303TargetSwitch(
    int voiceIndex, int patternIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  return tryManual303TargetSwitch(
      voiceIndex, current303BankIndex(voiceIndex), patternIndex);
}

inline bool MiniAcid::tryManual303BankSwitch(
    int voiceIndex, int bankIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  const int patternIndex = display303LocalPatternIndex(voiceIndex);
  if (patternIndex < 0) return false;
  return tryManual303TargetSwitch(voiceIndex, bankIndex, patternIndex);
}

inline bool MiniAcid::tryManualPageSwitch(int pageIndex) {
  if (pageIndex < 0 || pageIndex >= kMaxPages) return false;
  if (pageIndex == currentPageIndex()) return true;
  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
    if (hasModifiedWorking303Pattern(voice)) return false;
  }
  requestPageSwitch(pageIndex);
  return targetPageIndex() == pageIndex || currentPageIndex() == pageIndex;
}
'''
        text = once(
            text,
            "#endif // MINIACID_ENGINE_H",
            impl + "\n#endif // MINIACID_ENGINE_H",
            "guard implementations",
        )

    path.write_text(text)


def replace_method(path_str: str, old: str, new: str, required: bool = True) -> None:
    path = Path(path_str)
    text = path.read_text()
    count = text.count(old)
    if required and count == 0:
        raise SystemExit(f"MW-L {path_str}: missing expected {old}")
    if count:
        text = text.replace(old, new)
        path.write_text(text)


def patch_ui() -> None:
    # Manual synth target selection. The low-level engine setters remain intact
    # for Song/runtime authority; only UI-facing paths are redirected.
    for path in (
        "src/ui/pages/sequencer_hub_page.cpp",
        "src/ui/pages/pattern_edit_page_legacy.h",
        "src/ui/pages/pattern_edit_page.cpp",
    ):
        replace_method(path, "mini_acid_.set303PatternIndex(",
                       "mini_acid_.tryManual303TargetSwitch(")
        replace_method(path, "mini_acid_.set303BankIndex(",
                       "mini_acid_.tryManual303BankSwitch(", required=False)

    # Every page-navigation call in these files is a human/UI transition; Song
    # runtime auto-paging stays on requestPageSwitch inside MiniAcid.
    for path in (
        "src/ui/miniacid_display.cpp",
        "src/ui/pages/song_page.cpp",
        "src/ui/pages/pattern_edit_page_legacy.h",
        "src/ui/pages/drum_sequencer_page_legacy.h",
    ):
        replace_method(path, ".requestPageSwitch(", ".tryManualPageSwitch(")


def main() -> None:
    patch_engine_header()
    patch_ui()


if __name__ == "__main__":
    main()
