#!/usr/bin/env python3
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]

help_content = (ROOT / "src/ui/global_help_content.h").read_text()
overlay = (ROOT / "src/ui/global_help_overlay.h").read_text()
display = (ROOT / "src/ui/miniacid_display.cpp").read_text()
smf = (ROOT / "src/ui/pages/smf_player_page_structural.cpp").read_text()
drum = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text()
drum_legacy = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text()
workflow = (ROOT / "src/ui/workflow_mode.h").read_text()
readme = (ROOT / "README.md").read_text()
manual = (ROOT / "MANUAL.md").read_text()
keys = (ROOT / "src/ui/docs/keys.md").read_text()
docs_index = (ROOT / "docs/README.md").read_text()
quickstart_path = ROOT / "docs/user/QUICKSTART.md"
groove_lab = (ROOT / "docs/GROOVE_LAB.md").read_text()
release = (ROOT / "docs/releases/0_9_1_RELEASE.md").read_text()
integration = (
    ROOT / "docs/stages/INTEGRATED_GENERATE_PHRASE_ACCEPTANCE.md"
).read_text()

# Existing on-device help routing remains intact.
assert '"Alt+H       Toggle this help"' in help_content
assert "Ctrl+H" not in help_content
assert '"HELP: %s  ESC/ALT+H"' in overlay
assert "event.alt && (event.key == 'h' || event.key == 'H')" in overlay
assert "event.ctrl || event.alt" not in overlay

alt_h_handler = re.search(
    r"if \(event\.alt && \(event\.key == 'h'.*?return true;\n\s*}",
    display,
    re.S,
)
assert alt_h_handler, "Alt+H global handler missing"
# Alt+H opens the help of the current page, at that page's own section.
assert "openPageHelp_()" in alt_h_handler.group(0)
open_page_help = re.search(r"void MiniAcidDisplay::openPageHelp_\(\) \{.*?\n\}", display, re.S)
assert open_page_help, "openPageHelp_ missing"
assert "setPageContext(page_index_)" in open_page_help.group(0)
assert "helpAnchor()" in open_page_help.group(0)

for page_constant in (
    "kGenre", "kSynthA", "kSynthB", "kSynthAParameters",
    "kSynthBParameters", "kDrums", "kArrange", "kPhrase", "kPattern",
    "kTexture", "kFeel", "kProject", "kGeneration", "kPerform", "kPlayer",
):
    assert f"WorkflowPages::{page_constant}" in help_content

assert "Alt+H remains reserved for page-aware help" in smf
assert "Ctrl+H" not in smf
assert "drawDrumInputLockedFooter" in drum
assert '"ARROWS:GRID Q-I:PAT"' in drum
assert '"C1/2:BANK Alt[]:PAGE"' in drum
assert '"G:GEN Alt+G:ALL Q-I:PAT B:Bank"' in drum_legacy
assert '"DRUM Alt[]:PG"' in drum_legacy
assert '"REF         Mutable pattern references"' in help_content

# Runtime workflow truth: 12 active pages. Generation/Texture and standalone
# SOUND ids remain persisted compatibility aliases, not live pages.
assert "case WorkflowMode::Perform: return 2;" in workflow
assert "case WorkflowMode::Generate: return 2;" in workflow
assert "case WorkflowMode::Hub: return 4;" in workflow
assert "case WorkflowMode::Song: return 3;" in workflow
assert "case WorkflowMode::Settings: return 1;" in workflow
assert "if (page == kTexture || page == kGeneration) return kFeel;" in workflow
assert "if (page == kSynthAParameters) return kSynthA;" in workflow
assert "if (page == kSynthBParameters) return kSynthB;" in workflow
assert "kGenre, kFeel" in workflow
assert "kPattern, kSynthA, kSynthB, kDrums" in workflow

# 0.9.17 First Five Minutes product documentation contract.
assert readme.startswith("# GroovePuter\n")
assert "M5Stack Cardputer ADV" in readme
assert "v0.9.16" in readme
assert "docs/user/QUICKSTART.md" in readme
assert "0.9.14 / 0.9.15 public-beta candidate" not in readme
assert "docs/README.md" in readme
assert "docs/PRODUCT_POSITIONING.md" in readme
assert "GENRE != FEEL != SOUND" in readme
assert "Fn+M      workspace launcher" in readme

assert quickstart_path.exists(), "0.9.17 user quick start is missing"
quickstart = quickstart_path.read_text()
assert quickstart.startswith("# GroovePuter 0.9.17 — First Five Minutes")
for action in ("Space", "Fn+M", "G", "D", "Ctrl+Z", "Alt+H"):
    assert action in quickstart, action
for internal_term in ("P3", "provenance", "lineage", "MaterialVersion", "ReferenceRole"):
    assert internal_term not in quickstart, internal_term
assert "legacy FEEL page" not in quickstart
assert "`Alt+V` opens GENRE" in quickstart

