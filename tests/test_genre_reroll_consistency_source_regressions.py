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

# A Synth Notes reroll is lane-scoped only at publication. Seed/composition
# identity must remain the exact same full-material tuple used by GENRE G.
# Using the physical synth bank/slot as phraseOrdinal changes
# resolveGenerationComposition() and can audibly sound like another genre.
prepare_synth = QUANTIZED.split("inline bool prepareSynthCandidate(", 1)[1]
prepare_synth = prepare_synth.split("}  // namespace QuantizedGenerationDetail", 1)[0]
require(
    prepare_synth,
    "StrongRhythmMigrationContext context = migrationContextFor(scene, target);",
    "synth reroll no longer inherits the full-material generation context",
)
if "synthPatternAddressFor(" in prepare_synth:
    raise AssertionError(
        "synth reroll must not replace full-material phraseOrdinal with a synth address"
    )

regen_synth = QUANTIZED.split(
    "inline QuantizedGenerationResult regenerateSynthWithQuantizedCommit(", 1
)[1]
regen_synth = regen_synth.split("inline bool hasPendingQuantizedGeneration(", 1)[0]
require(
    regen_synth,
    "if (!allocateAttemptFor(genre, requestLevel, target, attemptOrdinal))",
    "synth reroll must consume the same tuple-local attempt stream as GENRE G",
)
if "allocateSynthAttemptFor(" in regen_synth:
    raise AssertionError(
        "synth reroll must not create a second attempt domain from synth bank/slot"
    )
for forbidden in ("synthPatternAddressFor(", "allocateSynthAttemptFor("):
    if forbidden in QUANTIZED:
        raise AssertionError(f"retired synth-specific identity helper returned: {forbidden}")

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
