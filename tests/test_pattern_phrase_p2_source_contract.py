#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


header = read("src/dsp/miniacid_engine.h")
engine = read("src/dsp/miniacid_engine.cpp")
bank_header = read("src/phrase/runtime_pattern_event_bank.h")
pattern_header = read("src/ui/pages/pattern_edit_page.h")
pattern_page = read("src/ui/pages/pattern_edit_page.cpp")
pattern_legacy = read("src/ui/pages/pattern_edit_page_legacy.h")
synth_page = read("src/ui/pages/synth_sequencer_page.cpp")
display = read("src/ui/miniacid_display.cpp")
feel = read("src/ui/pages/feel_page.cpp")

# P2-S1: one compact prepared bank describes exactly the resident Pattern page.
# Page identity belongs to the bank itself; do not create a second shadow owner.
for required in (
    '"../phrase/runtime_pattern_event_bank.h"',
    "PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_",
    "rebuildPatternRuntimeEventBank",
    "refreshPatternRuntimeEvents",
    "activePatternRuntimeEvents",
):
    require(required in header or required in engine,
            f"missing P2 prepared-source owner token: {required}")

for required in (
    "pageIdentity() const",
    "publishPageIdentity",
    "invalidatePageIdentity",
    "selectForPage",
):
    require(required in bank_header,
            f"compact Pattern bank lacks page-identity contract: {required}")

require("patternRuntimeBankPage_" not in header,
        "P2 must not duplicate compact-bank page identity in MiniAcid")
require("RuntimeSynthEventBuffer patternRuntime" not in header,
        "MiniAcid must not retain PHRASE-sized buffers per Pattern slot")
require("[kMaxPages]" not in between(
            header,
            "PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_",
            "PatternEventQueueHandle patternEventQueue_"),
        "P2 prepared bank must describe one resident page, not all pages")

# Resident material identity comes from PatternPagingService. SceneManager page
# bookkeeping is deliberately excluded from this runtime bank contract.
rebuild = between(engine,
                  "bool MiniAcid::rebuildPatternRuntimeEventBank",
                  "bool MiniAcid::refreshPatternRuntimeEvents")
require("PatternPagingService::activePageIndex()" in rebuild,
        "prepared bank must derive page identity from resident paging material")
require("sceneManager_.currentPageIndex()" not in rebuild and
        "sceneManager().currentPageIndex()" not in rebuild,
        "SceneManager page bookkeeping must not choose runtime bank identity")
require("publishPageIdentity" in rebuild,
        "complete resident bank must publish its page identity last")

active = between(engine,
                 "const PhraseRuntime::RuntimePatternEventBuffer&\nMiniAcid::activePatternRuntimeEvents",
                 "void MiniAcid::")
require("selectForPage" in active,
        "runtime selection must fail closed on a bank/page mismatch")
require("currentPageIndex()" in active,
        "runtime selection must use MiniAcid published page identity")
require("sceneManager_.currentPageIndex()" not in active and
        "sceneManager().currentPageIndex()" not in active,
        "SceneManager divergence must not select runtime Pattern material")

# The projector is control-side only. AudioTask may select prepared data but may
# never project/copy mutable Pattern bytes into runtime form.
sequencer = between(engine,
                    "void MiniAcid::processSequencerEvents",
                    "void MiniAcid::generateAudioBuffer")
audio = between(engine,
                "void MiniAcid::generateAudioBuffer",
                "void MiniAcid::randomize303Pattern")
for block_name, block in (("sequencer", sequencer), ("audio", audio)):
    for forbidden in (
        "projectPatternToRuntimeEvents",
        "projectPatternToRuntimePatternEvents",
        "patternRuntimeBank_.refresh",
        "rebuildPatternRuntimeEventBank",
        "refreshPatternRuntimeEvents",
    ):
        require(forbidden not in block,
                f"AudioTask {block_name} performs control-side projection: {forbidden}")

# Manual Pattern COMMIT settles derived data inside the existing bounded owner.
manual_commit = between(pattern_header,
                        "PatternMutationResult commitPatternMutation",
                        "template <typename PrepareFn>\n  bool commitSongMutation")
require("restoreSynthPatternUndo(manager, prepared)" in manual_commit,
        "authoritative manual Pattern assignment disappeared")
require("refreshPatternRuntimeEvents" in manual_commit,
        "manual Pattern COMMIT does not settle its prepared runtime slot")
require(manual_commit.index("restoreSynthPatternUndo(manager, prepared)") <
        manual_commit.index("refreshPatternRuntimeEvents"),
        "runtime projection must refresh from committed Pattern truth")
