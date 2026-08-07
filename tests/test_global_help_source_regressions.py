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
readme = (ROOT / "README.md").read_text()
keys = (ROOT / "src/ui/docs/keys.md").read_text()
integration = (
    ROOT / "docs/stages/INTEGRATED_GENERATE_PHRASE_ACCEPTANCE.md"
).read_text()

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

assert "Alt+H remains reserved for page-aware help" in smf
assert "Ctrl+H" not in smf
assert "drawDrumInputLockedFooter" in drum
assert '"ARROWS:GRID Q-I:PAT"' in drum and '"C1/2:BANK Alt[]:PAGE"' in drum
assert '"G:GEN Alt+G:ALL Q-I:PAT B:Bank"' in drum_legacy and '"DRUM Alt[]:PG"' in drum_legacy
assert '"C           Edit saved per-file route"' in help_content
assert '"f:GEN Alt+G:ALL Q-I:PAT B:Bank"' not in drum_legacy
assert '"G:GEN Alt+G:ALL 1..8:Edit B:Bank"' not in drum_legacy
assert '"REF         Mutable pattern references"' in help_content

for expected in (
    "| `Alt+H` | Open page-aware help",
    "| `Fn+M` | Open the workspace launcher |",
    "| `Alt+X` | Toggle LiveMix |",
    "| `Alt+M` | Toggle Song mode |",
    "same primary controls shown by `Alt+H`",
    "GENRE -> FEEL -> GENERATION",
    "SONG:     SONG -> PHRASE CORE",
    "**OVERVIEW / SEQUENCER HUB**",
    "**SYNTH A PATTERN**",
    "route profiles restore `AUTO` / `CH1..CH10` assignments across reloads and reboots",
    "REFERENCE VIEW / REF MUTABLE",
):
    assert expected in readme

assert "GENRE != FEEL != GENERATION" in readme
assert "GENRE != FEEL != GENERATION != TEXTURE" not in readme
assert "GENRE -> FEEL -> GENERATION -> TEXTURE" not in readme
assert "**MODE / FLAVOR**" not in readme

assert "`Alt+H` is the on-device" in keys
assert "`Ctrl+H`" not in keys
assert "Generate material into a free slot" in keys
assert "Digits remain available to the global mute fallback" in keys
assert "## PHRASE CORE" in keys
assert "GENRE -> FEEL -> GENERATION" in keys
assert "GENRE -> FEEL -> GENERATION -> TEXTURE" not in keys
assert "## GENRE 1/3" in keys
assert "## FEEL 2/3" in keys
assert "## GENERATION 3/3" in keys
assert "Select texture field" not in keys
assert "Apply texture" not in keys
assert "## FEEL / TEXTURE" not in keys
assert "## MODE / FLAVOR" not in keys
assert "## ADV GENERATOR" not in keys

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
