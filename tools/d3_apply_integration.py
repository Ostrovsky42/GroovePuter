#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    p = ROOT / path
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one exact anchor, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# Public activation owner: D3 is a payload extension of the existing C slots.
replace_once(
    "src/generation/migration/quantized_generation_commit.h",
    '#include "phrase_live_arrangement_activation.h"\n',
    '#include "phrase_live_arrangement_activation.h"\n#include "live_song_arrangement_activation.h"\n',
)

# Song persistent mutations keep the R4 Undo owner. D3 only arms a C slot when
# the prepared mutation changes the currently audible Song row/reverse truth.
replace_once(
    "src/ui/pages/song_page.h",
    '#include "src/state/undo_receipts.h"\n',
    '#include "src/state/undo_receipts.h"\n#include "src/generation/migration/quantized_generation_commit.h"\n',
)
replace_once(
    "src/ui/pages/song_page.h",
    '''    if (!GroovePuterUndo::songUndoTargetAvailable(manager, before)) return false;\n    return GroovePuterUndo::undoOwner().commitPrepared(\n        UndoKind::Song, before, [&]() {\n          const auto apply = [&]() {\n            manager.currentScene().songs[before.songSlot] = after;\n          };\n          if (audio_guard_) audio_guard_(apply);\n          else apply();\n        });\n''',
    '''    if (!GroovePuterUndo::songUndoTargetAvailable(manager, before)) return false;\n\n    auto lease =\n        GroovePuterRhythm::LiveSongArrangementDetail::\n            prepareSongMutationActivation(\n                mini_acid_, before.songSlot, before.before, after);\n    if (!lease.ok()) {\n      if (lease.status ==\n          GroovePuterRhythm::LiveSongArrangementDetail::SongLiveStatus::Busy) {\n        UI::showToast("SONG BUSY", 900);\n      }\n      return false;\n    }\n    if (!GroovePuterRhythm::LiveSongArrangementDetail::\n            songMutationTargetStillCommitSafe(mini_acid_, lease)) {\n      GroovePuterRhythm::LiveSongArrangementDetail::\n          abortSongMutationActivation(\n              lease, GroovePuterRhythm::QuantizedGenerationStatus::\n                         CancelledTargetChanged);\n      return false;\n    }\n\n    const bool committed = GroovePuterUndo::undoOwner().commitPrepared(\n        UndoKind::Song, before, [&]() {\n          const auto apply = [&]() {\n            manager.currentScene().songs[before.songSlot] = after;\n          };\n          if (audio_guard_) audio_guard_(apply);\n          else apply();\n        });\n    if (!committed) {\n      GroovePuterRhythm::LiveSongArrangementDetail::\n          abortSongMutationActivation(lease);\n      return false;\n    }\n    if (lease.boundaryRequired) {\n      GroovePuterRhythm::LiveSongArrangementDetail::\n          completeSongMutationActivation(\n              lease.slot, GroovePuterUndo::undoOwner().committedRevision());\n    }\n    return true;\n''',
)

# Undo-before-boundary cancels the matching D3 pending snapshot before restoring
# the Song receipt. Undo after activation is refused if it would rewrite audible
# truth mid-row.
replace_once(
    "src/ui/pages/song_page_r4_owner.inc",
    '''  const UndoResult result = owner.undoPrepared<SongUndoPayload>(\n      UndoKind::Song,\n      [&](const SongUndoPayload& receipt) {\n        return GroovePuterUndo::songUndoTargetAvailable(\n            mini_acid_.sceneManager(), receipt);\n      },\n      [&](const SongUndoPayload& receipt) {\n        const auto restore = [&]() {\n          GroovePuterUndo::restoreSongUndo(mini_acid_.sceneManager(), receipt);\n        };\n        if (audio_guard_) audio_guard_(restore);\n        else restore();\n      });\n''',
    '''  const uint32_t receiptRevision = owner.committedRevision();\n  const bool pending =\n      GroovePuterRhythm::LiveSongArrangementDetail::\n          hasPendingSongActivationForRevision(mini_acid_, receiptRevision);\n  const UndoResult result = owner.undoPrepared<SongUndoPayload>(\n      UndoKind::Song,\n      [&](const SongUndoPayload& receipt) {\n        return GroovePuterUndo::songUndoTargetAvailable(\n                   mini_acid_.sceneManager(), receipt) &&\n               (!GroovePuterRhythm::LiveSongArrangementDetail::\n                    songUndoWouldAffectAudibleTruth(mini_acid_, receipt) ||\n                pending);\n      },\n      [&](const SongUndoPayload& receipt) {\n        GroovePuterRhythm::LiveSongArrangementDetail::\n            cancelPendingSongActivationForRevision(\n                mini_acid_, receiptRevision);\n        const auto restore = [&]() {\n          GroovePuterUndo::restoreSongUndo(mini_acid_.sceneManager(), receipt);\n        };\n        if (audio_guard_) audio_guard_(restore);\n        else restore();\n      });\n''',
)
replace_once(
    "src/ui/pages/song_page_r4_owner.inc",
    '''  if (songR4QueuedReverseGesture(ui_event)) {\n    const bool pendingBefore = mini_acid_.hasPendingSongReverseToggle();\n    const bool reverseBefore = mini_acid_.isSongReverse();\n    const bool handled = handleEventLegacyUnowned(ui_event);\n    if (handled &&\n        (pendingBefore != mini_acid_.hasPendingSongReverseToggle() ||\n         reverseBefore != mini_acid_.isSongReverse())) {\n      GroovePuterState::markSceneMutated();\n    }\n    return handled;\n  }\n''',
    '''  if (songR4QueuedReverseGesture(ui_event)) {\n    // Legacy gesture timing/long-press UX is preserved, but the actual reverse\n    // mutation now routes through commitSongMutation inside the legacy handler.\n    return handleEventLegacyUnowned(ui_event);\n  }\n''',
)