require("audio_guard_(apply)" in manual_commit,
        "manual Pattern settlement must stay inside existing audio guard")

# Undo/Redo is another persistent Pattern mutation and must settle the same slot.
pattern_undo = between(pattern_legacy,
                       "case GROOVEPUTER_APP_EVENT_UNDO",
                       "default:\n        return false;")
require("exchangeSynthPatternUndo" in pattern_undo,
        "canonical Pattern Undo exchange disappeared")
require("refreshPatternRuntimeEvents" in pattern_undo,
        "Pattern Undo/Redo leaves prepared runtime data stale")
require(pattern_undo.index("exchangeSynthPatternUndo") <
        pattern_undo.index("refreshPatternRuntimeEvents"),
        "Pattern Undo must refresh from exchanged persistent truth")

compact_generation_undo = between(
    pattern_page,
    "if (owner.kind() == UndoKind::Generation &&\n        owner.payloadSize() == sizeof(SynthPatternUndoPayload))",
    "using GroovePuterUndo::PatternEdit::adjustFxParam")
require("exchangeSynthPatternUndo" in compact_generation_undo,
        "compact Generation Undo exchange disappeared")
require("refreshPatternRuntimeEvents" in compact_generation_undo,
        "compact Generation Undo leaves prepared runtime data stale")

# Stopped Synth-page generation is another canonical Pattern assignment.
stopped_generate = between(synth_page,
                           "if (synth_tab_ == SynthTab::Notes && isSynthGenerateKey",
                           "if (isOutputCycleKey(ui_event))")
require("restoreSynthPatternUndo(manager, prepared)" in stopped_generate,
        "stopped Synth generate canonical assignment disappeared")
require("refreshPatternRuntimeEvents" in stopped_generate,
        "stopped Synth generate does not settle prepared runtime data")

# Paging publication order is strict:
# resident Scene -> paging activePage -> complete bank -> MiniAcid currentPage.
paging_start = display.index("void MiniAcidDisplay::handlePaging_")
paging = display[paging_start:]
load_anchor = "if (PatternPagingService::loadPage(target, scene)) {"
load_begin = paging.index(load_anchor)
load_end = paging.index("} else {", load_begin)
load_success = paging[load_begin:load_end]
require("rebuildPatternRuntimeEventBank" in load_success,
        "successful existing-page load does not rebuild prepared bank")
require("setCurrentPage(target)" in load_success,
        "successful existing-page load does not publish MiniAcid page")
require(load_success.index("rebuildPatternRuntimeEventBank") <
        load_success.index("setCurrentPage(target)"),
        "MiniAcid page identity published before prepared target bank")

create_anchor = "PatternPagingService::initializeEmptyPage(scene);"
create_begin = paging.index(create_anchor)
create_end = paging.index("} else if (PatternPagingService::loadPage(current, scene))", create_begin)
create_success = paging[create_begin:create_end]
require("PatternPagingService::savePage(target, scene)" in create_success,
        "new page must become resident paging material before runtime publication")
require("rebuildPatternRuntimeEventBank" in create_success,
        "new page does not build its target compact bank")
require("setCurrentPage(target)" in create_success,
        "new page does not publish MiniAcid runtime identity")
require(create_success.index("PatternPagingService::savePage(target, scene)") <
        create_success.index("rebuildPatternRuntimeEventBank") <
        create_success.index("setCurrentPage(target)"),
        "new-page publication order is not save -> bank -> MiniAcid page")

# A failed target load must not publish either derived or runtime page identity.
failed_load_tail = paging[load_end:paging.index("} else {", load_end + len("} else {"))]
require("setCurrentPage(target)" not in failed_load_tail,
        "failed target load must keep MiniAcid page unchanged")
require("rebuildPatternRuntimeEventBank" not in failed_load_tail,
        "failed target load must leave compact bank byte-stable")

# Live FEEL swing changes playback timing today; refresh all prepared projections
# under the already-existing guard without adding FEEL semantics.
feel_adjust = between(feel,
                      "void FeelPage::adjustFocused",
                      "void FeelPage::applyPreset")
feel_preset = between(feel,
                      "void FeelPage::applyPreset",
                      "void FeelPage::draw")
for name, block in (("adjust", feel_adjust), ("preset", feel_preset)):
    require("rebuildPatternRuntimeEventBank" in block,
            f"live FEEL {name} does not refresh prepared timing projections")
    require("withAudioGuard" in block,
            f"live FEEL {name} projection refresh escaped existing audio guard")

print("P2 prepared-source ownership/publication contract: OK")
