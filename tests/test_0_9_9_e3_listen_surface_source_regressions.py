#!/usr/bin/env python3
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
REALIZER_H = ROOT / "src/generation/rhythm/rhythm_realizer.h"
GENERATOR = ROOT / "tests/generate_0_9_9_e3_listen_fixture.py"
PREPARE = ROOT / "tests/prepare_0_9_9_e3_listen_sketch.py"
BUILD = ROOT / "scripts/build_e3_listen.sh"
HOOK_H = ROOT / "tests/e3_listen_overlay/e3_listen_review_hook.h"
HOOK_CPP = ROOT / "tests/e3_listen_overlay/e3_listen_review_hook.cpp"
PLAYER = ROOT / "tests/e3_listen_overlay/e3_listen_fixture_player.cpp"
PAGE = ROOT / "tests/e3_listen_overlay/e3_listen_page.cpp"
CARDPUTER_SKETCH = ROOT / "GroovePuter.ino"

migration = MIGRATION.read_text(encoding="utf-8")
realizer_h = REALIZER_H.read_text(encoding="utf-8")
generator = GENERATOR.read_text(encoding="utf-8")
prepare = PREPARE.read_text(encoding="utf-8")
build = BUILD.read_text(encoding="utf-8")
hook_h = HOOK_H.read_text(encoding="utf-8")
hook_cpp = HOOK_CPP.read_text(encoding="utf-8")
player = PLAYER.read_text(encoding="utf-8")
page = PAGE.read_text(encoding="utf-8")
cardputer_sketch = CARDPUTER_SKETCH.read_text(encoding="utf-8")

# E3a stays the only DROP/DISPLACE executor in tracked production source.
assert "applyRhythmMutationDelta(" in realizer_h
assert "e3ListenOverrideComposition" not in migration
assert "e3ListenOverrideRhythmPlan" not in migration
assert "e3ListenOverrideBassPlan" not in migration
assert "e3_listen_review_hook.h" not in migration

# E3L may reproduce frozen C/V/W plans but must never grow a second mutation
# executor or call the E3a executor to derive W at runtime.
new_review_sources = "\n".join(
    (generator, prepare, hook_h, hook_cpp, player, page)
)
for forbidden in (
    "applyDrop(",
    "applyDisplace(",
    "moveOnset(",
    "removeOnset(",
    "applyRhythmMutationDelta(",
):
    assert forbidden not in new_review_sources, forbidden

# Frozen cap=1 provenance is fail-closed and includes all four authority hashes.
for digest in (
    "9c9d3983f456e8ef3ffe19422c08cba0dbaafcb9470ab4f1d33d9b6482b98ed1",
    "bbad8865638cc3dc620680b806cd4a6c00da13fb1f335383b6be0d569356abe5",
    "6216accb1d399dfe8d909646980b0cfee04fcb63d2dd4a88535cc52a6217fd7d",
    "edf2b8c0bf2bec8944648870be156fa237243a24fe0f8fa7fb6d6dc985f23ecb",
):
    assert digest in generator
for needle in (
    'if len(rows) != 32:',
    'Counter({"DROP": 12, "DISPLACE": 12, "COMBINED": 8})',
    'expected_roles = Counter({"ClosedHat": 15, "BassRhythm": 15, "Kick": 2})',
    '"ChordRhythm", "MelodicRhythm"',
    "no approved E3 LISTEN physical boundary exists",
):
    assert needle in generator

# The BassRhythm seam is deliberately after the existing rhythm owner and
# before existing pitch/tonal/Synth A ownership. No note/MIDI renderer exists
# in the hook, and the production profile keeps ownership of secondaryRole.
for needle in (
    "void e3ListenOverrideBassPlan(BassRhythmPlan& plan)",
    "plan.onsets = allOnsets(source);",
    "plan.continuations = 0;",
    "PRODUCTION_CONTEXT_AUDITION",
):
    assert needle in hook_cpp
for forbidden in (
    "SynthPattern",
    "Midi",
    "MIDI",
    "note =",
    "adaptTonalPlanToSynthPattern",
    "materializeTonalIntent",
    "composition.secondaryRole =",
):
    assert forbidden not in hook_cpp

# The frozen complete RhythmPhrasePlan is copied verbatim as masks. There is no
# operation switch here: C/V/W came from the frozen E3R-B corpus.
for needle in (
    "target.structural = source.structural;",
    "target.secondary = source.secondary;",
    "target.ghosts = source.ghosts;",
    "target.shortGate = source.shortGate;",
    "target.heldGate = source.heldGate;",
    "target.tieGate = source.tieGate;",
    "target.accents = source.accents;",
):
    assert needle in hook_cpp
assert "RhythmMutationOp::DROP" not in hook_cpp
assert "RhythmMutationOp::DISPLACE" not in hook_cpp

# Playback uses the normal migration/playback stack. No fixed-note substitute
# is allowed even as a review convenience; tonal materialization starts from
# empty SynthPattern storage and remains production-owned.
assert "reviewPitchSource" not in player
assert "event.note =" not in player
assert "SynthPattern synthA{};" in player
assert "SynthPattern synthB{};" in player

