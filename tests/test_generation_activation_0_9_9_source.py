#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OWNER = (ROOT / "src/generation/migration/quantized_generation_undo_owner_impl.h").read_text()
SLOTS = (ROOT / "src/generation/migration/quantized_generation_commit_impl.h").read_text()
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text()
HEADER = (ROOT / "src/dsp/miniacid_engine.h").read_text()
PATTERN = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text()
GENRE = (ROOT / "src/dsp/genre_manager.h").read_text()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    a = text.find(start)
    require(a >= 0, f"missing start anchor: {start}")
    b = text.find(end, a + len(start))
    require(b >= 0, f"missing end anchor: {end}")
    return text[a:b]


# One bounded owner: existing two fixed publication slots, no heap queue, and
# an explicit Busy/reject admission policy while anything is published.
require("inline PendingGeneration g_slots[2]" in SLOTS,
        "C must retain the fixed two-slot publication storage")
require("Armed" in SLOTS and "committedRevision" in SLOTS,
        "pending activation must carry Armed state and committed revision")
lease = between(SLOTS, "inline WriteLease acquireWriteLease()", "inline int acquireCompanionActivationSlot")
require("g_publishedSlot.load" in lease and "return WriteLease{}" in lease,
        "second generation intent must be explicitly rejected while pending")
for forbidden in ("std::vector", "std::deque", "std::list", "new PendingGeneration", "malloc("):
    require(forbidden not in OWNER, f"pending owner leaked heap/unbounded storage: {forbidden}")

# Persistent COMMIT is separate from runtime ACTIVATE.
persistent = between(OWNER, "inline void applyPreparedGenerationPersistent", "inline void activatePreparedGenerationRuntime")
require("scene.synthABanks" in persistent and "scene.drumBanks" in persistent,
        "persistent full generation must publish committed material")
require("scenes.setMode(pending.mode)" in persistent and "scenes.setBpm(pending.bpm)" in persistent,
        "persistent truth must include mode/BPM before activation")
require("engine.setBpm" not in persistent and "activateCommittedGrooveboxModeRuntime" not in persistent,
        "COMMIT must not publish runtime/audible controls")
activate = between(OWNER, "inline void activatePreparedGenerationRuntime", "inline bool commitPreparedGeneration")
require("activateCommittedGrooveboxModeRuntime" in activate and "engine.setBpm" in activate,
        "ACTIVATE must own deferred runtime mode/BPM")
require("markSceneMutated" not in activate and "commitPrepared" not in activate,
        "ACTIVATE must not create a revision or second Undo receipt")

# PLAY commits before PendingNextBar completion. BAR_START claims only Ready and
# performs no persistent commit/generation/filesystem work.
full_flow = between(OWNER, "inline QuantizedGenerationResult regenerateWithQuantizedCommit", "inline QuantizedGenerationResult regenerateSynthWithQuantizedCommit")
require(full_flow.find("commitPreparedGeneration(engine, candidate, before)") <
        full_flow.find("completeArmedActivation"),
        "PLAY full generation must COMMIT before exposing Ready pending activation")
require("g_commitSerial.fetch_add" in full_flow,
        "commit serial must advance at persistent COMMIT, not ACTIVATE")
bar = between(OWNER, "inline bool commitQuantizedGenerationAtBarStart", "inline QuantizedGenerationResult regenerateWithQuantizedCommit")
require("SlotState::Ready" in bar and "SlotState::Reading" in bar,
        "BAR_START may claim only a fully committed Ready activation")
require("pending.committedRevision" in bar and "currentRevision" in bar,
        "BAR_START must validate the committed revision identity")
require("targetStillActive" in bar,
        "BAR_START must validate exact target identity")
require("activatePreparedGenerationRuntime" in bar,
        "BAR_START must perform runtime activation")
require(bar.count("synchronizeCommittedGenerationRuntime(*owner)") >= 2,
        "stale target/revision drop must settle runtime to current committed Scene truth")
for forbidden in ("commitPreparedGeneration(", "undoOwner().commitPrepared", "markSceneMutated",
                  "generatePattern", "generateDrum", "SD.", "File ", "ArduinoJson", "writeScene"):
    require(forbidden not in bar, f"BAR_START leaked forbidden work: {forbidden}")

# Audio reads old audible material/swing/genre from the overlay while Scene is
# already the persistent truth. Global old truth remains pending through a
# selector change; only material accessors require exact target identity so old
# Pattern bytes can never be redirected to the new selector.
base_overlay = between(OWNER, "inline const PendingGeneration* pendingAudibleActivation", "inline const SynthPattern* pendingAudibleSynthPattern")
require("targetStillActive" not in base_overlay,
        "global audible overlay must survive selector changes until BAR_START")
synth_overlay = between(OWNER, "inline const SynthPattern* pendingAudibleSynthPattern", "inline const DrumPatternSet* pendingAudibleDrumPatternSet")
require("targetStillActive" in synth_overlay,
        "synth old-material overlay must validate exact target identity")
drum_overlay = between(OWNER, "inline const DrumPatternSet* pendingAudibleDrumPatternSet", "inline const GenreSettings* pendingAudibleGenreSettings")
require("targetStillActive" in drum_overlay,
        "drum old-material overlay must validate exact target identity")
