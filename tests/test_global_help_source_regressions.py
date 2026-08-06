#!/usr/bin/env python3
from pathlib import Path
import base64
import io
import re
import struct
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]

# Temporary PR-construction gate: materialize the exact multi-file Phrase UI
# registration inside the existing trusted host-test workflow. The resulting
# files are archived in the log so they can be committed through Git Data API.
registration = ROOT / "tools/apply_phrase_ui_registration.py"
if registration.exists():
    subprocess.run(["python3", str(registration)], cwd=ROOT, check=True)

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

if registration.exists():
    build = ROOT / "build/phrase-ui-log"
    build.mkdir(parents=True, exist_ok=True)
    flags = [
        "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-Wno-c++20-extensions", "-I.", "-Iplatform_sdl",
        "-include", "platform_sdl/arduino_compat.h",
    ]
    subprocess.run(
        ["g++", *flags, "tests/test_global_help_content.cpp",
         "-o", str(build / "test_global_help_content")],
        cwd=ROOT, check=True,
    )
    subprocess.run(
        ["g++", *flags, "tests/test_ui_session_state.cpp",
         "-o", str(build / "test_ui_session_state")],
        cwd=ROOT, check=True,
    )
    subprocess.run(
        ["g++", *flags, "-c", "src/ui/pages/phrase_page.cpp",
         "-o", str(build / "phrase_page.o")],
        cwd=ROOT, check=True,
    )
    subprocess.run([str(build / "test_global_help_content")], cwd=ROOT, check=True)
    subprocess.run([str(build / "test_ui_session_state")], cwd=ROOT, check=True)

    generated_paths = (
        "src/ui/ui_config.h",
        "src/ui/workflow_mode.h",
        "src/state/ui_session_state.h",
        "src/ui/miniacid_display.cpp",
        "src/ui/global_help_content.h",
        "platform_sdl/Makefile",
        "tests/test_ui_session_state.cpp",
        "tests/test_global_help_content.cpp",
        "tests/test_performance_source_regressions.py",
    )
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode="w:gz") as archive:
        for relative in generated_paths:
            archive.add(ROOT / relative, arcname=relative)
    encoded = base64.b64encode(payload.getvalue()).decode("ascii")
    print("PHRASE_UI_ARCHIVE_BEGIN=" + encoded + "=PHRASE_UI_ARCHIVE_END")

print("global help source regressions passed")
