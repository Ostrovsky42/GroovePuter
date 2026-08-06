#!/usr/bin/env python3
"""Apply the accepted review fixes for Phrase Arranger Stage 2.

The script is intentionally deterministic and idempotent. It exists so the
CI-only validation PR can build and test the exact candidate before the same
file contents are committed to the experimental branch.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    if new in text:
        return
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one old block, found {count}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


def append_once(path: str, marker: str, block: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    if marker in text:
        return
    if not text.endswith("\n"):
        text += "\n"
    target.write_text(text + "\n" + block.rstrip() + "\n", encoding="utf-8")


def patch_phrase_page() -> None:
    replace_once(
        "src/ui/pages/phrase_page.cpp",
        """    if (key == '\\b' || key == 0x7F) {
      return ui_event.shift ? clearArrangementChain()
                            : removeArrangementSlot();
    }
""",
        """    if (key == '\\b' || key == 0x7F) {
      // Plain Backspace is deliberately non-destructive in ARRANGE. Requiring
      // Ctrl prevents a missed Tab press from clearing a Phrase slot in CORE.
      if (!ui_event.ctrl) return true;
      return ui_event.shift ? clearArrangementChain()
                            : removeArrangementSlot();
    }
""",
    )
    replace_once(
        "src/ui/pages/phrase_page.cpp",
        """  if (key == '\\b' || key == 0x7F) return clearCurrentSlot();
""",
        """  if (key == '\\b' || key == 0x7F) {
    // Ctrl-modified Backspace belongs to ARRANGE only. Consume it in CORE so
    // an unsuccessful view switch cannot clear the selected Phrase slot.
    return ui_event.ctrl ? true : clearCurrentSlot();
  }
""",
    )
    replace_once(
        "src/ui/pages/phrase_page.cpp",
        """                         \"ENT:CAP D:DERIVE W:WRITE SH+W:OVER\");
""",
        """                         \"ENT:CAP D:DERIVE W:SAFE A+W:OVER\");
""",
    )
    replace_once(
        "src/ui/pages/phrase_page.cpp",
        """                         \"W:WRITE SH+W:OVER DEL:RM SH+DEL:CLEAR\");
""",
        """                         \"W:SAFE A+W:OVER C+BK:RM CS+BK:CLR\");
""",
    )


def patch_help() -> None:
    replace_once(
        "src/ui/global_help_content.h",
        """    \"=== PHRASE CORE ===\",
    \"1..4        Select Phrase A/B/C/D\",
    \"Up/Down     Capture length 1/2/4/8\",
    \"Left/Right  Preview Phrase bar\",
    \"R           Cycle capture role\",
    \"Shift+R     Previous role\",
    \"P           Cycle derive parent\",
    \"Enter       Capture current Song row\",
    \"D           Derive parent into slot\",
    \"W           Write to empty Song row\",
    \"Alt+W       Overwrite Song row\",
    \"Bksp/Del    Clear selected Phrase\",
    \"REF         Mutable pattern references\",
""",
        """    \"=== PHRASE CORE / ARRANGE ===\",
    \"Tab         Switch Core / Arrange\",
    \"1..4        Slot or chain assignment\",
    \"Core arrows Capture length / preview\",
    \"Enter       Capture current Song row\",
    \"D           Derive parent into slot\",
    \"W           Safe write to Song\",
    \"Alt+W       Explicit overwrite\",
    \"Bksp/Del    Clear Phrase in Core\",
    \"Ctrl+Bksp   Remove Arrange position\",
    \"Ctrl+Sh+Bk  Clear Arrange chain\",
    \"Arrange arrows Navigate only\",
    \"REF         Mutable pattern references\",
""",
    )


def patch_source_tests() -> None:
    replace_once(
        "tests/test_phrase_ui_source_regressions.py",
        """    '"ENT:CAP D:DERIVE W:WRITE SH+W:OVER"',
    '"TAB:CORE L/R:POS U/D:+8 1-4:SET"',
    '"W:WRITE SH+W:OVER DEL:RM SH+DEL:CLEAR"',
    "if (key == '\\\\t')",