assert "../user/QUICKSTART.md" in docs_index or "user/QUICKSTART.md" in docs_index
assert "0.9.16" in docs_index
assert "older 0.9.1-era manual" not in docs_index

assert manual.startswith("# GroovePuter 0.9.17 Manual")
assert "docs/user/QUICKSTART.md" in manual
assert "GENERATE: GENRE -> FEEL" in manual
assert "SONG:     SONG -> MATERIAL -> MATERIAL BANK" in manual
assert "DEVELOP + BREAK" in manual
assert "`Alt+Enter` / `Ctrl+Enter`: ACCEPT" in manual
assert "Pattern or Melody" in manual
assert "GENERATION -> FEEL" in manual
assert "TEXTURE    -> FEEL" in manual
assert "GENRE 1/3" not in manual
assert "GENERATION 3/3" not in manual

# Hard-global ownership is part of the key truth. The v0.9.16 Foundation sent
# Alt+V to legacy page 11 (FEEL); the focused 0.9.17 navigation fix routes it to
# GENRE, and this witness changed together with the docs, as intended.
alt_v_handler = re.search(
    r"if \(event\.alt && \(event\.key == 'v'.*?return true;\n\s*}",
    display,
    re.S,
)
assert alt_v_handler, "Alt+V global handler missing"
assert "goToPage(WorkflowPages::kGenre);" in alt_v_handler.group(0)
assert "kGeneration = 11" in workflow
assert "if (page == kTexture || page == kGeneration) return kFeel;" in workflow
assert "Fn+M -> GENRE" in manual
assert "Alt+V` is **not** a reachable Synth CONNECT shortcut" in manual
assert "Alt+X` is **not** a reachable Synth DISCARD shortcut" in manual

# Alt+X is hard-global LiveMix before page dispatch. The key map must not claim
# it as Synth DISCARD, while Alt+Backspace remains the documented reachable path.
assert "LiveMix: ON" in display and "event.alt && (event.key == 'x'" in display
assert "| `Alt+Backspace` / `Alt+X` | Discard working edits" not in keys
assert "| `Alt+Backspace` | Discard working edits to accepted material |" in keys
assert "`Alt+X` is reserved globally for LiveMix" in keys

# Canonical key map: beginner constitution at the top plus full expert reference.
assert keys.startswith("# GroovePuter 0.9.17 Key Map")
assert "## First Five Minutes" in keys
for expected in (
    "`Space`",
    "`Fn+M`",
    "Generate the thing you are looking at",
    "`D`",
    "`Ctrl+Z`",
    "`Alt+H`",
):
    assert expected in keys
assert "| `Alt+V` | GENRE |" in keys
assert "GENERATE: GENRE -> FEEL" in keys
assert "MATERIAL BANK keeps `D` as derive" in keys
assert "slot's saved descriptor selects" in keys
assert "## GENRE 1/2" in keys
assert "## FEEL 2/2" in keys
assert "## MATERIAL BANK" in keys
assert "## GENERATION 3/3" not in keys
assert "PAUSE MIDI FIRST" not in keys

# On-device Help must be truthful about current hard-global navigation and expose
# the actual MATERIAL hero actions.
assert '"Fn+M        Workspace launcher"' in help_content
assert '"Alt+V       GENRE"' in help_content
assert '"Alt+V       FEEL (legacy)"' not in help_content
assert '"Alt+V       Groove Lab"' not in help_content
assert '"G           New TAKE at TO"' in help_content
assert '"D           DEVELOP fresh TAKE"' in help_content
assert '"R           Make room / reuse"' in help_content

assert groove_lab.startswith("# Groove Lab — Historical Page Note")
assert "Mode Page is retired" in groove_lab
assert "GENRE 1/2 -> FEEL 2/2" in groove_lab

assert release.startswith("# GroovePuter 0.9.1 — Release Record")
assert "170bbe1407daf37621949301a34a5ec345844b24" in release
assert "4cd8244b091e748ddf93819a03c051d978e01266" in release
assert "## Runtime freeze" in release
assert "## Hardware acceptance" in release
assert "## Known deferred" in release

# Historical integrated Phrase acceptance remains reproducible evidence.
for expected in (
    "## Purpose",
    "## Hardware list",
    "## Wiring",
    "## Build and flash",
    "## Expected behavior",
    "## Troubleshooting",
    "## Acceptance checklist",
    "REF MUTABLE refresh",
    "Alt+W",
):
    assert expected in integration

screenshots = (
    "genre.png",
    "sequencer_hub.png",
    "drum_page_cyber.png",
    "synth_params.png",
    "pattern_edit.png",
    "song_page.png",
)
for name in screenshots:
    path = ROOT / "docs/screenshots" / name
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    width, height = struct.unpack(">II", data[16:24])
    assert (width, height) == (488, 275), (name, width, height)
    assert f"docs/screenshots/{name}" in readme

assert "docs/screenshots/groove_lab.png" not in readme

print("global help source regressions passed")