# Ctrl+B becomes a runtime-only bounded PLAY slot switch. Edit A/B remains
# persistent selection and no longer drags PLAY with it during transport.
replace_once(
    "src/ui/pages/song_page.cpp",
    '''  if (ui_event.ctrl && key_b) {\n    if (!mini_acid_.liveMixModeEnabled()) {\n      withRuntimeAudioGuard([&]() { mini_acid_.setLiveMixMode(true); });\n    }\n    int nextPlaySlot = mini_acid_.songPlaybackSlot() == 0 ? 1 : 0;\n    withRuntimeAudioGuard([&]() { mini_acid_.setSongPlaybackSlot(nextPlaySlot); });\n    showToast(nextPlaySlot == 0 ? "Play: A" : "Play: B", 900);\n    return true;\n  }\n''',
    '''  if (ui_event.ctrl && key_b) {\n    if (!mini_acid_.liveMixModeEnabled()) {\n      withRuntimeAudioGuard([&]() { mini_acid_.setLiveMixMode(true); });\n    }\n    const int nextPlaySlot = mini_acid_.songPlaybackSlot() == 0 ? 1 : 0;\n    const auto status =\n        GroovePuterRhythm::LiveSongArrangementDetail::\n            requestSongPlaybackSwitch(mini_acid_, nextPlaySlot);\n    using GroovePuterRhythm::LiveSongArrangementDetail::SongLiveStatus;\n    if (status == SongLiveStatus::Busy) {\n      showToast("SONG BUSY", 900);\n    } else if (status == SongLiveStatus::TargetChanged) {\n      showToast("PLAY TARGET CHANGED", 1100);\n    } else if (status == SongLiveStatus::PendingNextRow) {\n      showToast(nextPlaySlot == 0 ? "Play A -> NEXT ROW"\n                                  : "Play B -> NEXT ROW", 1100);\n    } else {\n      showToast(nextPlaySlot == 0 ? "Play: A" : "Play: B", 900);\n    }\n    return true;\n  }\n''',
)

# Preserve Ctrl+R hold semantics but replace the private reverse queue with the
# same one-revision Song transaction and C activation owner used by other D3 edits.
replace_once(
    "src/ui/pages/song_page.cpp",
    '''    withAudioGuard([&]() {\n      if (!mini_acid_.songModeEnabled()) {\n        // Keep reverse toggle anchored to the row user is editing.\n        mini_acid_.setSongPosition(cursorRow());\n        mini_acid_.setSongMode(true);\n      }\n      mini_acid_.queueSongReverseToggle();\n    });\n    bool songModeNow = mini_acid_.songModeEnabled();\n''',
    '''    if (!mini_acid_.songModeEnabled()) {\n      withRuntimeAudioGuard([&]() {\n        mini_acid_.setSongPosition(cursorRow());\n        mini_acid_.setSongMode(true);\n      });\n    }\n    const bool reverseCommitted = commitSongMutation(\n        [&](Song& song) { song.reverse = newReverse; });\n    if (!reverseCommitted) return true;\n    bool songModeNow = mini_acid_.songModeEnabled();\n''',
)
replace_once(
    "src/ui/pages/song_page.cpp",
    '''      showToast(newReverse ? "Reverse queued (bar)" : "Forward queued (bar)", 1300);\n''',
    '''      showToast(newReverse ? "Reverse -> NEXT ROW" : "Forward -> NEXT ROW", 1300);\n''',
)

# Snapshot is authoritative audible truth while a D3 mutation is pending. The
# Synth accessor must check it before dereferencing the newly committed Song ref;
# otherwise clear (ref -> -1) would cut sound immediately. Drums already check
# the C snapshot first, and sequencer microtiming also reads that same C slot.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    '''const SynthPattern& MiniAcid::activeSynthPattern(int synthIndex) const {\n  int idx = clamp303Voice(synthIndex);\n  SongTrack track = idx == 0 ? SongTrack::SynthA : SongTrack::SynthB;\n  int pat = songPatternIndexForTrack(track);\n  if (pat < 0) return kEmptySynthPattern;\n  if (const SynthPattern* pending =\n          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleSynthPattern(\n              *this, idx)) {\n    return *pending;\n  }\n  return sceneManager_.getSynthPattern(idx, pat);\n}\n''',
    '''const SynthPattern& MiniAcid::activeSynthPattern(int synthIndex) const {\n  int idx = clamp303Voice(synthIndex);\n  if (const SynthPattern* pending =\n          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleSynthPattern(\n              *this, idx)) {\n    return *pending;\n  }\n  SongTrack track = idx == 0 ? SongTrack::SynthA : SongTrack::SynthB;\n  int pat = songPatternIndexForTrack(track);\n  if (pat < 0) return kEmptySynthPattern;\n  return sceneManager_.getSynthPattern(idx, pat);\n}\n''',
)

