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
keys = (ROOT / "src/ui/docs/keys.md").read_text()
release = (ROOT / "docs/releases/0_9_1_RELEASE.md").read_text()

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
    "kTexture", "kFeel", "kProject", "kGeneration",
    "kPerform", "kPlayer",
):
    assert f"WorkflowPages::{page_constant}" in help_content

# Runtime workflow truth: two GENERATE pages and four HUB pages. Persisted
# Generation/Texture and standalone SOUND ids are compatibility aliases only.
assert "case WorkflowMode::Generate: return 2;" in workflow
assert "case WorkflowMode::Hub: return 4;" in workflow
assert "if (page == kTexture || page == kGeneration) return kFeel;" in workflow
assert "if (page == kSynthAParameters) return kSynthA;" in workflow
assert "if (page == kSynthBParameters) return kSynthB;" in workflow
assert "kGenre, kFeel" in workflow
assert "kPattern, kSynthA, kSynthB, kDrums" in workflow

assert "Alt+H remains reserved for page-aware help" in smf
assert "Ctrl+H" not in smf
assert "drawDrumInputLockedFooter" in drum
assert '"ARROWS:GRID Q-I:PAT"' in drum and '"C1/2:BANK Alt[]:PAGE"' in drum
assert '"G:GEN Alt+G:ALL Q-I:PAT B:Bank"' in drum_legacy and '"DRUM Alt[]:PG"' in drum_legacy
assert '"REF         Mutable pattern references"' in help_content

for expected in (
    "# GroovePuter v0.9.1",
    "PERFORM:  MIDI KEYBOARD -> MIDI PLAYER",
    "GENERATE: GENRE -> FEEL",
    "HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS",
    "SONG:     SONG -> PHRASE CORE",
    "11 active pages",
    "NOTES -> KNOBS -> MORE",
    "P1 CANON -> P2 VAR -> P3 TRANS",
    "Generate fresh connected `1/2/4/8B` material at `TO:`",
    "Receiver `MONO/POLY`",
    "plain `Left/Right` to change route immediately",
    "REFERENCE VIEW / REF MUTABLE",
    "docs/releases/0_9_1_RELEASE.md",
):
    assert expected in readme

assert "GENRE -> FEEL -> GENERATION" not in readme
assert "three-page GENERATE" not in readme
assert "SYNTH A SOUND -> SYNTH B SOUND" not in readme

for expected in (
    "# GroovePuter 0.9.1 Key Map",
    "GENERATE: GENRE -> FEEL",
    "## GENRE 1/2",
    "## FEEL 2/2",
    "11 active pages",
    "Receiver `MONO/POLY`",
    "Velocity -10",
    "Generate fresh connected Phrase at `TO:`",
    "Route changes work during PLAY",
    "NOTES -> KNOBS -> MORE",
):
    assert expected in keys

assert "## GENERATION 3/3" not in keys
assert "## GENRE 1/3" not in keys
assert "## FEEL 2/3" not in keys
assert "PAUSE MIDI FIRST" not in keys

for expected in (
    "# GroovePuter 0.9.1 — Release Record",
    "170bbe1407daf37621949301a34a5ec345844b24",
    "4cd8244b091e748ddf93819a03c051d978e01266",
    "Runtime freeze",
    "Hardware acceptance",
    "Known deferred",
):
    assert expected in release

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