# Playback uses deterministic fixed review context, four repeated rows, and
# reserved Bank B / Pattern 1 / Song B.
for needle in (
    "constexpr int kReviewBank = 1;",
    "constexpr int kReviewPattern = 0;",
    "constexpr int kReviewSongSlot = 1;",
    "constexpr uint8_t kReviewBars = 4;",
    "constexpr float kReviewBpm = 124.0f;",
    "migrateStrongRhythmMaterial(",
    "engine.setSongLength(kReviewBars);",
    "engine.setSongPosition(0);",
    "engine.setLoopRange(0, kReviewBars - 1);",
    "engine.start();",
):
    assert needle in player

# Hardware review is complete without Serial: case navigation, C/V/W, replay,
# transport and isolation are all page-owned keys.
for needle in (
    "GROOVEPUTER_LEFT",
    "GROOVEPUTER_RIGHT",
    "GROOVEPUTER_UP",
    "GROOVEPUTER_DOWN",
    "E3ListenVariant::Canonical",
    "E3ListenVariant::Before",
    "E3ListenVariant::After",
    "replay();",
    "togglePlaying();",
    "isolated_ = !isolated_;",
    '"FULL MIX"',
):
    assert needle in page
assert '"CTX AUDITION"' in player

# Ctrl+V deliberately uses the established hardware Ctrl+letter path.
for needle in (
    "auto applyCtrlLetter = []",
    "if (!ks.ctrl || hid < HID_KEY_A || hid > HID_KEY_Z) return false;",
    "} else if (applyCtrlLetter(ks, hid, evt)) {",
):
    assert needle in cardputer_sketch

# Review writes must not ask normal scene/session persistence to save the
# destructive sandbox.
for forbidden in (
    "markSceneMutated",
    "saveScene",
    "autoSave",
):
    assert forbidden not in player
    assert forbidden not in hook_cpp

# Build is staged and therefore cannot mutate tracked production src.
for needle in (
    "generate_0_9_9_e3_listen_fixture.py",
    "prepare_0_9_9_e3_listen_sketch.py",
    "build/cardputer-adv-e3-listen",
    "rsync -a --delete",
):
    assert needle in build

# Exercise every staging anchor against this exact source without touching the
# checked-out production tree.
with tempfile.TemporaryDirectory(prefix="e3-listen-source-test-") as temp_name:
    staged = Path(temp_name) / "GroovePuter"
    for relative in (
        "src/ui/ui_config.h",
        "src/ui/workflow_mode.h",
        "src/ui/miniacid_display.cpp",
        "src/generation/migration/strong_rhythm_migration.cpp",
    ):
        source = ROOT / relative
        destination = staged / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    fixture = Path(temp_name) / "e3_listen_fixture_generated.h"
    fixture.write_text(
        "#pragma once\n"
        "namespace GroovePuterRhythm { namespace E3ListenFixtureData {} }\n",
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

    staged_config = (
        staged / "src/ui/ui_config.h"
    ).read_text(encoding="utf-8")
    staged_workflow = (
        staged / "src/ui/workflow_mode.h"
    ).read_text(encoding="utf-8")
    staged_display = (
        staged / "src/ui/miniacid_display.cpp"
    ).read_text(encoding="utf-8")
    staged_migration = (
        staged / "src/generation/migration/strong_rhythm_migration.cpp"
    ).read_text(encoding="utf-8")

    assert "kPageCount = 17" in staged_config
    assert "kE3Listen = 16" in staged_workflow
    assert 'case kE3Listen: return "E3 LISTEN";' in staged_workflow
    assert '#include "pages/e3_listen_page.h"' in staged_display
    assert "std::make_unique<E3ListenPage>" in staged_display
    assert "page_index_ == WorkflowPages::kE3Listen) return;" in staged_display
    assert "index != WorkflowPages::kE3Listen" in staged_display
    assert "event.ctrl && !event.meta" in staged_display
    assert "(event.key == 'v' || event.key == 'V')" in staged_display

    for needle in (
        '#include "e3_listen_review_hook.h"',
        "e3ListenOverrideComposition(composition);",
        "e3ListenOverrideRhythmPlan(realization.plan);",
        "e3ListenOverrideBassPlan(bass.plan);",
    ):
        assert needle in staged_migration

    bass_realize = staged_migration.index(
        "BassRhythmResult bass = realizeBassRhythm(bassRequest);"
    )
    bass_override = staged_migration.index(
        "e3ListenOverrideBassPlan(bass.plan);"
    )
    pitch_realize = staged_migration.index(
        "bassPitch = realizeBassPitchBehavior(pitchRequest);"
    )
    assert bass_realize < bass_override < pitch_realize

    assert (
        staged / "src/ui/pages/e3_listen_page.cpp"
    ).is_file()
    assert (
        staged / "src/generation/migration/e3_listen_review_hook.cpp"
    ).is_file()
    assert (
        staged / "src/generation/migration/e3_listen_fixture_player.cpp"
    ).is_file()
    assert (
        staged / "src/generation/migration/e3_listen_fixture_generated.h"
    ).is_file()

print("0.9.9-E3L disposable LISTEN source regressions: OK")
