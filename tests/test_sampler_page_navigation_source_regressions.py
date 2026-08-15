#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    config = (ROOT / "src/ui/ui_config.h").read_text(encoding="utf-8")
    session = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
    workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    drum_page = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
    page_h = (ROOT / "src/ui/pages/sampler_page.h").read_text(encoding="utf-8")
    page_cpp = (ROOT / "src/ui/pages/sampler_page.cpp").read_text(encoding="utf-8")
    track_h = (ROOT / "src/sampler/drum_sampler_track.h").read_text(encoding="utf-8")
    track_cpp = (ROOT / "src/sampler/drum_sampler_track.cpp").read_text(encoding="utf-8")
    help_source = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")

    require("static constexpr int kPageCount = 16;" in config,
            "legacy SAMPLER page id must remain decodable")
    require("constexpr int kSampler = 15;" in session and
            "constexpr int kSampler = 15;" in workflow,
            "legacy SAMPLER page id must remain stable across UI/session layers")
    require("if (page == kSampler) return kDrums;" in workflow,
            "legacy SAMPLER page must normalize to DRUMS")
    standalone = workflow[workflow.index("inline bool isStandalonePage"):workflow.index("inline Workspace workspaceForPage")]
    require("return false;" in standalone,
            "SAMPLER must no longer be a standalone workspace")

    require("case WorkflowPages::kSampler:" in display and
            "std::make_unique<SamplerPage>" in display,
            "legacy sampler factory must remain source-compatible")
    require('#include "sampler_page.h"' in drum_page,
            "DRUMS wrapper must own the SAMPLES tab")
    require("if (pageCount() == 3)" in drum_page and
            "addPage(std::make_shared<SamplerPage>" in drum_page,
            "SAMPLES must attach as the fourth DRUMS tab before Tab navigation")
    require("SamplerPage(MiniAcid& mini_acid, AudioGuard audio_guard);" in page_h,
            "SamplerPage must be constructible as an internal DRUMS tab")

    require("static constexpr int kRecoveredPadCount = 8;" in page_h,
            "sampler workflow must stay bounded to sequenced pads 1..8")
    require('constexpr char kSequencedPadKeys[] = "qwertyui";' in page_cpp,
            "local sample audition mapping must remain Q W E R T Y U I")

    require("void setEnabled(bool enabled);" in track_h and
            "bool isEnabled() const { return enabled_; }" in track_h,
            "DrumSamplerTrack must expose a master layer state")
    require("if (!enabled_) return;" in track_cpp,
            "disabled sampler layer must reject triggers/render work")
    require("pool_.stopAll();" in track_cpp,
            "turning the layer OFF must stop already active sample voices")
    require("toggleSampleLayer();" in page_cpp and "lowerKey == 'm'" in page_cpp,
            "SAMPLES M must toggle the whole sample layer")

    require("bool SamplerPage::clearCurrentPad()" in page_cpp and
            "p.id = SampleId{0};" in page_cpp and
            "GroovePuterState::markSceneMutated();" in page_cpp,
            "SAMPLE Backspace must clear only the selected pad assignment")
    require("file_ctrl_->isFocused()" in page_cpp and
            "ui_event.key == '\\b' || ui_event.key == 0x7F" in page_cpp,
            "pad sample clear must be explicit and scoped to the SAMPLE row")

    require("ui_event.key == '\\n' || ui_event.key == '\\r'" in page_cpp and
            "prelisten();" in page_cpp,
            "Enter must preview the selected pad")
    require("ui_event.key == ' '" not in page_cpp,
            "SAMPLES must not hijack Space from transport")

    require("GROOVEPUTER_LEFT" in page_cpp and "adjustFocusedElement(-1)" in page_cpp,
            "SAMPLES must support backward selection/adjustment")
    require("GROOVEPUTER_RIGHT" in page_cpp and "adjustFocusedElement(1)" in page_cpp,
            "SAMPLES must support forward selection/adjustment")
    require("sampleStore->preload(candidateId)" in page_cpp and
            "mini_acid_.samplerTrack->pad(padIndex).id = candidateId;" in page_cpp,
            "sample preload must remain outside the short identity publication")
    require("kit_ctrl_" not in page_h and "openLoadKitDialog" not in page_cpp and
            '"/bonnethead/kits"' not in page_cpp,
            "historical KIT LOAD must remain outside this UX cleanup")

    require('"Alt+K       Sampler"' not in help_source,
            "global help must stop advertising standalone Alt+K sampler")
    require('"Tab         Grid/feel/auto/samples"' in help_source and
            '"SAMPLES M   Layer ON/OFF"' in help_source and
            '"SAMPLES Bksp Clear pad sample"' in help_source and
            '"SAMPLES Enter Preview pad"' in help_source,
            "DRUMS help must document the product SAMPLES tab controls")
    require('"=== SAMPLER ==="' not in help_source,
            "standalone debug-style SAMPLER help must be retired")

    print("Sampler DRUMS workflow UX source contract: PASS")


if __name__ == "__main__":
    main()
