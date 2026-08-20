from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


page = Path("src/ui/pages/pattern_edit_page.cpp").read_text()
engine = Path("src/dsp/miniacid_engine.cpp").read_text()

helper_start = page.index("void preparePatternEditorGeneration(")
helper_end = page.index("}  // namespace", helper_start)
helper = page[helper_start:helper_end]

require("getCompiledGenerativeParams()" in helper,
        "B2 prepare must use the compiled genre profile")
require("getBehavior()" in helper,
        "B2 prepare must preserve Genre behavior")
require("GenerativeMode::Reggae" in helper,
        "B2 prepare must preserve the legacy Reggae split")
for token in ("0x1111", "0xAAAA", "motifLength = 2", "motifLength = 4",
              "avoidClusters = true", "avoidClusters = false",
              "forceOctaveJump = false"):
    require(token in helper, f"legacy Pattern G semantic missing: {token}")
require("engine.modeManager().generatePattern(" in helper,
        "B2 prepare must use the existing mode generator")
require("pattern, engine.bpm(), genreParams, behavior, idx" in helper,
        "B2 prepare changed the legacy generator inputs")

legacy_start = engine.index("void MiniAcid::randomize303Pattern(int voiceIndex)")
legacy_end = engine.index("void MiniAcid::setParameter(", legacy_start)
legacy = engine[legacy_start:legacy_end]
for token in ("getCompiledGenerativeParams()", "getBehavior()",
              "GenerativeMode::Reggae", "0x1111", "0xAAAA",
              "motifLength = 2", "motifLength = 4",
              "avoidClusters = true", "avoidClusters = false",
              "forceOctaveJump = false", "modeManager_.generatePattern(",
              "editSynthPattern(idx), bpmValue, genreParams, behavior, idx"):
    require(token in legacy, f"legacy randomize303Pattern semantic moved: {token}")

# C changes only audible publication timing. Modified/fallback G must keep the
# exact B2 musical PREPARE and compact Generation receipt, but PLAY now joins
# the shared bounded activation owner rather than replacing the Pattern mid-bar.
c_comment = page.index("// C keeps B2's legacy/fallback musical generator")
g_start = page.index("if (keyG)", c_comment)
g_end = page.index("\n  if (keyF)", g_start)
g_block = page[g_start:g_end]

require("captureCurrentSynthPatternUndo" in g_block,
        "fallback Pattern G must capture the exact current Pattern before-image")
require("SynthPattern after = before.before" in g_block,
        "fallback Pattern G PREPARE must start from a scratch copy")
require("preparePatternEditorGeneration" in g_block,
        "fallback Pattern G must preserve the B2 musical generator")
require("samePattern(before.before, after)" in g_block,
        "fallback Pattern G no-op must not publish a receipt/revision")
require("synthPatternUndoTargetAvailable" in g_block,
        "fallback Pattern G target must be validated before COMMIT")
require("undoOwner().commitPrepared(" in g_block,
        "fallback Pattern G must use the canonical UndoOwner")
require("UndoKind::Generation" in g_block,
        "fallback Pattern G must publish a Generation receipt")
require("restoreSynthPatternUndo(manager, prepared)" in g_block,
        "fallback Pattern G COMMIT must remain one bounded Pattern assignment")
require("armCompactSynthActivation" in g_block and
        "completeArmedActivation" in g_block,
        "fallback PLAY G must use the shared C activation owner")
require("set303PatternIndex(voice_index_, currentPattern)" not in g_block,
        "fallback PLAY G must not replace audible material mid-bar")
require("handleEventLegacyUnowned" not in g_block,
        "fallback Pattern G must not use the unowned legacy mutation path")
require("markSceneMutated" not in g_block,
        "fallback Pattern G page code must not own Scene revision directly")
require(g_block.index("preparePatternEditorGeneration") <
        g_block.index("undoOwner().commitPrepared("),
        "fallback generation must finish before persistent COMMIT publication")
require(g_block.index("undoOwner().commitPrepared(") <
        g_block.index("completeArmedActivation"),
        "fallback activation may become Ready only after persistent COMMIT")

# Plain unmodified G keeps the B1 quantized synth-generation path. C changes
# its internal COMMIT/ACTIVATE lifecycle, not its musical entry point or
# PendingNextBar UI result.
plain_start = page.index("// Outside NOTE ENTRY, plain G rerolls only this physical synth voice")
plain_end = page.index("// Global navigation, pattern rotation/FX editing", plain_start)
plain_g = page[plain_start:plain_end]
require("regenerateSynthWithQuantizedCommit" in plain_g,
        "plain G must preserve the B1 quantized generator")
require("PendingNextBar" in plain_g,
        "plain G must preserve next-bar activation UX")
require("GroovePuterState::markSceneMutated();" not in plain_g,
        "plain G must not double-advance Scene revision")
require("commitPatternMutation" not in plain_g,
        "plain quantized G must not be rewritten as a manual Pattern edit")

# Pattern-page Undo must route both Generation receipt shapes without decoding
# one as the other. C additionally requires compact Undo to invalidate a pending
# activation that belongs to the exact committed revision being undone.
handler_start = page.index("bool PatternEditPage::handleEventLegacy(UIEvent& ui_event)")
handler_prefix_end = page.index("using GroovePuterUndo::PatternEdit::adjustFxParam", handler_start)
handler_prefix = page[handler_start:handler_prefix_end]
require("GROOVEPUTER_APP_EVENT_UNDO" in handler_prefix,
        "Pattern page must route Generation Undo")
require("owner.kind() == UndoKind::Generation" in handler_prefix,
        "Pattern page must distinguish Generation receipts")
require("quantizedGenerationUndoPayloadSize()" in handler_prefix and
        "undoLastQuantizedGeneration(mini_acid_)" in handler_prefix,
        "Pattern page must route the B1 quantized Generation receipt")
require("owner.payloadSize() == sizeof(SynthPatternUndoPayload)" in handler_prefix,
        "Pattern page must size-discriminate the compact B2 receipt")
require("undoPrepared<SynthPatternUndoPayload>" in handler_prefix,
        "Pattern page must restore the compact B2 receipt")
require("committedRevision" in handler_prefix and
        "cancelPendingGenerationActivationForRevision" in handler_prefix,
        "compact generation Undo must cancel only its matching C pending activation")
require(handler_prefix.index("quantizedGenerationUndoPayloadSize()") <
        handler_prefix.index("sizeof(SynthPatternUndoPayload)"),
        "larger B1 Generation receipt must be dispatched before compact fallback")
require('UI::showToast("UNDO: GENERATION"' in handler_prefix,
        "Pattern page must surface successful generation Undo")

print("0.9.9-B2 Pattern Editor generation ownership source regressions: OK")
