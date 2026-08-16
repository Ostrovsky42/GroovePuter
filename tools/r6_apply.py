#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, got {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# Central UI routing: translate one conflict-free physical chord into the
# already-existing application Undo event before the active page sees it.
display = ROOT / "src/ui/miniacid_display.cpp"
replace_once(
    display,
    '#include "src/state/scene_revision.h"\n',
    '#include "src/state/scene_revision.h"\n#include "src/state/undo_owner.h"\n#include "undo_ux.h"\n',
    "display Undo UX includes",
)
replace_once(
    display,
    '    IPage* currentPage = getPage_(page_index_);\n',
    '    // R6 exposes one global user gesture while preserving page ownership.\n'
    '    // The active page still decides whether it can restore the retained\n'
    '    // domain receipt; this layer only promotes Ctrl+U to APP_EVENT_UNDO.\n'
    '    GroovePuterUndoUx::promoteUndoShortcut(event);\n\n'
    '    IPage* currentPage = getPage_(page_index_);\n',
    "display shortcut promotion",
)
replace_once(
    display,
    '    if (event.event_type == GROOVEPUTER_APPLICATION_EVENT) {\n',
    '    // If the active page declined Undo, do not restore another domain here.\n'
    '    // A retained receipt remains intact so the user can return to its owner.\n'
    '    if (GroovePuterUndoUx::isUndoEvent(event)) {\n'
    '        const bool hasReceipt = GroovePuterUndo::undoOwner().hasUndo();\n'
    '        UI::showToast(GroovePuterUndoUx::fallbackToast(hasReceipt), 1000);\n'
    '        return true;\n'
    '    }\n\n'
    '    if (event.event_type == GROOVEPUTER_APPLICATION_EVENT) {\n',
    "display Undo fallback",
)

# Normalize domain success/failure copy without changing restore semantics.
replace_once(ROOT / "src/ui/pages/song_page_r4_owner.inc",
             'showToast("Undo Song", 900);',
             'showToast("UNDO: SONG", 900);',
             "Song success toast")
replace_once(ROOT / "src/ui/pages/song_page_r4_owner.inc",
             'showToast("Undo target unavailable", 1100);',
             'showToast("UNDO: RETURN PAGE", 1100);',
             "Song unavailable toast")

phrase = ROOT / "src/ui/pages/phrase_page.cpp"
text = phrase.read_text(encoding="utf-8")
for old, new, label in (
    ('UI::showToast("Undo Phrase", 900);', 'UI::showToast("UNDO: PHRASE", 900);', "Phrase success toast"),
    ('UI::showToast("Undo Song", 900);', 'UI::showToast("UNDO: SONG", 900);', "Phrase Song success toast"),
    ('UI::showToast("Undo target unavailable", 1100);', 'UI::showToast("UNDO: RETURN PAGE", 1100);', "Phrase unavailable toast"),
):
    count = text.count(old)
    expected = 2 if label == "Phrase unavailable toast" else 1
    if count != expected:
        raise RuntimeError(f"{label}: expected {expected} matches, got {count}")
    text = text.replace(old, new)
phrase.write_text(text, encoding="utf-8")

pattern = ROOT / "src/ui/pages/pattern_edit_page_legacy.h"
replace_once(pattern, 'UI::showToast("NOTHING TO UNDO", 800);',
             'UI::showToast("UNDO: EMPTY", 800);', "Pattern empty toast")
replace_once(pattern, 'UI::showToast("UNDO EXPIRED", 900);',
             'UI::showToast("UNDO: EXPIRED", 900);', "Pattern expired toast")

# On-device help and external key map both expose the same global chord.
help_content = ROOT / "src/ui/global_help_content.h"
replace_once(help_content,
             '    "Alt+H       Toggle this help",\n',
             '    "Alt+H       Toggle this help",\n    "Ctrl+U      Undo last edit",\n',
             "global help Undo row")

