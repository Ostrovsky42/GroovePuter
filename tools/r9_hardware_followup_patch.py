from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one replacement target, found {count}")
    p.write_text(text.replace(old, new, 1))


# 1) DRUMS: Backspace on one visible grid cell must use the same bounded
# PREPARE -> canonical COMMIT path as Enter/tap toggles. This preserves the
# exact before-state and turns an already-empty cell into a true no-op.
drum_old = '''        if (!patternRowFocused() && !bankRowFocused()) {
            withAudioGuard([&]() { mini_acid_.sceneManager().setDrumStep(voice, step, false, false); });
            return true;
        }
'''
drum_new = '''        if (!patternRowFocused() && !bankRowFocused()) {
            commitDrumPatternMutation([&](DrumPatternSet& pattern) {
              pattern.voices[voice].steps[step].hit = false;
              pattern.voices[voice].steps[step].accent = false;
            });
            return true;
        }
'''
replace_once("src/ui/pages/drum_sequencer_page_legacy.h", drum_old, drum_new)


# 2) SYNTH NOTES G: preserve the exact legacy STOP-state musical generator,
# but run it against a local candidate before the bounded Pattern COMMIT. The
# commit callback itself remains an infallible fixed assignment; generation is
# never executed inside UndoOwner::commitPrepared(). PLAY keeps the historical
# direct path and remains a 0.9.9 activation/ownership concern.
synth_old = '''  // Generation is intentionally not converted into a Pattern receipt in R3.
  // It is not a bounded pure prepare operation and is part of the 0.9.9
  // PREPARE/ACTIVATE handoff. Preserve its old mutation path but still advance
  // the persistent revision so any older Pattern Undo expires safely.
  if (keyG) {
    const bool handled = handleEventLegacyUnowned(ui_event);
    if (handled) GroovePuterState::markSceneMutated();
    return handled;
  }
'''
synth_new = '''  // STOP-state Synth G can be prepared without touching Scene state: mirror
  // MiniAcid::randomize303Pattern() into a local candidate, then publish one
  // bounded Pattern receipt and perform only a fixed assignment in COMMIT.
  // PLAY retains the legacy direct-generation path; quantized activation and
  // its reversible ownership remain a 0.9.9 TIME concern.
  if (keyG) {
    if (mini_acid_.isPlaying()) {
      const bool handled = handleEventLegacyUnowned(ui_event);
      if (handled) GroovePuterState::markSceneMutated();
      return handled;
    }

    SynthPattern generated =
        mini_acid_.sceneManager().getCurrentSynthPattern(voice_index_);
    const GenerativeParams& genreParams =
        mini_acid_.genreManager().getCompiledGenerativeParams();
    auto behavior = mini_acid_.genreManager().getBehavior();
    if (mini_acid_.genreManager().generativeMode() == GenerativeMode::Reggae) {
      // Keep MiniAcid::randomize303Pattern()'s exact Reggae split so adding
      // Undo cannot change the musical result.
      if (voice_index_ == 0) {
        behavior.stepMask = 0x1111;
        behavior.motifLength = 2;
        behavior.avoidClusters = true;
        behavior.forceOctaveJump = false;
      } else {
        behavior.stepMask = 0xAAAA;
        behavior.motifLength = 4;
        behavior.avoidClusters = false;
        behavior.forceOctaveJump = false;
      }
    }
    mini_acid_.modeManager().generatePattern(
        generated, mini_acid_.bpm(), genreParams, behavior, voice_index_);

    const PatternMutationResult result = commitPatternMutation(
        [&](SynthPattern& pattern) { pattern = generated; });
    return result != PatternMutationResult::Invalid;
  }
'''
replace_once("src/ui/pages/pattern_edit_page.cpp", synth_old, synth_new)


# Permanent source regression: connects the physical UI paths to the existing
# one-slot host/owner tests without introducing a second Undo implementation.
test_path = Path("tests/test_undo_0_9_8_r9_hardware_followup_source.py")
test_path.write_text(r'''#!/usr/bin/env python3
from pathlib import Path


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle!r}")


drum = Path("src/ui/pages/drum_sequencer_page_legacy.h").read_text()
synth = Path("src/ui/pages/pattern_edit_page.cpp").read_text()
owner = Path("src/state/undo_owner.h").read_text()

# DRUM single-cell Backspace: no direct write; before-state/no-op handling is
# delegated to the same canonical helper already used by normal cell toggles.
assert "sceneManager().setDrumStep(voice, step, false, false)" not in drum
require(
    drum,
    "commitDrumPatternMutation([&](DrumPatternSet& pattern) {\n"
    "              pattern.voices[voice].steps[step].hit = false;\n"
    "              pattern.voices[voice].steps[step].accent = false;",
    "single-cell Drum Backspace canonical commit",
)
require(drum, "if (GroovePuterUndo::sameDrumPattern(before.before, after)) return false;",
        "Drum no-op guard")

# Synth G at STOP: generation happens on a local candidate before COMMIT; the
# canonical Pattern receipt therefore retains the exact previous pattern.
require(synth, "if (mini_acid_.isPlaying()) {", "PLAY/STOP boundary")
require(synth, "SynthPattern generated =\n        mini_acid_.sceneManager().getCurrentSynthPattern(voice_index_);",
        "Synth before-state candidate")
require(synth, "mini_acid_.modeManager().generatePattern(\n        generated, mini_acid_.bpm(), genreParams, behavior, voice_index_);",
        "legacy-equivalent Synth generator")
require(synth, "const PatternMutationResult result = commitPatternMutation(\n        [&](SynthPattern& pattern) { pattern = generated; });",
        "Synth generated Pattern canonical commit")

# Guard musical equivalence with MiniAcid::randomize303Pattern's established
# Reggae voice split.
for needle in (
    "behavior.stepMask = 0x1111;",
    "behavior.motifLength = 2;",
    "behavior.stepMask = 0xAAAA;",
    "behavior.motifLength = 4;",
):
    require(synth, needle, "Reggae generation invariant")

# This follow-up must not silently move Synth G onto the 0.9.9 quantized/pending
# activation API.
assert "regenerateSynthWithQuantizedCommit" not in synth

# The core owner contract continues to forbid doing generation inside COMMIT.
require(owner, "it must not perform generation", "bounded COMMIT generation prohibition")

print("R9 hardware follow-up source regressions: PASS")
''')

runner = Path("tests/run_undo_0_9_8_r9_tests.sh")
runner_text = runner.read_text()
runner_old = "python3 tests/test_undo_0_9_8_r9_source.py\n"
runner_new = (
    "python3 tests/test_undo_0_9_8_r9_source.py\n"
    "python3 tests/test_undo_0_9_8_r9_hardware_followup_source.py\n"
)
if runner_text.count(runner_old) != 1:
    raise SystemExit("R9 runner: expected source-test insertion point exactly once")
runner.write_text(runner_text.replace(runner_old, runner_new, 1))

print("R9 hardware follow-up patch applied")
