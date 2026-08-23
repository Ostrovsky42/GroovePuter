#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "src/ui/pages/song_page.h").read_text()
owner = (ROOT / "src/phrase/pattern_lease_owner.h").read_text()
picker = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text()
candidate = (ROOT / "src/dsp/song_pattern_candidate.h").read_text()
paging = (ROOT / "src/audio/pattern_paging.cpp").read_text()
help_content = (ROOT / "src/ui/global_help_content.h").read_text()
legacy_song = (ROOT / "src/ui/pages/song_page.cpp").read_text()

# Entry and modal capture: Alt+Enter is a Song-only gesture and the active modal
# is checked before Undo/Cut/Paste or any legacy Song handler.
assert "songPatternPickerOpenGesture" in picker
assert "event.alt && !event.ctrl" in picker
assert "event.key == '\\n' || event.key == '\\r'" in picker
handle_start = picker.index("bool SongPage::handleEvent(UIEvent& ui_event)")
active_idx = picker.index("if (pattern_picker_.active)", handle_start)
open_idx = picker.index("if (songPatternPickerOpenGesture(ui_event))", handle_start)
app_idx = picker.index("if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT)", handle_start)
assert active_idx < open_idx < app_idx

# EXISTING is read-only until canonical accept and never owns a PatternLease.
existing_start = picker.index("bool SongPage::selectExistingPickerPattern")
existing_end = picker.index("bool SongPage::openPatternPicker", existing_start)
existing_body = picker[existing_start:existing_end]
assert "patternLeaseOwner" not in existing_body
assert "commitSongMutation" not in existing_body
assert "setSongPattern" not in existing_body

# Generate acquires exactly one selected-track lease and reuses that same handle.
gen_start = picker.index("bool SongPage::generatePatternPickerCandidate")
gen_end = picker.index("bool SongPage::enterPatternPickerGenerateMode", gen_start)
gen_body = picker[gen_start:gen_end]
assert "owner.acquire(" in gen_body
assert "pattern_picker_.page,\n      1,\n      pattern_picker_.trackMask" in gen_body
assert "pattern_picker_.lease" in gen_body
assert "SongPatternCandidate::produce" in gen_body
assert "SongPatternCandidate::writeToLeasedAddress" in gen_body
assert "materializeSongTracks" not in gen_body
assert "commitSongMutation" not in gen_body

# Candidate production reuses legacy musical semantics but never invokes the
# allocation+Song-assignment helper.
assert "SongPatternMaterializer::actionSeed" in candidate
assert "AtlasRuntime::applyRecipe" in candidate
assert "generator.generatePattern" in candidate
assert "generator.generateDrumPattern" in candidate
assert "SongPatternMaterializer::generate(" not in candidate
assert "setSongPattern" not in candidate
assert "markSceneMutated" not in candidate

# Accept order is exactly lease prepare -> canonical Song mutation -> lease
# completion. The P4I transfer asks the same owner to classify accepted backing
# as existing Song-generated/reclaimable storage.
accept_start = picker.index("bool SongPage::acceptPatternPicker")
accept_end = picker.index("bool SongPage::discardPatternPicker", accept_start)
accept_body = picker[accept_start:accept_end]
generate_branch = accept_body[accept_body.index("auto& owner =") :]
prepare_idx = generate_branch.index("owner.preparePersistentTransfer")
commit_idx = generate_branch.index("commitSongMutation", prepare_idx)
complete_idx = generate_branch.index("owner.completePersistentTransfer", commit_idx)
assert prepare_idx < commit_idx < complete_idx
assert "PersistentClass::SongGenerated" in generate_branch
assert "markSlotSongGenerated" not in picker

# A failed Song commit does not complete or discard the lease.
failed_commit = generate_branch[
    generate_branch.index("if (!committed)") : complete_idx
]
assert "completePersistentTransfer" not in failed_commit
assert ".discard(" not in failed_commit
assert "enterPatternPickerAuditionRuntime" in failed_commit

# P1a2 remains the sole allocator and owns reclaim classification. Completion is
# still bookkeeping-only and Scene-independent after a successful Song commit.
assert "enum class PersistentClass" in owner
assert "PersistentClass::SongGenerated" in owner
assert "markOwnedTracksSongGenerated" in owner
complete_start = owner.index("LeaseStatus completePersistentTransfer(")
complete_end = owner.index("  bool isLeased(int globalPattern) const", complete_start)
complete_body = owner[complete_start:complete_end]
assert "const Scene&" not in complete_body
assert "clearOwnedTracks" not in complete_body
assert "globalPatternIsReferenced" not in complete_body

# Discard is lease-only cleanup. It cannot become another Song mutation/Undo path.
discard_start = picker.index("bool SongPage::discardPatternPicker")
discard_end = picker.index("void SongPage::onExit", discard_start)
discard_body = picker[discard_start:discard_end]
assert "patternLeaseOwner().discard" in discard_body
assert "commitSongMutation" not in discard_body
assert "undoOwner" not in discard_body

# PAGE PIN is enforced below UI routing at raw page persistence, so global Alt+[]
# cannot serialize or load leased preview bytes before Accept/Discard.
assert '#include "src/phrase/pattern_lease_owner.h"' in paging
assert "pageStoragePinnedByLease" in paging
for signature in (
    "bool PatternPagingService::savePage",
    "bool PatternPagingService::loadPage",
    "bool PatternPagingService::restoreBackup",
):
    start = paging.index(signature)
    body = paging[start : start + 260]
    assert "if (pageStoragePinnedByLease()) return false;" in body

# Picker UI updates use the bounded existing overlay. Candidate changes do not
# explicitly redraw the Song grid.
status_start = picker.index("void SongPage::showPatternPickerStatus")
status_end = picker.index("void SongPage::closePatternPickerState", status_start)
assert "showToast(status, 60000)" in picker[status_start:status_end]
assert "draw(gfx_)" not in picker
assert "drawMinimalStyle" not in picker

# RELATED remains product-visible in help only and has no producer or state in
# the implementation. Do not alias it to EVOLVE/DERIVE.
assert '"RELATED     Not implemented"' in help_content
assert "Related" not in header
assert "RELATED" not in candidate
assert "DERIVE" not in candidate
assert "EVOLVE" not in candidate

# Alt+H help matches actual controls and legacy Song G path remains present and
# untouched outside the modal implementation.
for expected in (
    '"Alt+Enter   Open Pattern Picker"',
    '"Tab         EXISTING / GENERATE"',
    '"G           Reroll GENERATE take"',
    '"Enter       Accept candidate"',
    '"Esc/`       Discard and close"',
    '"B           PAT assignment bank A/B"',
    '"Alt+B       Flip stored ref bank"',
):
    assert expected in help_content
assert "generateCurrentCellPattern(true)" in legacy_song
assert "rollbackPendingCellGeneration(cursorRow())" in legacy_song
assert "generateEntireRow()" in legacy_song

print("0.9.9-P4I Pattern Picker source contract tests passed")