keys = ROOT / "src/ui/docs/keys.md"
replace_once(keys,
             '| `Alt+H` | Toggle page-aware help |\n',
             '| `Alt+H` | Toggle page-aware help |\n| `Ctrl+U` | Undo last retained Pattern / Song / Phrase edit |\n',
             "external key map Undo row")

# The helper is deliberately stateless: no receipt, page or Scene ownership.
undo_ux = ROOT / "src/ui/undo_ux.h"
undo_ux.write_text(r'''#pragma once

#include "ui_core.h"

namespace GroovePuterUndoUx {

inline bool isUndoShortcut(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN || !event.ctrl ||
      event.alt || event.meta || event.shift) {
    return false;
  }
  const unsigned char key = static_cast<unsigned char>(event.key);
  return event.scancode == GROOVEPUTER_U || event.key == 'u' || event.key == 'U' ||
         key == 21;  // Ctrl+U control character on terminals/Cardputer paths.
}

inline bool promoteUndoShortcut(UIEvent& event) {
  if (!isUndoShortcut(event)) return false;
  event.event_type = GROOVEPUTER_APPLICATION_EVENT;
  event.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
  return true;
}

inline bool isUndoEvent(const UIEvent& event) {
  return event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
         event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO;
}

inline const char* fallbackToast(bool hasRetainedReceipt) {
  return hasRetainedReceipt ? "UNDO: RETURN PAGE" : "UNDO: EMPTY";
}

}  // namespace GroovePuterUndoUx
''', encoding="utf-8")

