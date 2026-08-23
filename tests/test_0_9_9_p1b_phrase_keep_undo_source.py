#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

bounded = (ROOT / "src/state/bounded_undo_slot.h").read_text(encoding="utf-8")
owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
receipts = (ROOT / "src/state/undo_receipts.h").read_text(encoding="utf-8")
materializer = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")
lease = (ROOT / "src/phrase/pattern_lease_owner.h").read_text(encoding="utf-8")
backing = (ROOT / "src/phrase/phrase_undo_backing.h").read_text(encoding="utf-8")
keep = (ROOT / "src/phrase/phrase_keep.h").read_text(encoding="utf-8")
phrase_page_h = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")
phrase_page_cpp = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
phrase_persistence = (ROOT / "src/phrase/phrase_persistence.h").read_text(encoding="utf-8")
scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
paging = (ROOT / "src/audio/pattern_paging.cpp").read_text(encoding="utf-8")
test = (ROOT / "tests/test_0_9_9_p1b_phrase_keep_undo.cpp").read_text(encoding="utf-8")

# Canonical one-slot owner remains the only history owner. Lifecycle metadata
# lives inside the already-resident payload storage rather than beside it.
for token in (
    "constexpr std::size_t kUndoPayloadBytes = 1536;",
    "constexpr std::size_t kUndoLifecycleTailBytes = 112;",
    "constexpr uint8_t kUndoRetainedResourceCapacity = 20;",
    "UndoRetainedResourceKind::PatternBacking",
    "publishWithLifecycle",
    "lifecycleStorage()",
    "return PayloadBytes - kUndoLifecycleTailBytes;",
    "commitPreparedWithLifecycle",
    "retainsPatternBacking",
    "sanitizeForPersistence",
    "togglePrepared",
):
    assert token in bounded + owner, f"missing P1b canonical Undo token: {token}"

assert owner.count("BoundedUndoSlot<kUndoPayloadBytes> slot_") == 1
assert "std::vector" not in bounded
assert "std::vector" not in owner
assert "std::unique_ptr" not in bounded
assert "std::unique_ptr" not in owner

# Phrase Undo is still a PhraseBank receipt, not a Scene or physical material
# snapshot. Model B retains addresses/masks only.
assert "PhraseCore::PhraseBank before" in receipts
phrase_start = receipts.index("struct PhraseUndoPayload")
phrase_end = receipts.index("struct DrumPatternUndoPayload", phrase_start)
phrase_payload = receipts[phrase_start:phrase_end]
for forbidden in ("Scene before", "SynthPattern", "DrumPatternSet"):
    assert forbidden not in phrase_payload, f"Phrase Undo snapshot regression: {forbidden}"
for forbidden in ("SynthPattern", "DrumPatternSet", "PhraseBar"):
    assert forbidden not in bounded, f"resident lifecycle copied physical material: {forbidden}"

# P1a2 lease is a frozen dependency: exact budgets, mask semantics and
# two-phase transfer ordering remain intact; complete remains Scene-independent.
for token in (
    "uint8_t trackMask",
    "preparePersistentTransfer",
    "completePersistentTransfer",
    "requestedTracksAreReferenced",
    "localSlotIsSafeForTrackMask",
    "clearOwnedTracks",
    "static_assert(sizeof(PatternLease) == 14",
    "static_assert(sizeof(PreparedPersistentTransfer) == 14",
    "static_assert(sizeof(PatternLeaseOwner) == 28",
):
    assert token in lease, f"P1a2 lease contract drifted: {token}"
complete_start = lease.index("LeaseStatus completePersistentTransfer(")
complete_end = lease.index("  bool isLeased(int globalPattern) const", complete_start)
complete = lease[complete_start:complete_end]
assert "const Scene&" not in complete
assert "globalPatternIsReferenced" not in complete
assert "slotContentIsEmpty" not in complete

# Effective allocation safety includes retained Redo ownership, while cleanup
# and persistence use only real Song/Phrase persistent references.
for token in (
    "persistentGlobalPatternReferenceCount",
    "GroovePuterUndo::undoOwner().retainsPatternBacking",
    "return persistent +",
):
    assert token in materializer, f"missing allocator retention contract: {token}"
