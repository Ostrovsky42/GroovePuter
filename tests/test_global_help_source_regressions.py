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
readme = (ROOT / "README.md").read_text()
keys = (ROOT / "src/ui/docs/keys.md").read_text()

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
    "kFeelTexture", "kGenerator", "kProject", "kMode",
    "kPerform", "kPlayer",
):
    assert f"WorkflowPages::{page_constant}" in help_content

assert "Alt+H remains reserved for page-aware help" in smf
assert "Ctrl+H" not in smf
assert '"G:GEN Alt+G:ALL 1..8:Edit B:Bank"' in drum
assert '"C           Edit saved per-file route"' in help_content
assert '"f:GEN Alt+G:ALL 1..8:Edit B:Bank"' not in drum
assert '"1..4        Select Phrase A/B/C/D"' in help_content
assert '"Enter       Capture current Song row"' in help_content
assert '"REF         Mutable pattern references"' in help_content

for expected in (
    "| `Alt+H` | Open page-aware help",
    "| `Fn+M` | Open the workspace launcher |",
    "| `Alt+X` | Toggle LiveMix |",
    "| `Alt+M` | Toggle Song mode |",
    "same primary controls shown by `Alt+H`",
    "**OVERVIEW / SEQUENCER HUB**",
    "**SYNTH A PATTERN**",
    "**MODE / FLAVOR**",
    "route profiles restore `AUTO` / `CH1..CH10` assignments across reloads and reboots",
):
    assert expected in readme

assert "`Alt+H` is the on-device" in keys
assert "`Ctrl+H`" not in keys
assert "Generate material into a free slot" in keys
assert "Digits remain available to the global mute fallback" in keys
assert "persisted per file and restored across reloads and reboots" in keys

screenshots = (
    "genre.png",
    "sequencer_hub.png",
    "drum_page_cyber.png",
    "synth_params.png",
    "pattern_edit.png",
    "song_page.png",
    "groove_lab.png",
)
for name in screenshots:
    path = ROOT / "docs/screenshots" / name
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    width, height = struct.unpack(">II", data[16:24])
    assert (width, height) == (488, 275), (name, width, height)
    assert f"docs/screenshots/{name}" in readme

print("global help source regressions passed")
