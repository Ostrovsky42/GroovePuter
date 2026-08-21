#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ACT = (ROOT / "src/generation/migration/live_song_arrangement_activation.h").read_text(encoding="utf-8")
QPUBLIC = (ROOT / "src/generation/migration/quantized_generation_commit.h").read_text(encoding="utf-8")
QIMPL = (ROOT / "src/generation/migration/quantized_generation_commit_impl.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
ENGINE_H = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
SONG_H = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
SONG_CPP = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
R4 = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")


def require(ok: bool, msg: str) -> None:
    if not ok:
        raise AssertionError(msg)


def between(text: str, start: str, end: str) -> str:
    a = text.find(start)
    require(a >= 0, f"missing start anchor: {start}")
    b = text.find(end, a + len(start))
    require(b >= 0, f"missing end anchor: {end}")
    return text[a:b]


# Same bounded owner, no second scheduler/queue/history.
require('live_song_arrangement_activation.h' in QPUBLIC,
        'D3 must be published through the existing generation activation owner')
require('inline PendingGeneration g_slots[2]' in QIMPL,
        'D3 depends on the accepted C two-slot owner')
require('inline SongActivationMetadata g_songActivation[2]' in ACT,
        'D3 metadata must be indexed by those same two slots')
for forbidden in ('std::vector', 'std::deque', 'std::list', 'malloc(', 'new '):
    require(forbidden not in ACT,
            f'D3 pending payload must stay fixed/no-heap: {forbidden}')

# Canonical Song persistent mutation still goes through the one-level R4 owner.
commit = between(SONG_H, 'bool commitSongMutation(PrepareFn&& prepare)', 'void drawMinimalStyle')
require(commit.count('GroovePuterUndo::undoOwner().commitPrepared') == 1,
        'one Song action must publish exactly one canonical UndoOwner COMMIT')
require('prepareSongMutationActivation' in commit,
        'D3 must decide audible impact before persistent COMMIT')
require(commit.find('prepareSongMutationActivation') < commit.find('commitPrepared'),
        'D3 snapshot/lease must be prepared before Song COMMIT')
require('completeSongMutationActivation' in commit and
        commit.find('completeSongMutationActivation') > commit.find('commitPrepared'),
        'D3 Ready publication must occur only after canonical COMMIT')
require('markSceneMutated' not in commit,
        'D3 must not create a second Scene revision path')

# Non-current/non-playing edits are immediate; current audible row/reverse requires boundary.
needs = between(ACT, 'inline bool mutationNeedsBoundary(', 'inline bool persistentSongMutationConflictsWithPending(')
require('!engine.isPlaying()' in needs and 'editedSlot != engine.songPlaybackSlot()' in needs,
        'non-playing/non-audible Song edits must not allocate pending activation')
require('before.reverse != after.reverse' in needs and 'sameSongPosition' in needs,
        'current row replacement/clear and reverse must be detected as audible mutations')

# Core acceptance: old snapshot is authoritative BEFORE dereferencing newly committed refs.
synth = between(ENGINE, 'const SynthPattern& MiniAcid::activeSynthPattern', 'const DrumPattern& MiniAcid::activeDrumPattern')
require(synth.find('pendingAudibleSynthPattern') < synth.find('songPatternIndexForTrack'),
        'Synth clear/replace would leak committed ref before boundary')
drum = between(ENGINE, 'const DrumPattern& MiniAcid::activeDrumPattern', 'int MiniAcid::clampSongPosition')
require(drum.find('pendingAudibleDrumPatternSet') < drum.find('getDrumPatternSet'),
        'Drum clear/replace must prefer the captured old audible snapshot')
sequencer = between(ENGINE, 'void MiniAcid::processSequencerEvents', 'void MiniAcid::generateAudioBuffer')
require('pendingAudibleDrumPatternSet' in sequencer,
        'Drum microtiming path must also use the pending old snapshot')

# Clear and replace are symmetric: the playback accessor must not special-case -1.
require('if (pat < 0) return kEmptySynthPattern;' in synth,
        'committed silence still needs the normal post-activation path')
require(synth.find('pendingAudibleSynthPattern') < synth.find('if (pat < 0)'),
        'clear current Synth cell must keep old audible material until ACTIVATE')

# Undo-before-boundary cancels exact revision pending first; post-activation audible Undo is rejected.
undo = between(R4, 'bool SongPage::undoPreparedSongState()', 'bool SongPage::handleEvent(')
require('hasPendingSongActivationForRevision' in undo,
        'D3 Undo must recognize its exact pending receipt revision')
require('songUndoWouldAffectAudibleTruth' in undo,
        'already-audible Song Undo must not mutate current row mid-play')
require('cancelPendingSongActivationForRevision' in undo,
        'Undo-before-boundary must invalidate matching D3 pending activation')

# Edit and Play slot ownership are separated during PLAY; Ctrl+B goes through the bounded owner.
edit_slot = between(ENGINE, 'void MiniAcid::setActiveSongSlot', 'int MiniAcid::songPlaybackSlot')
require('if (!playing)' in edit_slot,
        'EDIT:A/B selection must not redirect PLAY:A/B while transport runs')
ctrl_b = between(SONG_CPP, 'if (ui_event.ctrl && key_b)', 'if (ui_event.alt && !ui_event.ctrl && key_b)')
require('requestSongPlaybackSwitch' in ctrl_b and 'setSongPlaybackSlot' not in ctrl_b,
        'Ctrl+B PLAY switch must be boundary-owned, not immediate runtime mutation')

# Old reverse-specific queue/state machine is retired; Ctrl+R keeps gesture UX but commits Song truth.
require('songReverseTogglePending_' not in ENGINE_H,
        'D3 must not retain a second reverse pending state machine')
reverse_block = between(SONG_CPP, '// Song operations (Ctrl held)', '/*\n  if (ui_event.meta && key_m)')
require('commitSongMutation' in reverse_block,
        'Ctrl+R reverse must use canonical Song COMMIT')
require('queueSongReverseToggle();' not in reverse_block,
        'Ctrl+R must not use legacy queued reverse owner')
require('markSceneMutated' not in R4,
        'R4/D3 must not manually dirty Scene around canonical Song commits')

# ACTIVATE is runtime-only and row-boundary gated. Anchor to the implementation
# body, not the forward declaration near the top of the header.
activate = between(ACT, 'inline bool activatePendingSongArrangementAtBarStart(SceneManager& scenes) {', 'inline bool settlePendingSongArrangementOnStop(')
require('songRowBoundaryDue' in activate,
        'D3 live Song activation must wait for the existing musical row boundary')
require('SlotState::Ready' in activate and 'SlotState::Reading' in activate,
        'D3 ACTIVATE must claim only committed Ready state')
for forbidden in ('commitPrepared', 'markSceneMutated', 'currentScene().songs',
                  'SongEdit::', 'writeScene', 'ArduinoJson', 'malloc(', 'new '):
    require(forbidden not in activate,
            f'D3 audio-boundary ACTIVATE leaked forbidden work: {forbidden}')

# STOP settles D3 before generic C cancellation. Persistence must not know pending metadata.
stop = between(ENGINE, 'void MiniAcid::stop()', 'void MiniAcid::pauseTransport()')
require('settlePendingSongArrangementOnStop' in stop,
        'STOP must settle committed D3 truth')
require(stop.find('settlePendingSongArrangementOnStop') < stop.find('cancelPendingGenerationActivation'),
        'D3 STOP settlement must precede generic C cancellation')
for persisted in (ROOT / 'scenes.h', ROOT / 'scenes.cpp'):
    text = persisted.read_text(encoding='utf-8')
    require('SongActivationMetadata' not in text,
            f'D3 pending state leaked into persistence: {persisted.name}')

print('0.9.9-D3 live Song source contracts: OK')
