from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
DRUMS = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
SYNTH = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
QUANTIZED = (
    ROOT / "src/generation/migration/quantized_generation_commit_impl.h"
).read_text(encoding="utf-8")
MIGRATION = (
    ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
).read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# GENRE owns full A+B+Drums reroll. SYNTH owns a Genre-aware single-voice
# reroll. DRUMS keeps its established drums-only strong generation command.
require(GENRE, "regenerateWithQuantizedCommit(",
        "GENRE G does not use the full action")
require(SYNTH, "regenerateSynthWithQuantizedCommit(",
        "SYNTH G does not use the Genre-aware synth action")
require(DRUMS, "regenerateDrumsWithStrongRhythmMigration(",
        "DRUMS G lost drums-only ownership")
for forbidden in ("randomize303Pattern", "randomizeDrumPattern"):
    if forbidden in GENRE:
        raise AssertionError(f"GENRE regained a second materializer: {forbidden}")
if SYNTH.count("regenerateSynthWithQuantizedCommit(") != 1:
    raise AssertionError("synth G must materialize exactly once")

# Direct note entry deliberately owns the letter G as a note. Outside that
# mode, SYNTH G must be intercepted before the retained legacy randomizer.
note_entry = SYNTH.index("if (note_entry_mode_ && !ui_event.ctrl")
full_reroll = SYNTH.index("if (!note_entry_mode_ && keyG &&")
legacy_fallthrough = SYNTH.index("return handleEventLegacy(ui_event);", full_reroll)
if not note_entry < full_reroll < legacy_fallthrough:
    raise AssertionError("SYNTH G ownership order changed")

# GENRE pending state and active Scene state use the same full request. SYNTH
# routes the active context through its separate quantized single-voice owner.
for needle in (
    "mini_acid_, requestedSettings, nextMode, doApplyTempo, requestedBpm",
    "regenerateSynthWithQuantizedCommit(",
    "if (mini_acid_.isPlaying())",
    "withAudioGuard(generate);",
):
    require(GENRE + DRUMS + SYNTH, needle,
            f"unified request contract missing: {needle}")

# Non-zero full-material rerolls receive a deterministic bounded articulation
# after realization. It cannot alter attempt zero or selection/composition IDs.
for needle in (
    "applyFullMaterialRerollArticulation(",
    "if (attemptOrdinal == 0) return;",
    "event.accent = !event.accent;",
    "context.generationAttemptOrdinal, nextDrums",
):
    require(MIGRATION, needle, f"repeated-G variation contract missing: {needle}")

for needle in (
    "migrateStrongRhythmSynths(",
    "if (!replaceDrums) nextDrums = drums;",
    "applySynthRerollArticulation(",
):
    require(MIGRATION, needle, f"synth-only Genre contract missing: {needle}")

for needle in (
    "QuantizedGenerationScope::SynthA",
    "QuantizedGenerationScope::SynthB",
    "prepareSynthCandidate(",
    "migrateStrongRhythmSynths(",
    "bank.patterns[pending.target.synthSlot[voice]]",
):
    require(QUANTIZED, needle, f"quantized synth-only ownership missing: {needle}")

# PLAY still publishes only the complete transaction at BAR_START.
commit = QUANTIZED.split("inline bool commitQuantizedGenerationAtBarStart(", 1)[1]
commit = commit.split("inline QuantizedGenerationResult regenerateWithQuantizedCommit(", 1)[0]
for needle in (
    ".synthABanks[",
    ".synthBBanks[",
    ".drumBanks[",
    "g_commitSerial.fetch_add",
):
    require(commit, needle, f"atomic BAR_START commit lost: {needle}")

print("Genre reroll consistency source regressions: OK")