""",
        """    '"ENT:CAP D:DERIVE W:SAFE A+W:OVER"',
    '"TAB:CORE L/R:POS U/D:+8 1-4:SET"',
    '"W:SAFE A+W:OVER C+BK:RM CS+BK:CLR"',
    "if (key == '\\\\t')",
    "if (!ui_event.ctrl) return true;",
    "return ui_event.ctrl ? true : clearCurrentSlot();",
""",
    )
    replace_once(
        "tests/test_global_help_content.cpp",
        """    assert(sectionContains(WorkflowPages::kPhrase, \"Select Phrase A/B/C/D\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Capture current Song row\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Mutable pattern references\"));
""",
        """    assert(sectionContains(WorkflowPages::kPhrase, \"Switch Core / Arrange\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Slot or chain assignment\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Capture current Song row\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Remove Arrange position\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Clear Arrange chain\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Navigate only\"));
    assert(sectionContains(WorkflowPages::kPhrase, \"Mutable pattern references\"));
""",
    )
    replace_once(
        "tests/test_global_help_source_regressions.py",
        """assert '"1..4        Select Phrase A/B/C/D"' in help_content
assert '"Enter       Capture current Song row"' in help_content
assert '"REF         Mutable pattern references"' in help_content
""",
        """assert '"Tab         Switch Core / Arrange"' in help_content
assert '"1..4        Slot or chain assignment"' in help_content
assert '"Enter       Capture current Song row"' in help_content
assert '"Ctrl+Bksp   Remove Arrange position"' in help_content
assert '"Ctrl+Sh+Bk  Clear Arrange chain"' in help_content
assert '"Arrange arrows Navigate only"' in help_content
assert '"REF         Mutable pattern references"' in help_content
""",
    )


def patch_boundary_test() -> None:
    test_block = r'''
void testExactSongCapacityBoundary() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song source = makeSong(8, 1);
  const PhraseCore::SlotId slots[] = {
      PhraseCore::SlotId::A,
      PhraseCore::SlotId::B,
      PhraseCore::SlotId::C,
      PhraseCore::SlotId::D,
  };
  for (PhraseCore::SlotId slot : slots) {
    assert(PhraseCore::captureSongRegion(
        bank, slot, source, 0, 0, 8,
        PhraseCore::Role::Main, PhraseCore::Source::InternalPattern));
  }
  for (uint8_t position = 0; position < PhraseCore::kArrangementCapacity;
       ++position) {
    assert(PhraseCore::assignArrangementStep(
        bank, position, slots[position % 4]));
  }
  assert(bank.arrangement.length == 16);
  assert(PhraseCore::arrangementTotalBars(bank) == 128);

  Song exact = emptySong();
  const auto exactWrite = PhraseCore::writeArrangementToSong(
      bank, exact, 0, false);
  assert(exactWrite);
  assert(exactWrite.totalBars == 128);
  assert(exact.length == Song::kMaxPositions);
  assert(exact.positions[127].patterns[0] == 8);

  Song offset = emptySong();
  const Song before = offset;
  const auto range = PhraseCore::writeArrangementToSong(
      bank, offset, 1, false);
  assert(!range);
  assert(range.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&offset, &before, sizeof(offset)) == 0);
}
'''
    replace_once(
        "tests/test_phrase_arranger.cpp",
        """void testSanitizeDropsDanglingEntries() {
""",
        test_block + "\nvoid testSanitizeDropsDanglingEntries() {\n",
    )
    replace_once(
        "tests/test_phrase_arranger.cpp",
        """  testArrangementPersistenceAndLegacyDecode();
  testSanitizeDropsDanglingEntries();
""",
        """  testArrangementPersistenceAndLegacyDecode();
  testExactSongCapacityBoundary();
  testSanitizeDropsDanglingEntries();
""",
    )


def patch_docs() -> None:
    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_DESIGN.md",
        """- no DSP, transport, pattern generator or MIDI scheduler changes.
""",
        """- no DSP, transport, pattern generator or MIDI scheduler changes.

## Memory contract

- `PhraseArrangement`: **18 bytes** (`16 slots + length + reserved`);
- `PhraseBank`: **262 bytes**;
- previous Phrase Core contract: **244 bytes**;
- Stage 2 fixed-DRAM delta: **+18 bytes**.

This is a structure-size contract, not a new global DRAM budget. The existing
Cardputer ADV fixed-DRAM gate must still pass before hardware acceptance.
""",
    )
    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_DESIGN.md",
        """Backspace       Remove selected position
Shift+Backspace Clear the chain
""",
        """Backspace       No destructive action in Arrange
Ctrl+Backspace  Remove selected position
Ctrl+Shift+Bksp Clear the chain
""",
    )
    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_DESIGN.md",
        """- generated arrangement templates;
""",
        """- move/reorder commands; arrows navigate the cursor only, so order changes
  currently require reassigning the affected positions;
- generated arrangement templates;
""",
    )

    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_ACCEPTANCE.md",
        """- fixed RAM/layout assertions;
""",
        """- fixed RAM/layout assertions: `PhraseArrangement=18`, `PhraseBank=262`
  bytes, Stage 2 delta `+18` bytes;
""",
    )
    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_ACCEPTANCE.md",
        """- full-capacity handling;
""",
        """- full-capacity handling, including the exact `16 x 8B = 128B`
  Song boundary and atomic row-1 range rejection;
""",
    )
    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_ACCEPTANCE.md",
        """- `A A B A C A B D` can be entered and edited;
""",
        """- `A A B A C A B D` can be entered and reassigned;
- arrows navigate positions only; no move/reorder command is claimed;
- plain Backspace in Arrange is non-destructive;
- `Ctrl+Backspace` removes one position and `Ctrl+Shift+Backspace` clears;
""",
    )

    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_TEST_SEQUENCE.md",
        """10. Clear Phrase B: all B entries must disappear from the chain.
11. Reboot after autosave: remaining chain must restore exactly.
""",
        """10. Confirm plain Backspace in ARRANGE changes nothing.
11. Ctrl+Backspace removes one entry; Ctrl+Shift+Backspace clears the chain.
12. Confirm arrows navigate only; reorder requires reassignment.
13. Clear Phrase B: all B entries must disappear from the chain.
14. Set A/B/C/D to 8B and fill all 16 positions: expect TOTAL 128B.
15. Write at row 0: expect exact success through row 128.
16. Repeat from row 1 in a fresh Song: expect RANGE and zero modification.
17. Reboot after autosave: remaining chain must restore exactly.
""",
    )

    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_HARDWARE_TEST.md",
        """- `Backspace`: remove the selected position and close the gap;
- `Shift+Backspace`: clear the complete chain.
""",
        """- plain `Backspace`: consumed with no destructive action;
- `Ctrl+Backspace`: remove the selected position and close the gap;
- `Ctrl+Shift+Backspace`: clear the complete chain.
""",
    )
    replace_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_HARDWARE_TEST.md",
        """- removing one position shifts later items left;
""",
        """- arrows move the cursor only; Stage 2 has no move/reorder command;
- changing order requires reassigning the affected positions;
- removing one position shifts later items left;
""",
    )
    append_once(
        "docs/stages/PHRASE_ARRANGER_STAGE_2_HARDWARE_TEST.md",
        "### 9. Exact 128-bar boundary",
        """### 9. Exact 128-bar boundary

1. Capture A, B, C and D as 8-bar Phrases.
2. Fill all 16 arrangement positions.
3. Confirm `TOTAL 128B`.
4. Use a completely empty Song and set the destination to row 1 (zero-based row 0).
5. Press `W`.

Expected: all 128 rows are written exactly and Song length becomes 128.

Repeat with a fresh empty Song from the next row (zero-based row 1).

Expected: `ARR WRITE: RANGE` appears before any Song row changes.

## Published memory contract

```text
Phrase Core PhraseBank: 244 bytes
PhraseArrangement:       18 bytes
Stage 2 PhraseBank:      262 bytes
Fixed-DRAM delta:        +18 bytes
```

The normal Cardputer ADV fixed-DRAM gate remains mandatory; these structure
sizes do not replace the repository-wide budget.
""",
    )


def main() -> None:
    patch_phrase_page()
    patch_help()
    patch_source_tests()
    patch_boundary_test()
    patch_docs()
    print("Phrase Arranger review fixes applied")


if __name__ == "__main__":
    main()