for token in (
    "persistentGlobalPatternReferenceCount",
    "undoOwner().retainsPatternBacking",
    "patternLeaseOwner().isLeased",
    "clearBackingTrack",
    "sanitizeLifecycleForPersistence",
    "addGeneratedPhraseBacking",
):
    assert token in backing, f"missing reference-safe cleanup contract: {token}"

# KEEP is exactly the frozen two-phase lease transfer composed with canonical
# Phrase Undo publication; there is no UI history or second mutation owner.
prepare_pos = keep.index("preparePersistentTransfer")
commit_pos = keep.index("commitPhrasePrepared", prepare_pos)
complete_pos = keep.index("completePersistentTransfer", commit_pos)
assert prepare_pos < commit_pos < complete_pos
for token in (
    "scene.phraseBank = after;",
    "PhraseCore::Source::Generated",
    "PhraseCore::StorageMode::ReferenceView",
    "PhraseCore::kFlagMutableBacking",
):
    assert token in keep
for forbidden in (
    "std::vector",
    "std::unique_ptr",
    "std::shared_ptr",
    "malloc(",
    "calloc(",
    "realloc(",
    "new (",
    "evolveMultiBarPhrase",
):
    assert forbidden not in keep + backing, f"forbidden P1b dependency: {forbidden}"

# Existing Phrase capture/derive/clear now publish through the same backing-aware
# canonical owner; P1b itself adds no KEEP/A-B/TAKE/REROLL UI.
assert "PhraseUndoBacking::captureCurrentPhraseUndo" in phrase_page_h
assert "PhraseUndoBacking::commitPhrasePrepared" in phrase_page_h
assert "PhraseKeep::keep" not in phrase_page_h
assert "PhraseKeep::keep" not in phrase_page_cpp
for ui_token in ("TAKE", "REROLL", "EVOLVE NEXT"):
    assert ui_token not in keep, f"out-of-scope P1b UI token: {ui_token}"

# Both persistence boundaries remove redo-only backing from detached serialized
# views. Accepted/live Phrase refs remain untouched and are serialized normally.
for token in (
    "const Scene* persistentScene = scene_;",
    "sceneTransactionScratch()",
    "undoOwner().sanitizeForPersistence(&persistenceView)",
    "writeDrumBanks(persistentScene->drumBanks)",
    "writeSynthBanks(persistentScene->synthABanks)",
    "writeSynthBanks(persistentScene->synthBBanks)",
    "PhraseCore::persistentValueAt(scene_->phraseBank, i)",
):
    assert token in scenes_h, f"Scene JSON persistence contract missing: {token}"
assert scenes_cpp.count("GroovePuterUndo::undoOwner().clear();\n  *scene_ = *loaded;") == 2
for token in (
    "GroovePuterUndo::undoOwner().sanitizeForPersistence(&staging)",
    "writeAll(file, persistentScene->synthABanks",
    "writeAll(file, persistentScene->synthBBanks",
    "writeAll(file, persistentScene->drumBanks",
    "GroovePuterUndo::undoOwner().clear();",
):
    assert token in paging, f"raw page persistence contract missing: {token}"

# Runtime ownership is not part of persistent Scene/Phrase schemas.
for forbidden in ("UndoLifecycleMetadata", "UndoRetainedResource", "PatternLease"):
    assert forbidden not in phrase_persistence, f"transient owner leaked into Phrase persistence: {forbidden}"
assert "UndoLifecycleMetadata" not in scenes_h.split("struct Scene {", 1)[1].split("};", 1)[0]
assert "PatternLease" not in scenes_h.split("struct Scene {", 1)[1].split("};", 1)[0]

# Permanent focused cases required by P1b.
for token in (
    "testKeepUndoRedoLengths",
    "testSupersededRedoBackingIsReclaimed",
    "testSharedReferenceProtectsBacking",
    "testNewKeepReplacesRetainedPair",
    "testDiscardBeforeKeepCreatesNoUndo",
    "testFailedKeepLeavesLeaseActive",
    "testPersistenceAndTransientSanitization",
    "testRetainedBackingBlocksAllocators",
    "lifecyclePayloadCapacity() == 1424",
):
    assert token in test, f"missing P1b focused case: {token}"

print("0.9.9-P1b Phrase KEEP / Undo ownership source contracts: PASS")
