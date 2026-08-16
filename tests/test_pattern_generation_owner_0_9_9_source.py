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

b2_comment = page.index("// B2 closes the R3 generation handoff")
g_start = page.index("if (keyG)", b2_comment)
g_end = page.index("\n  if (keyF)", g_start)
g_block = page[g_start:g_end]

require("captureCurrentSynthPatternUndo" in g_block,
        "Pattern G must capture the exact current Pattern before-image")
require("SynthPattern after = before.before" in g_block,
        "Pattern G PREPARE must start from a scratch copy")
require("preparePatternEditorGeneration" in g_block,
        "Pattern G must prepare before owner publication")
require("samePattern(before.before, after)" in g_block,
        "Pattern G no-op must not publish a receipt/revision")
require("synthPatternUndoTargetAvailable" in g_block,
        "Pattern G target must be validated before COMMIT")
require("undoOwner().commitPrepared(" in g_block,
        "Pattern G must use the canonical UndoOwner")
require("UndoKind::Generation" in g_block,
        "Pattern G must publish a Generation receipt")
require("set303PatternIndex(voice_index_, currentPattern)" in g_block,
        "Pattern G must preserve the legacy PLAY note-off using the existing selector")
require("restoreSynthPatternUndo(manager, prepared)" in g_block,
        "Pattern G COMMIT must be one bounded Pattern assignment")
require("handleEventLegacyUnowned" not in g_block,
        "Pattern G must not fall back to the unowned legacy mutation path")
require("markSceneMutated" not in g_block,
        "Pattern G page code must not own Scene revision directly")
require(g_block.index("preparePatternEditorGeneration") <
        g_block.index("undoOwner().commitPrepared("),
        "generation must finish before COMMIT publication")

handler_start = page.index("bool PatternEditPage::handleEventLegacy(UIEvent& ui_event)")
handler_prefix_end = page.index("using GroovePuterUndo::PatternEdit::adjustFxParam", handler_start)
handler_prefix = page[handler_start:handler_prefix_end]
require("GROOVEPUTER_APP_EVENT_UNDO" in handler_prefix,
        "Pattern page must route B2 generation Undo")
require("owner.kind() == UndoKind::Generation" in handler_prefix,
        "Pattern page must distinguish the B2 generation receipt")
require("owner.payloadSize() == sizeof(SynthPatternUndoPayload)" in handler_prefix,
        "Pattern page must not reinterpret the larger B1 generation receipt")
require("undoPrepared<SynthPatternUndoPayload>" in handler_prefix,
        "Pattern page must restore the bounded B2 receipt")
require('UI::showToast("UNDO: GENERATION"' in handler_prefix,
        "Pattern page must surface successful generation Undo")

print("0.9.9-B2 Pattern Editor generation ownership source regressions: OK")
