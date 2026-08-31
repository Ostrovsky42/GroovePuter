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
groove_lab = (ROOT / "docs/GROOVE_LAB.md").read_text()
release = (ROOT / "docs/releases/0_9_1_RELEASE.md").read_text()
integration = (
    ROOT / "docs/stages/INTEGRATED_GENERATE_PHRASE_ACCEPTANCE.md"
).read_text()

# Existing on-device help routing remains intact. The embedded string table itself is
# intentionally unchanged by this docs-only PR.
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
assert "setPageContext(page_index_)" in alt_h_handler.group(0)

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

# Runtime workflow truth: 12 active pages (PHW-P1 split PHRASE CORE out of
# PHRASE as its own separately-reachable Song-workflow page). Generation/
# Texture and standalone SOUND ids remain persisted compatibility aliases,
# not live pages.
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

# Canonical release-facing documents must identify the same current workflow and must
# not advertise the retired three-page Generate/Groove-Lab contract as live UI.
assert readme.startswith("# GroovePuter v0.9.1")
assert "GENERATE: GENRE -> FEEL" in readme
assert "SONG:     SONG -> PHRASE -> PHRASE CORE" in readme
assert "12 active pages" in readme
assert "docs/releases/0_9_1_RELEASE.md" in readme
assert "GENRE -> FEEL -> GENERATION" not in readme

assert manual.startswith("# GroovePuter 0.9.1 Manual")
assert "GENERATE: GENRE -> FEEL" in manual
assert "12 active pages" in manual
assert "GENERATION -> FEEL" in manual
assert "TEXTURE    -> FEEL" in manual
assert "Current `dev_0.9` Firmware" not in manual
assert "GENRE 1/3" not in manual
assert "GENERATION 3/3" not in manual

# 0.9.1 README/manual remain the frozen release record, while the canonical key map
# follows the active 0.9.2 hardening branch.
assert keys.startswith("# GroovePuter 0.9.2 Key Map")
assert "GENERATE: GENRE -> FEEL" in keys
assert "## GENRE 1/2" in keys
assert "## FEEL 2/2" in keys
assert "## PHRASE CORE" in keys
assert "## GENERATION 3/3" not in keys
assert "PAUSE MIDI FIRST" not in keys

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
