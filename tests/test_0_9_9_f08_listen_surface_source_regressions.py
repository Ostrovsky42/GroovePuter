#!/usr/bin/env python3
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
GENERATOR = ROOT / "tests/generate_0_9_9_f08_listen_fixture.py"
DUMP = ROOT / "tests/dump_0_9_9_f08_listen_fixture.cpp"
PREPARE = ROOT / "tests/prepare_0_9_9_f08_listen_sketch.py"
BUILD = ROOT / "scripts/build_f08_listen.sh"
PAGE = ROOT / "tests/f08_listen_overlay/f08_listen_page.cpp"
PLAYER = ROOT / "tests/f08_listen_overlay/f08_listen_fixture_player.cpp"
CARDPUTER_SKETCH = ROOT / "GroovePuter.ino"

migration = MIGRATION.read_text(encoding="utf-8")
generator = GENERATOR.read_text(encoding="utf-8")
dump = DUMP.read_text(encoding="utf-8")
prepare = PREPARE.read_text(encoding="utf-8")
build = BUILD.read_text(encoding="utf-8")
page = PAGE.read_text(encoding="utf-8")
player = PLAYER.read_text(encoding="utf-8")
cardputer_sketch = CARDPUTER_SKETCH.read_text(encoding="utf-8")

# The committed production F08 owner stays independent. OLD exists only as an
# exact temporary reverse patch in the fixture generator.
assert "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" not in migration
assert "progressionRequest.harmonicEventCount = harmonic.plan.eventCount;" in migration
assert "HarmonicRhythmRequest harmonicRequest" in migration
assert "reverse_f08_source" in generator
assert "expected five F08 tonal harmonic-clock consumers" in generator
assert 'source.replace("harmonic.plan.onsets", "chord.plan.onsets")' in generator

cases = (
    ("DrumAndBass", 5, "B", "MINOR FALL", "0000", "8080"),
    ("TripHop", 4, "A", "II-V-I", "0000", "8080"),
    ("House", 4, "A", "POP CYCLE", "0000", "8080"),
    ("House", 5, "B", "POP CYCLE", "4904", "8080"),
    ("Outrun", 0, "B", "POP CYCLE", "2448", "8080"),
    ("UkGarage", 1, "B", "BORROWED LIFT", "0101", "8080"),
    ("FunkSoul", 6, "B", "BORROWED LIFT", "0802", "8080"),
    ("TripHop", 2, "B", "PARALLEL SHIFT", "0902", "8080"),
    ("Acid", 2, "B", "STATIC MODAL", None, None),
    ("Techno", 4, "B", "PEDAL DRONE", None, None),
    ("Reggae", 4, "B", "BORROWED LIFT", "0202", "8080"),
)
for mode, ordinal, voice, progression, old_clock, new_clock in cases:
    for needle in (f'"{mode}"', str(ordinal), f'"{voice}"', f'"{progression}"'):
        assert needle in generator
    if old_clock:
        assert f'"{old_clock}"' in generator
    if new_clock:
        assert f'"{new_clock}"' in generator

assert dump.count("{GenerativeMode::") == 11
for needle in (
    "--legacy",
    "--independent",
    "result.chordOnsets",
    "result.harmonicEventOnsets",
    "generationProfileFor(settings)",
):
    assert needle in dump

# The UI has exactly the requested listening controls and no generator call.
for needle in (
    "GROOVEPUTER_LEFT",
    "GROOVEPUTER_RIGHT",
    "F08ListenVariant::Old",
    "F08ListenVariant::New",
    "replay();",
    '"TEST ONLY',
    '"A progression   B movement natural"',
    '"C no-step8      D roles coherent"',
    '"G:REPLAY CTRL+F:EXIT"',
):
    assert needle in page
for forbidden in (
    "migrateStrongRhythmMaterial(",
    "realizeHarmonicRhythm(",
    "realizeChordProgression(",
):
    assert forbidden not in page
    assert forbidden not in player

# Review playback is confined to Bank B pattern 1 / Song B and uses frozen data.
for needle in (
    "constexpr int kReviewBank = 1;",
    "constexpr int kReviewPattern = 0;",
    "constexpr int kReviewSongSlot = 1;",
    "f08_listen_fixture_generated.h",
    "engine.setSongLength(1);",
    "engine.setLoopRange(0, 0);",
):
    assert needle in player

for needle in (
    "generate_0_9_9_f08_listen_fixture.py",
    "prepare_0_9_9_f08_listen_sketch.py",
    "build/cardputer-adv-f08-listen",
):
    assert needle in build

# Ctrl+F is intentionally chosen because the real Cardputer input layer has an
# explicit Ctrl+letter HID route. The discarded Ctrl+Alt+F chord had no hardware
# acceptance coverage and proved unreliable on-device.
for needle in (
    "auto applyCtrlLetter = []",
    "if (!ks.ctrl || hid < HID_KEY_A || hid > HID_KEY_Z) return false;",
    "} else if (applyCtrlLetter(ks, hid, evt)) {",
):
    assert needle in cardputer_sketch

# Exercise the staging patch itself without changing the checked-out src tree.
with tempfile.TemporaryDirectory(prefix="f08-listen-source-test-") as temp_name:
    staged = Path(temp_name) / "GroovePuter"
    for relative in (
        "src/ui/ui_config.h",
        "src/ui/workflow_mode.h",
        "src/ui/miniacid_display.cpp",
    ):
        source = ROOT / relative
        destination = staged / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    fixture = Path(temp_name) / "f08_listen_fixture_generated.h"
    fixture.write_text(
        "#pragma once\nnamespace GroovePuterRhythm { namespace F08ListenFixtureData {} }\n",
        encoding="utf-8",
    )
    subprocess.run(
        [
            "python3",
            str(PREPARE),
            "--root",
            str(staged),
            "--fixture",
            str(fixture),
        ],
        check=True,
    )

    staged_config = (staged / "src/ui/ui_config.h").read_text(encoding="utf-8")
    staged_workflow = (staged / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
    staged_display = (staged / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    assert "kPageCount = 17" in staged_config
    assert "kF08Listen = 16" in staged_workflow
    assert 'case kF08Listen: return "F08 LISTEN";' in staged_workflow
    assert '#include "pages/f08_listen_page.h"' in staged_display
    assert "std::make_unique<F08ListenPage>" in staged_display
    assert "page_index_ == WorkflowPages::kF08Listen) return;" in staged_display
    assert "index != WorkflowPages::kF08Listen" in staged_display
    assert "event.ctrl && !event.meta" in staged_display
    assert "event.ctrl && event.alt && !event.meta" not in staged_display
    assert "display Ctrl+F shortcut" not in staged_display
    assert (staged / "src/ui/pages/f08_listen_page.cpp").is_file()
    assert (staged / "src/generation/migration/f08_listen_fixture_player.cpp").is_file()
    assert (staged / "src/generation/migration/f08_listen_fixture_generated.h").is_file()

print("0.9.9-F08 test-only LISTEN surface source regressions: OK")