TEST = ROOT / "tests/test_undo_0_9_8_r6_ux_source_regressions.py"
TEST.write_text(r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    ux = (ROOT / "src/ui/undo_ux.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    pattern = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")
    song = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    phrase = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    sound = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    help_content = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
    keys = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")
    owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")

    require("event.ctrl" in ux and "GROOVEPUTER_U" in ux and "key == 21" in ux,
            "R6 global Ctrl+U recognition is incomplete")
    require("event.alt || event.meta || event.shift" in ux,
            "Undo shortcut must reject modified Ctrl+U variants")
    require("GROOVEPUTER_APP_EVENT_UNDO" in ux and
            "GROOVEPUTER_APPLICATION_EVENT" in ux,
            "Ctrl+U must reuse the existing application Undo event")
    for forbidden in ("UndoOwner", "SceneManager", "AudioGuard", "std::vector", "new "):
        require(forbidden not in ux,
                f"stateless Undo UX helper acquired ownership/state: {forbidden}")

    promotion = display.index("GroovePuterUndoUx::promoteUndoShortcut(event);")
    page_dispatch = display.index("IPage* currentPage = getPage_(page_index_);")
    fallback = display.index("GroovePuterUndoUx::isUndoEvent(event)", page_dispatch)
    app_fallback = display.index(
        "if (event.event_type == GROOVEPUTER_APPLICATION_EVENT)", fallback)
    require(promotion < page_dispatch < fallback < app_fallback,
            "Undo must be promoted before page dispatch and fall back only after page refusal")
    require("undoOwner().hasUndo()" in display[fallback:app_fallback] and
            "fallbackToast(hasReceipt)" in display[fallback:app_fallback],
            "central fallback must distinguish EMPTY from retained other-page receipt")
    require("undoPrepared" not in display[fallback:app_fallback] and
            "restore" not in display[fallback:app_fallback].lower(),
            "MiniAcidDisplay must not become a second Undo restore owner")

    require('"UNDO: PATTERN"' in pattern and '"UNDO: EMPTY"' in pattern and
            '"UNDO: RETURN PAGE"' in pattern,
            "Pattern Undo UX is not normalized")
    require('"UNDO: SONG"' in song and '"UNDO: RETURN PAGE"' in song,
            "Song Undo UX is not normalized")
    require('"UNDO: PHRASE"' in phrase and '"UNDO: SONG"' in phrase and
            phrase.count('"UNDO: RETURN PAGE"') >= 2,
            "Phrase Undo UX is not normalized")

    # Ctrl+Z remains a TB303 parameter reset. R6 must not steal it globally.
    require("if (lowerKey == 'z')" in sound and
            "set303Parameter(TB303ParamId::Cutoff, 800.0f" in sound,
            "Synth Sound Ctrl+Z reset compatibility disappeared")
    require("GROOVEPUTER_Z" not in ux and "'z'" not in ux and '"z"' not in ux,
            "R6 must not bind global Undo to Z")

    require('"Ctrl+U      Undo last edit"' in help_content,
            "on-device global help must expose Ctrl+U Undo")
    require("`Ctrl+U` | Undo last retained Pattern / Song / Phrase edit" in keys,
            "canonical external key map must expose the same Undo chord")

    require("class UndoOwner" in owner and "kUndoPayloadBytes = 1536" in owner,
            "R6 must retain the accepted bounded owner")
    require("static UndoOwner owner" in owner,
            "R6 must keep one authoritative retained owner")

    print("0.9.8 R6 Undo UX source regressions: OK")


if __name__ == "__main__":
    main()
''', encoding="utf-8")

RUNNER = ROOT / "tests/run_undo_0_9_8_r6_tests.sh"
RUNNER.write_text('''#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash tests/run_undo_0_9_8_r5_tests.sh
python3 tests/test_undo_0_9_8_r6_ux_source_regressions.py

echo "0.9.8 R6 Undo UX tests passed"
''', encoding="utf-8")
RUNNER.chmod(0o755)

DOC = ROOT / "docs/releases/0_9_8_R6_UNDO_UX.md"
DOC.write_text('''# 0.9.8 R6 — Undo UX / shortcut consistency

## Scope

R6 exposes the accepted one-level Undo architecture through one user gesture and
one vocabulary. It adds no receipt type, history depth, restore dispatcher or
mutation owner.

## Shortcut decision

`Ctrl+Z` is intentionally **not** global Undo. SYNTH KNOBS uses `Ctrl+Z/X/C/V`
for TB303 parameter resets, including Cutoff reset on `Ctrl+Z`. Stealing that
chord would regress an established instrument control.

R6 therefore uses **`Ctrl+U`** (`U` = Undo). No current literal Ctrl+U binding was
found in the production pages. The Phrase help abbreviation `Ctrl+U/D` means
Ctrl+Up/Down arrows, not the U/D letter keys.

The shortcut is translated to the existing `GROOVEPUTER_APP_EVENT_UNDO` before
the active page receives it, so Cardputer and SDL share the same application
routing.

## Page ownership remains authoritative

The active page remains the restore owner:

- Pattern page -> `UndoKind::Pattern`;
- Song page -> `UndoKind::Song`;
- Phrase page -> `UndoKind::Phrase`, and `UndoKind::Song` for its manual
  Phrase-to-Song write path.

`MiniAcidDisplay` never decodes a receipt and never restores Scene data. If the
active page declines the event, R6 only reports:

- `UNDO: EMPTY` when there is no retained receipt;
- `UNDO: RETURN PAGE` when a receipt still exists for another owner/page.

That retained receipt is left untouched. R6 does not load a paged Scene from SD
or create cross-page restore ownership.

## User messages

Successful domain restores use:

- `UNDO: PATTERN`
- `UNDO: SONG`
- `UNDO: PHRASE`

Unavailable targets report `UNDO: RETURN PAGE`. Pattern expired receipts report
`UNDO: EXPIRED`.

`UndoKind::Generation` remains reserved in the bounded slot, but the merged
0.9.8 line has no current `UndoKind::Generation` publisher. R6 therefore does not
invent a fake generation restore path; generation/activation ownership stays on
its existing 0.9.9 track.

## Help

Both on-device `Alt+H` global help and `src/ui/docs/keys.md` document `Ctrl+U`.

## Acceptance

Focused R6 reruns R2-R5 and checks shortcut compatibility, central no-restore
routing, domain messages, and the preserved Synth Sound Ctrl+Z reset. Normal
exact-head host/SDL/ADV/fixed-DRAM/SEQTRAK gates remain mandatory before merge.
''', encoding="utf-8")
