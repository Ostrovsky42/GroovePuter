#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/phrase/pattern_lease_owner.h"
TEST = ROOT / "tests/test_0_9_9_p1a2_pattern_lease_generalization.cpp"
DOC = ROOT / "docs/audits/P1A2_PATTERN_LEASE_GENERALIZATION.md"
PHRASE_PAGE = ROOT / "src/ui/pages/phrase_page.cpp"
SONG_PAGE = ROOT / "src/ui/pages/song_page.cpp"
SCENES = ROOT / "scenes.h"
PHRASE_PERSISTENCE = ROOT / "src/phrase/phrase_persistence.h"

header = HEADER.read_text(encoding="utf-8")
test = TEST.read_text(encoding="utf-8")
doc = DOC.read_text(encoding="utf-8")
phrase_page = PHRASE_PAGE.read_text(encoding="utf-8")
song_page = SONG_PAGE.read_text(encoding="utf-8")
scenes = SCENES.read_text(encoding="utf-8")
phrase_persistence = PHRASE_PERSISTENCE.read_text(encoding="utf-8")

for token in (
    "uint8_t trackMask",
    "SongPatternMaterializer::kSynthAMask",
    "SongPatternMaterializer::kSynthBMask",
    "SongPatternMaterializer::kDrumsMask",
    "SongPatternMaterializer::slotContentIsEmpty",
    "SongPatternMaterializer::globalPatternIsReferenced",
    "requestedTracksAreReferenced",
    "localSlotIsSafeForTrackMask",
    "clearOwnedTracks",
    "preparePersistentTransfer",
    "completePersistentTransfer",
    "PreparedPersistentTransfer",
    "static_assert(sizeof(PatternLease) == 14",
    "static_assert(sizeof(PreparedPersistentTransfer) == 14",
    "static_assert(sizeof(PatternLeaseOwner) == 28",
):
    assert token in header, f"missing P1a2 contract token: {token}"

assert "transferCommittedOwnership" not in header
assert "PhraseGenerator::localSlotIsSafeForPhrase" not in header
assert "SongPatternLeaseOwner" not in header
assert "PatternLeaseOwner records_" not in header

# complete is intentionally Scene-independent: no fallible post-commit
# reference/persistence inspection is allowed after canonical commit success.
complete_start = header.index("LeaseStatus completePersistentTransfer(")
complete_end = header.index("  bool isLeased(int globalPattern) const", complete_start)
complete_body = header[complete_start:complete_end]
assert "Scene" not in complete_body
assert "globalPatternIsReferenced" not in complete_body
assert "slotContentIsEmpty" not in complete_body
assert "clearOwnedTracks" not in complete_body

# No UI or persistence integration in P1a2.
assert "pattern_lease_owner" not in phrase_page
assert "pattern_lease_owner" not in song_page
assert "PatternLease" not in scenes
assert "PatternLease" not in phrase_persistence

for token in (
    "testAllTrackAcquireCompatibility",
    "testSingleTrackAcquire",
    "testMixedRequestedUnrequestedOccupancy",
    "testRequestedReferencedTrackRejected",
    "testUnrequestedReferencedTrackPreserved",
    "testDiscardClearsOnlyOwnedMask",
    "testRerollReusePreservesAddressAndMask",
    "testSimultaneousMaskedLeaseCollisions",
    "testPrepareTransferValidation",
    "testFailedPersistentCommitLeavesLeaseActive",
    "testCompleteAfterCommitCannotFailOrClearAcceptedBytes",
    "testNoHeapAllocation",
):
    assert token in test, f"missing permanent P1a2 test: {token}"

for token in (
    "PAGE PIN",
    "SynthA-only",
    "SynthB-only",
    "Drums-only",
    "preparePersistentTransfer",
    "completePersistentTransfer",
    "No Phrase KEEP",
):
    assert token in doc, f"missing P1a2 audit contract: {token}"

print("0.9.9-P1a2 source contracts: PASS")
