#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


ux = text("src/ui/undo_ux.h")
display = text("src/ui/miniacid_display.cpp")
sound = text("src/ui/pages/tb303_params_page.cpp")
help_content = text("src/ui/global_help_content.h")
keys = text("src/ui/docs/keys.md")
owner = text("src/state/undo_owner.h")

require("GROOVEPUTER_Z" in ux and "event.key == 'z'" in ux and "event.key == 'Z'" in ux and "key == 26" in ux,
        "Ctrl+Z must be recognized through scancode, printable key and control character")
require("GROOVEPUTER_U" not in ux and "key == 21" not in ux,
        "Ctrl+U must not remain a second public Undo chord")

handle = display.index("bool MiniAcidDisplay::handleEvent(UIEvent event)")
promote = display.index("GroovePuterUndoUx::promoteUndoShortcut(event);", handle)
page = display.index("IPage* currentPage = getPage_(page_index_);", promote)
require(promote < page, "Ctrl+Z must be promoted before any active page can consume it")

reset_start = sound.index("if (isTb303Engine() && ui_event.ctrl")
reset_end = sound.index("if (ui_event.ctrl && !ui_event.alt && key >= '1'", reset_start)
reset_block = sound[reset_start:reset_end]
require("lowerKey == 'z'" not in reset_block, "TB303 params still steal Ctrl+Z")
require("lowerKey == 'a'" in reset_block and "TB303ParamId::Cutoff" in reset_block,
        "Cutoff reset must move to Ctrl+A")
require("lowerKey == 'x'" in reset_block and "lowerKey == 'c'" in reset_block and "lowerKey == 'v'" in reset_block,
        "remaining Synth reset chords changed unexpectedly")

require('"Ctrl+Z      Undo last edit"' in help_content, "on-device global help is stale")
require('"Ctrl+A/X/C/V Reset parameter"' in help_content, "on-device Synth reset help is stale")
require("| `Ctrl+Z` | Undo last retained Pattern / Song / Phrase edit |" in keys,
        "external key map is stale")
require("| `Ctrl+A/X/C/V` | Reset Cutoff / Resonance / Env Amount / Env Decay |" in keys,
        "external Synth reset key map is stale")

# This is still 0.9.8 Safe Editing: no generation restore ownership is pulled in.
require("kUndoPayloadBytes = 1536" in owner and "static UndoOwner owner" in owner,
        "bounded one-level Undo owner changed")
for forbidden in ("GenerationUndoPayload", "PendingGeneration", "PendingActivation"):
    require(forbidden not in ux, f"0.9.8 UX pulled generation ownership backward: {forbidden}")

print("0.9.8 R8 global Ctrl+Z contract: PASS")
