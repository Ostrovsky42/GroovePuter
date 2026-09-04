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
pattern_page = read("src/ui/pages/pattern_edit_page.h")
synth_page = read("src/ui/pages/synth_sequencer_page.cpp")
display = read("src/ui/miniacid_display.cpp")
feel = read("src/ui/pages/feel_page.cpp")

# Stage P2-S1: MiniAcid owns one compact prepared bank for the currently
# resident physical page. No page-multiplied cache and no PHRASE-sized buffers.
for required in (
    '"../phrase/runtime_pattern_event_bank.h"',
    "PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_",
    "patternRuntimeBankPage_",
    "rebuildPatternRuntimeEventBank",
    "refreshPatternRuntimeEvents",
    "activePatternRuntimeEvents",
):
    require(required in header or required in engine,
            f"missing P2 prepared-source owner token: {required}")

require("RuntimeSynthEventBuffer patternRuntime" not in header,
        "MiniAcid must not retain PHRASE-sized buffers per Pattern slot")
require("[kMaxPages]" not in between(
            header,
            "PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_",
            "PatternEventQueueHandle patternEventQueue_"),
        "P2 prepared bank must describe one resident page, not all pages")

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

# Manual Pattern COMMIT already has one bounded audio-guarded assignment. The
# derived runtime slot must settle inside that same owner, not through per-key
# hooks or a second publication queue.
manual_commit = between(pattern_page,
                        "PatternMutationResult commitPatternMutation",
                        "template <typename PrepareFn>\n  bool commitSongMutation")
require("restoreSynthPatternUndo(manager, prepared)" in manual_commit,
        "authoritative manual Pattern assignment disappeared")
require("refreshPatternRuntimeEvents" in manual_commit,
        "manual Pattern COMMIT does not settle its prepared runtime slot")
require(manual_commit.index("restoreSynthPatternUndo(manager, prepared)") <
        manual_commit.index("refreshPatternRuntimeEvents"),
        "runtime projection must be refreshed from committed Pattern truth")
require("audio_guard_(apply)" in manual_commit,
        "manual Pattern settlement must stay inside existing audio guard")

# The stopped Synth-page generate path is another canonical Pattern receipt
# owner and must settle derived runtime data in the same guarded assignment.
stopped_generate = between(synth_page,
                           "if (synth_tab_ == SynthTab::Notes && isSynthGenerateKey",
                           "if (isOutputCycleKey(ui_event))")
require("restoreSynthPatternUndo(manager, prepared)" in stopped_generate,
        "stopped Synth generate canonical assignment disappeared")
require("refreshPatternRuntimeEvents" in stopped_generate,
        "stopped Synth generate does not settle prepared runtime data")

# Physical page publication is failure-atomic: a loaded/created resident Scene
# must have its whole compact bank prepared before currentPage identity changes.
paging_start = display.index("void MiniAcidDisplay::handlePaging_")
paging = display[paging_start:]
require(paging.count("rebuildPatternRuntimeEventBank(target)") >= 2,
        "page switch/create paths must rebuild prepared bank before publication")
for marker in ("PageSwitchResult::Switched", "PageSwitchResult::Created"):
    marker_pos = paging.index(marker)
    page_publish = paging.rfind("setCurrentPage(target)", 0, marker_pos)
    bank_publish = paging.rfind("rebuildPatternRuntimeEventBank(target)", 0, marker_pos)
    require(page_publish >= 0 and bank_publish >= 0 and bank_publish < page_publish,
            f"prepared bank must publish before page identity for {marker}")

# Live FEEL swing changes playback timing today. Rebuild all resident Pattern
# projections under the existing guard; do not add new FEEL semantics.
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

print("P2 prepared-source ownership contract: OK")
