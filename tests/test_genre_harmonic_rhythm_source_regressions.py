#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
HEADER = ROOT / "src/generation/migration/strong_rhythm_migration.h"
PLANNER = ROOT / "src/generation/composition/genre_harmonic_rhythm.h"
LIVE = ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
GENRE_PAGE = ROOT / "src/ui/pages/genre_page.cpp"

migration = MIGRATION.read_text()
header = HEADER.read_text()
planner = PLANNER.read_text()
live = LIVE.read_text()
genre_page = GENRE_PAGE.read_text()

# Production Genre G must stay on the established live bridge. P2/P3 is
# integrated below that bridge, not exposed as another audition shortcut.
assert "regenerateWithStrongRhythmMigration" in genre_page
assert "migrateStrongRhythmMaterial" in live
assert "Ctrl+1" not in planner
assert "Ctrl+1" not in migration

# The normal migration composes the exact frozen P2/P3 APIs.
assert "realizeGenreHarmonicRhythm" in migration
assert "realizeChordRhythmTimeline" in planner
assert "realizeChordRhythmRetriggers" in planner

# Audible chord rhythm remains the existing ChordRhythm topology. The new
# masks classify those onsets; they do not manufacture sequencer events.
assert "harmonic.currentBar.audibleOnsets != chord.plan.onsets" in migration
assert "chordSourceAdvanceOnsets" in header
assert "chordSameChordRetriggers" in header
assert "request.chord.onsets & ~retriggerRequest.sourceAdvanceOnsets" in planner

# Legacy rhythm variation keeps the Stage15 semanticBarOrdinal policy. Harmonic
# phrase position is a separate transient coordinate so P2/P3 cannot silently
# perturb the established Bass/Chord/Melody rhythm skeleton.
assert "const bool useAddress =" in migration
assert "GenerativeMode::LoFi" in migration
assert "GenerativeMode::HipHop" in migration
assert "GenerativeMode::FunkSoul" in migration
assert "uint8_t harmonicBarOrdinal(int16_t patternAddress)" in migration
assert "harmonicRequest.barOrdinal = harmonicBarOrdinal(context.patternAddress)" in migration

# The rollback/default state is exactly the old one-bar harmonic clock. P2/P3
# replaces it only inside the explicit Stage15 tonal-materialization branch.
assert "StepMask harmonicEventOnsets = chord.plan.onsets" in migration
assert "uint8_t harmonicEventCount = onsetCount(chord.plan.onsets)" in migration
assert "uint8_t progressionPhraseBars = 1" in migration
assert "if (context.tonalMaterializationEnabled) {\n    GenreHarmonicRhythmRequest harmonicRequest" in migration
assert "harmonicEventOnsets = harmonic.currentBar.sourceAdvanceOnsets" in migration
assert "harmonicEventCount = harmonic.currentBar.sourceAdvanceCount" in migration
assert "progressionPhraseBars = harmonic.boundedPhraseBars" in migration
assert "progressionRequest.harmonicEventCount = harmonicEventCount" in migration
assert "progressionRequest.phraseBars = progressionPhraseBars" in migration

# TonalMaterializer consumes N, while FEEL and physical chord topology continue
# to consume all audible ChordRhythm onsets.
assert "harmonicEventOnsets, chord.plan.onsets, chord.plan.continuations" in migration
assert "RhythmRole::ChordRhythm, chord.plan.onsets, context.feelProfile" in migration

# The legacy rollback binding intentionally remains topology-compatible and
# does not pretend it can express semantic same-chord pitch reuse.
assert "Legacy redistribution remains a compatibility/rollback binding" in migration
assert "not execute P2/P3" in migration
assert "projectLegacyPitchPattern(\n          synthB, chord.plan.onsets, chord.plan.continuations" in migration

# Standalone Pattern generation must not fake incoming cross-bar ownership.
assert "retriggerRequest.sourceAvailableAtStart = false" in planner
assert "future Song phrase materializer" in planner

print("Genre harmonic rhythm production ownership: OK")
