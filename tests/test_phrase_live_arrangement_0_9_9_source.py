#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATED = (ROOT / "src/dsp/generated_phrase_song.h").read_text(encoding="utf-8")
ACTIVATION = (ROOT / "src/generation/migration/phrase_live_arrangement_activation.h").read_text(encoding="utf-8")
QUANTIZED = (ROOT / "src/generation/migration/quantized_generation_commit_impl.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
PAGE = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
PUBLIC = (ROOT / "src/generation/migration/quantized_generation_commit.h").read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    a = text.find(start)
    require(a >= 0, f"missing start anchor: {start}")
    b = text.find(end, a + len(start))
    require(b >= 0, f"missing end anchor: {end}")
    return text[a:b]


# D2 must reuse C's two publication slots and Busy admission policy rather than
# inventing a second queue/scheduler.
require('inline PendingGeneration g_slots[2]' in QUANTIZED,
        "D2 depends on the accepted C two-slot pending owner")
require('phrase_live_arrangement_activation.h' in PUBLIC,
        "D2 Phrase activation must be wired through the canonical generation boundary")
require('inline PhraseActivationMetadata g_phraseActivation[2]' in ACTIVATION,
        "D2 metadata must be indexed by the existing two C slots")
for forbidden in ('std::vector', 'std::deque', 'std::list', 'malloc(',
                  'new PhraseActivationMetadata'):
    require(forbidden not in ACTIVATION,
            f"D2 pending metadata leaked unbounded/heap ownership: {forbidden}")

# PREPARE is bounded, control-side work. It stages all material before COMMIT.
require('constexpr int kMaxPreparedBars = 8;' in GENERATED,
        "D2 PREPARE must remain bounded to at most eight bars")
require('std::array<PhraseGenerator::PhraseBar, kMaxPreparedBars> material' in GENERATED,
        "D2 must stage generated bars before publication")
prepare = between(GENERATED, 'inline bool prepare(', 'template <typename Guard>\nResult generate(')
require('applyRecipe' in prepare or 'deriveBar' in prepare,
        "D2 musical generation belongs to PREPARE")
require('commitPrepared' not in prepare and 'markSceneMutated' not in prepare,
        "PREPARE must not mutate Scene revision/Undo")

# One logical mutation: Pattern allocation/data + Song refs + row timing through
# the authoritative Undo owner. No direct markSceneMutated is allowed.
generate = between(GENERATED, 'template <typename Guard>\nResult generate(', 'inline std::size_t preparedPhraseArrangementSize()')
require(generate.find('acquireWriteLease()') < generate.find('prepare(engine'),
        "PLAY D2 must acquire the common Busy owner before PREPARE")
require(generate.count('GroovePuterUndo::undoOwner().commitPrepared') == 1,
        "D2 must publish exactly one canonical persistent COMMIT")
require('markSceneMutated' not in GENERATED,
        "D2 must not create a second Scene revision path")
persistent = between(GENERATED, 'inline void applyPreparedPersistent(', 'inline GeneratedPhraseUndoPayload captureUndo(')
for required in ('scene.synthABanks', 'scene.synthBBanks', 'scene.drumBanks',
                 'song.positions', 'scene.feel.patternBars = 1'):
    require(required in persistent,
            f"D2 atomic persistent domain missing: {required}")

# PLAY publication keeps old audible material through C's snapshot and installs
# the existing GenreManager BAR_START hook. ACTIVATE is runtime-only.
require('copyCurrentAudibleSnapshot' in ACTIVATION,
        "D2 must retain the old audible bar after persistent COMMIT")
complete = between(ACTIVATION, 'inline void completePhraseActivation(', 'inline void abortPhraseActivation(')
require('setPendingCommitHook' in complete and
        'activatePendingPhraseArrangementAtBarStart' in complete,
        "D2 must use the existing musical BAR_START hook")
activate = between(ACTIVATION, 'inline bool activatePendingPhraseArrangementAtBarStart(', 'inline bool settlePendingPhraseArrangementOnStop(')
require('SlotState::Ready' in activate and 'SlotState::Reading' in activate,
        "BAR_START may claim only committed Ready state")
require('currentRevision' in activate and 'pending.committedRevision' in activate,
        "BAR_START must reject stale revision identity")
require('exactAudibleTargetStillActive' in activate,
        "BAR_START must reject stale audible target identity")
require('owner->setSongPosition(metadata.songStart)' in activate,
        "D2 ACTIVATE must publish the prepared Song destination")
for forbidden in ('commitPrepared', 'markSceneMutated', 'applyPreparedPersistent',
                  'generatePattern', 'generateDrum', 'applyRecipe', 'SD.',
                  'ArduinoJson', 'writeScene', 'malloc(', 'new ('):
    require(forbidden not in activate,
            f"D2 BAR_START leaked forbidden work: {forbidden}")

# Boundary ordering matters: ACTIVATE must run before Song row advancement so a
# generated one-bar row neither leaks the natural next row nor lasts two bars.
bar = between(ENGINE, 'if (barTick == 0) {', '} else if (barTick % 24 == 0)')
require(bar.find('genreManager_.commitPendingRecipe()') < bar.find('advanceSongBar_()'),
        "D2 pending ACTIVATE must precede Song row advancement at BAR_START")

# STOP settles committed Phrase truth immediately; ordinary C generation keeps
# its existing cancellation/runtime-settlement path.
stop = between(ENGINE, 'void MiniAcid::stop()', 'void MiniAcid::pauseTransport()')
require('settlePendingPhraseArrangementOnStop' in stop,
        "STOP must settle a committed D2 Phrase destination")
require('cancelPendingGenerationActivation' in stop and
        'synchronizeCommittedGenerationRuntime' in stop,
        "STOP must preserve ordinary C pending semantics")

# Public Phrase G works while PLAYING and reports PendingNextBar rather than the
# old hard stop. Generated Phrase Undo is routed through the same Ctrl+Z owner.
phrase_g = between(PAGE, 'bool PhrasePage::generatePhraseToSong()', 'bool PhrasePage::deriveFromParent()')
require('STOP PLAYBACK FOR PHRASE' not in phrase_g,
        "D2 must remove the old PLAY rejection")
require('GeneratedPhraseSong::Result' in phrase_g and
        'LifecycleStatus::PendingNextBar' in phrase_g,
        "Phrase page must surface the D2 lifecycle result")
undo = between(PAGE, 'bool PhrasePage::undoPreparedOwnedState()', 'void PhrasePage::draw(')
require('GeneratedPhraseSong::ownsCurrentUndoReceipt()' in undo and
        'undoLastGeneratedPhrase' in undo,
        "generated Phrase Ctrl+Z must use its canonical D2 receipt")

# Undo-before-boundary cancels only the pending activation for the committed
# revision. Once already audible, D2 refuses mid-bar restore instead of mutating
# current sound.
undo_impl = between(GENERATED, 'GroovePuterUndo::UndoResult undoLastGeneratedPhrase(', 'inline bool prepare(')
require('cancelPendingPhraseActivationForRevision' in undo_impl,
        "D2 Undo must invalidate its matching pending activation")
require('generatedTargetAudible && !pending' in undo_impl and
        'TargetUnavailable' in undo_impl,
        "already-audible generated Phrase must not be restored mid-bar")

# Pending activation is runtime-only and must never enter Scene persistence.
for persisted in (ROOT / 'scenes.h', ROOT / 'scenes.cpp'):
    text = persisted.read_text(encoding='utf-8')
    require('PhraseActivationMetadata' not in text and
            'PreparedPhraseArrangement' not in text,
            f"D2 pending/staging leaked into persistence: {persisted.name}")

print('0.9.9-D2 live Phrase source contracts: OK')
