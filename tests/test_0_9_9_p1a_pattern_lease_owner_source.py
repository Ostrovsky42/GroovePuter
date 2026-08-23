#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/phrase/pattern_lease_owner.h"
TEST = ROOT / "tests/test_0_9_9_p1a_pattern_lease_owner.cpp"
PHRASE_PAGE = ROOT / "src/ui/pages/phrase_page.cpp"
SCENES = ROOT / "scenes.h"
PHRASE_PERSISTENCE = ROOT / "src/phrase/phrase_persistence.h"
PAGING = ROOT / "src/audio/pattern_paging.cpp"

header = HEADER.read_text(encoding="utf-8")
test = TEST.read_text(encoding="utf-8")
phrase_page = PHRASE_PAGE.read_text(encoding="utf-8")
scenes = SCENES.read_text(encoding="utf-8")
phrase_persistence = PHRASE_PERSISTENCE.read_text(encoding="utf-8")
paging = PAGING.read_text(encoding="utf-8")

required_header_tokens = [
    "constexpr uint8_t kMaxLeasePatterns = 4;",
    "constexpr uint8_t kLeaseOwnerCapacity = 2;",
    "return count == 1 || count == 2 || count == 4;",
    "SongPatternMaterializer::kEditableTrackMask",
    "SongPatternMaterializer::slotContentIsEmpty",
    "SongPatternMaterializer::globalPatternIsReferenced",
    "preparePersistentTransfer",
    "completePersistentTransfer",
    "clearOwnedTracks",
    "static_assert(sizeof(PatternLease) == 14",
    "static_assert(sizeof(PatternLeaseOwner) == 28",
]
for token in required_header_tokens:
    assert token in header, f"missing cumulative P1a contract token: {token}"

for forbidden in (
    "std::vector",
    "std::unique_ptr",
    "std::shared_ptr",
    "malloc(",
    "calloc(",
    "realloc(",
    "new (",
    "evolveMultiBarPhrase",
    "deriveReferenceView",
    "transferCommittedOwnership",
):
    assert forbidden not in header, f"forbidden P1a owner dependency: {forbidden}"

# P1a lease remains runtime-only: P1b composes it behind a production service,
# but the Phrase page itself still does not own or serialize the lease.
assert "pattern_lease_owner" not in phrase_page
assert "PatternLease" not in scenes
assert "PatternLease" not in phrase_persistence

# Raw page persistence still writes all three physical banks. P1b legitimately
# routes those writes through a detached persistence view so redo-only runtime
# backing is not serialized; this does not weaken P1a discard ownership.
for token in (
    "writeAll(file, persistentScene->synthABanks",
    "writeAll(file, persistentScene->synthBBanks",
    "writeAll(file, persistentScene->drumBanks",
):
    assert token in paging, f"paging persistence audit changed: {token}"

required_test_tokens = [
    "testAcquireSupportedLengths",
    "testUniqueAddresses",
    "testSongReferenceNeverLeased",
    "testPhraseReferenceNeverLeased",
    "testSimultaneousLeaseCollisionPrevented",
    "testExhaustionFailsWithoutMutation",
    "testDiscardClearsAndReturnsAddress",
    "testActiveLeaseReuseDoesNotGrowPool",
    "testTransferMakesBackingPermanent",
    "testTransferRejectsIncompleteOwnership",
    "testInvalidAndDoubleReleaseSafety",
    "testLeaseOwnerDoesNotAllocateHeap",
]
for token in required_test_tokens:
    assert token in test, f"missing permanent cumulative P1a test: {token}"

print("0.9.9-P1a cumulative source contracts: PASS")