# EDIT slot changes during PLAY no longer redirect audible PLAY slot.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    '''void MiniAcid::setActiveSongSlot(int slot) {\n  sceneManager_.setActiveSongSlot(slot);\n  songBarIndex_ = -1;\n  if (!liveMixMode_) {\n    songPlaybackSlot_ = sceneManager_.activeSongSlot();\n  }\n  if (songMode_ && songPlaybackSlot_ == sceneManager_.activeSongSlot()) {\n    applySongPositionSelection();\n  }\n}\n''',
    '''void MiniAcid::setActiveSongSlot(int slot) {\n  sceneManager_.setActiveSongSlot(slot);\n  // D3 separates persistent EDIT selection from runtime PLAY selection. During\n  // transport, changing EDIT:A/B must never redirect the audible Song.\n  if (!playing) {\n    songBarIndex_ = -1;\n    if (!liveMixMode_) {\n      songPlaybackSlot_ = sceneManager_.activeSongSlot();\n    }\n    if (songMode_ && songPlaybackSlot_ == sceneManager_.activeSongSlot()) {\n      applySongPositionSelection();\n    }\n  }\n}\n''',
)

# Disabling LiveMix while playing also must not create an immediate hidden slot
# switch; stopped behavior stays backward-compatible.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    '''  if (!liveMixMode_) {\n    songPlaybackSlot_ = sceneManager_.activeSongSlot();\n    if (songMode_) applySongPositionSelection();\n  }\n}\nvoid MiniAcid::toggleLiveMixMode() { setLiveMixMode(!liveMixMode_); }\n''',
    '''  if (!liveMixMode_ && !playing) {\n    songPlaybackSlot_ = sceneManager_.activeSongSlot();\n    if (songMode_) applySongPositionSelection();\n  }\n}\nvoid MiniAcid::toggleLiveMixMode() { setLiveMixMode(!liveMixMode_); }\n''',
)

# Remove the separate queued-reverse state machine. Compatibility methods remain
# but never create pending state; SongPage is the canonical persistent owner.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    '''void MiniAcid::queueSongReverseToggle() {\n  if (playing && songMode_) {\n    songReverseTogglePending_ = true;\n    return;\n  }\n  sceneManager_.setSongReverse(!sceneManager_.isSongReverse());\n}\nbool MiniAcid::hasPendingSongReverseToggle() const { return songReverseTogglePending_; }\n''',
    '''void MiniAcid::queueSongReverseToggle() {\n  if (playing && songMode_) return;\n  sceneManager_.setSongReverse(!sceneManager_.isSongReverse());\n}\nbool MiniAcid::hasPendingSongReverseToggle() const { return false; }\n''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    '''    if (songMode_) {\n      if (songReverseTogglePending_) {\n        sceneManager_.setSongReverse(!sceneManager_.isSongReverse());\n        songReverseTogglePending_ = false;\n      }\n      advanceSongPlayhead();\n    }\n''',
    '''    if (songMode_) {\n      advanceSongPlayhead();\n    }\n''',
)
replace_once(
    "src/dsp/miniacid_engine.h",
    '''  bool songReverseTogglePending_ = false;\n''',
    '''''',
)

# STOP first settles D3 committed truth, then D2 Phrase, then ordinary C.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    '''  const bool phraseSettled =\n      GroovePuterRhythm::PhraseLiveArrangementDetail::\n          settlePendingPhraseArrangementOnStop(*this);\n  if (!phraseSettled &&\n      GroovePuterRhythm::QuantizedGenerationDetail::\n          cancelPendingGenerationActivation(*this)) {\n''',
    '''  const bool songSettled =\n      GroovePuterRhythm::LiveSongArrangementDetail::\n          settlePendingSongArrangementOnStop(*this);\n  const bool phraseSettled = !songSettled &&\n      GroovePuterRhythm::PhraseLiveArrangementDetail::\n          settlePendingPhraseArrangementOnStop(*this);\n  if (!songSettled && !phraseSettled &&\n      GroovePuterRhythm::QuantizedGenerationDetail::\n          cancelPendingGenerationActivation(*this)) {\n''',
)

# Avoid -Werror on a state flag that is intentionally represented by status.
replace_once(
    "src/generation/migration/live_song_arrangement_activation.h",
    '''  bool activated = false;\n''',
    '''''',
)
replace_once(
    "src/generation/migration/live_song_arrangement_activation.h",
    '''    activated = true;\n''',
    '''''',
)

print("0.9.9-D3 exact integration patch applied")
