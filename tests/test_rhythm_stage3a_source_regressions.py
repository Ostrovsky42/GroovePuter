#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


ino = read("GroovePuter.ino")
cardputer = read("src/generation/audition/rhythm_audition_cardputer.h")
session = read("src/generation/audition/rhythm_audition_session.cpp")
controller = read("src/generation/audition/rhythm_audition_controller.cpp")
materializer = read("src/generation/audition/rhythm_audition_materializer.cpp")

assert '#include "src/generation/audition/rhythm_audition_cardputer.h"' in ino
hook = "GroovePuterRhythm::Audition::handleCardputerAuditionEvent("
display = "g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false"
assert hook in ino
assert display in ino
assert ino.index(hook) < ino.index(display)

# The hook executes inside the already-established control-plane audio gate.
hook_window = ino[max(0, ino.index(hook) - 300): ino.index(hook) + 300]
assert "AudioMutationScope mutationScope(g_audioMutationGate);" in hook_window

# Existing Alt+A accent must remain untouched until the explicit test mode is
# armed with Ctrl+Alt+A.
assert "const bool toggleChord = evt.ctrl && (key == 'a' || key == 'A');" in cardputer
assert "if (!session.active() && !toggleChord) return false;" in cardputer

for forbidden in (
    "saveScene",
    "saveSceneAs",
    "autoSaveSceneRecovery",
    "setSongMode",
    "toggleSongMode",
    "setBpm(",
    "regeneratePatternsWithGenre",
    "syncGrooveModeToGenre",
):
    assert forbidden not in cardputer
    assert forbidden not in session
    assert forbidden not in controller

# Stage 3A tests topology. It must not turn rhythmic gate intent into TB303
# articulation or invent pitch movement.
assert "target.slide = false;" in materializer
assert "target.accent = false;" in materializer
assert "options.bassNote = 36;" in controller
assert "RhythmRole::ChordRhythm" not in materializer
assert "RhythmRole::MelodicRhythm" not in materializer

# Backups are fixed-capacity values and deactivation restores them exactly.
assert "backupDrums_ = drums;" in session
assert "backupA_ = synthA;" in session
assert "backupB_ = synthB;" in session
assert "drums = backupDrums_;" in session
assert "synthA = backupA_;" in session
assert "synthB = backupB_;" in session

for source in (session, controller, materializer):
    assert "std::vector" not in source
    assert "std::string" not in source
    assert "malloc(" not in source
    assert "calloc(" not in source
    assert "realloc(" not in source

print("Groove Vocabulary Stage 3A source regressions: OK")