require("pendingAudibleSynthPattern" in ENGINE,
        "synth playback must consult pending audible overlay")
require(ENGINE.count("pendingAudibleDrumPatternSet") >= 3,
        "all drum playback/timing paths must consult pending audible overlay")
require("audibleGenerationSwingPct" in ENGINE,
        "swing must remain old audible truth until activation")
require("pendingAudibleGenreSettings" in ENGINE and "GenreCatalog::grooveRecipe" in ENGINE,
        "gate/groove recipe must remain old audible truth until activation")

# Save serializes Scene committed truth and never serializes pending. In
# particular it may not overwrite newly committed BPM with old audible BPM.
sync = between(ENGINE, "void MiniAcid::syncSceneStateToManager()", "int dorian_intervals")
require("hasPendingFullGenerationActivation" in sync,
        "Save sync must detect a pending full activation")
require("if (!GroovePuterRhythm::QuantizedGenerationDetail::hasPendingFullGenerationActivation" in sync,
        "old runtime BPM may be copied only when no full activation is pending")
for persisted_file in (ROOT / "scene_storage.h", ROOT / "scenes.h", ROOT / "scenes.cpp"):
    if persisted_file.exists():
        require("PendingGeneration" not in persisted_file.read_text(),
                f"pending activation leaked into persistence: {persisted_file.name}")

# Lifecycle cancellation is runtime-only: load/new/reset/stop discard pending;
# cancellation helpers themselves never restore Scene material. STOP additionally
# settles runtime to the already-committed Scene because no later BAR_START will
# perform normal ACTIVATE.
for anchor in ("void MiniAcid::reset()", "void MiniAcid::stop()",
               "bool MiniAcid::createNewSceneWithName", "void MiniAcid::loadSceneFromStorage"):
    block = ENGINE[ENGINE.find(anchor):ENGINE.find("}\n", ENGINE.find(anchor)) + 2]
    require("cancelPendingGenerationActivation" in block,
            f"pending activation not cancelled by lifecycle path: {anchor}")
stop_block = between(ENGINE, "void MiniAcid::stop()", "void MiniAcid::pauseTransport()")
require("cancelPendingGenerationActivation" in stop_block and
        "synchronizeCommittedGenerationRuntime" in stop_block,
        "STOP must drop pending and settle runtime to committed truth")
require("cancelPendingGenerationActivation(*this);\n  Serial.println(\"[LoadScene] Applying scene state...\")" in ENGINE,
        "successful project Load must drop pending before applying loaded state")
cancel = between(OWNER, "inline bool cancelPendingGenerationActivationForRevision", "inline int armCompactSynthActivation")
for forbidden in ("restoreGenerationUndo", "restoreSynthPatternUndo", "setMode(", "setBpm("):
    require(forbidden not in cancel,
            f"pending cancellation must not roll back committed persistent truth: {forbidden}")
settle = between(OWNER, "inline void synchronizeCommittedGenerationRuntime", "inline bool cancelPendingGenerationActivationForRevision")
require("sceneManager().getMode()" in settle and "sceneManager().getBpm()" in settle,
        "runtime settlement must derive from current committed Scene truth")
require("markSceneMutated" not in settle and "commitPrepared" not in settle,
        "runtime settlement must not create persistent mutation or Undo")

# Undo of either receipt shape invalidates only a pending activation belonging
# to the undone committed revision.
large_undo = between(OWNER, "inline GroovePuterUndo::UndoResult undoLastQuantizedGeneration", "inline std::size_t quantizedGenerationUndoPayloadSize")
require("committedRevision" in large_undo and "cancelPendingGenerationActivationForRevision" in large_undo,
        "large quantized Undo must invalidate matching pending activation")
compact_undo = between(PATTERN, "owner.payloadSize() == sizeof(SynthPatternUndoPayload)", "using GroovePuterUndo::PatternEdit::adjustFxParam")
require("committedRevision" in compact_undo and "cancelPendingGenerationActivationForRevision" in compact_undo,
        "compact fallback Undo must invalidate matching pending activation")

# Both generators share one owner, but their musical preparation remains
# separate. Fallback still uses its B2 helper; plain G still uses quantized flow.
fallback = between(PATTERN, "// C keeps B2's legacy/fallback musical generator", "if (keyF)")
require("preparePatternEditorGeneration" in fallback,
        "C must not replace fallback G musical generator")
require("armCompactSynthActivation" in fallback and "completeArmedActivation" in fallback,
        "fallback G must use the common bounded activation owner")
plain = between(PATTERN, "// Outside NOTE ENTRY, plain G", "// Global navigation")
require("regenerateSynthWithQuantizedCommit" in plain and "PendingNextBar" in plain,
        "plain G must retain quantized generator semantics")
require("activateCommittedGrooveboxModeRuntime" in HEADER,
        "engine must expose a runtime-only committed-mode activation boundary")
require("Persistent COMMIT has already completed" in GENRE,
        "BAR_START compatibility hook must document ACTIVATE ownership")

print("0.9.9-C bounded activation source contracts: OK")
