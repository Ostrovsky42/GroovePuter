#!/usr/bin/env python3
import argparse
import shutil
from pathlib import Path

SOURCE_ROOT = Path(__file__).resolve().parents[1]
OVERLAY = SOURCE_ROOT / "tests/e3_listen_overlay"


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise AssertionError(
            f"{label}: expected exactly one patch anchor, got {count}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def patch_ui(sketch: Path) -> None:
    ui_config = sketch / "src/ui/ui_config.h"
    workflow = sketch / "src/ui/workflow_mode.h"
    display = sketch / "src/ui/miniacid_display.cpp"

    replace_once(
        ui_config,
        "    // Fourteen established pages plus Phrase Core and standalone Sampler.\n"
        "    static constexpr int kPageCount = 16;\n",
        "    // Disposable E3 review build adds one non-persisted listening page.\n"
        "    static constexpr int kPageCount = 17;\n",
        "ui_config page count",
    )

    replace_once(
        workflow,
        "constexpr int kSampler = 15;\n",
        "constexpr int kSampler = 15;\n"
        "// Test-only E3 DROP/DISPLACE musical review page.\n"
        "constexpr int kE3Listen = 16;\n",
        "workflow E3 page id",
    )
    replace_once(
        workflow,
        '        case kSampler: return "SAMPLER";\n'
        '        default: return "PAGE";\n',
        '        case kSampler: return "SAMPLER";\n'
        '        case kE3Listen: return "E3 LISTEN";\n'
        '        default: return "PAGE";\n',
        "workflow E3 page name",
    )

    replace_once(
        display,
        '#include "pages/phrase_page.h"\n',
        '#include "pages/phrase_page.h"\n'
        '#include "pages/e3_listen_page.h"\n',
        "display include",
    )
    replace_once(
        display,
        "        case WorkflowPages::kPhrase:\n"
        "            page = std::make_unique<PhrasePage>(gfx_, mini_acid_, audio_guard_);\n"
        "            break;\n"
        "        case WorkflowPages::kSampler:\n",
        "        case WorkflowPages::kPhrase:\n"
        "            page = std::make_unique<PhrasePage>(gfx_, mini_acid_, audio_guard_);\n"
        "            break;\n"
        "        case WorkflowPages::kE3Listen:\n"
        "            page = std::make_unique<E3ListenPage>(gfx_, mini_acid_, audio_guard_);\n"
        "            break;\n"
        "        case WorkflowPages::kSampler:\n",
        "display page factory",
    )

    replace_once(
        display,
        "void MiniAcidDisplay::captureUiSession_() {\n"
        "    GroovePuterState::UiSessionState next = ui_session_;\n",
        "void MiniAcidDisplay::captureUiSession_() {\n"
        "    // E3 LISTEN is disposable review UI. Never persist page id 16.\n"
        "    if (page_index_ == WorkflowPages::kE3Listen) return;\n"
        "    GroovePuterState::UiSessionState next = ui_session_;\n",
        "display session capture guard",
    )

    old_transition = """    previous_page_index_ = page_index_;
    page_index_ = index;
    if (WorkflowPages::isWorkspacePage(index)) {
        active_workspace_ = WorkflowPages::workspaceForPage(index);
    }
    if (WorkflowPages::isStandalonePage(index)) {
        // A direct utility page must not replace the user's remembered
        // workflow child in the compact session state.
        ui_session_.activePage = static_cast<int8_t>(index);
    } else {
        GroovePuterState::rememberWorkflowPage(ui_session_, index);
    }
    scheduleUiSessionSave_();
"""
    new_transition = """    previous_page_index_ = page_index_;
    page_index_ = index;
    if (index != WorkflowPages::kE3Listen) {
        if (WorkflowPages::isWorkspacePage(index)) {
            active_workspace_ = WorkflowPages::workspaceForPage(index);
        }
        if (WorkflowPages::isStandalonePage(index)) {
            // A direct utility page must not replace the user's remembered
            // workflow child in the compact session state.
            ui_session_.activePage = static_cast<int8_t>(index);
        } else {
            GroovePuterState::rememberWorkflowPage(ui_session_, index);
        }
        scheduleUiSessionSave_();
    }
"""
    replace_once(
        display,
        old_transition,
        new_transition,
        "display non-persistent transition",
    )

    shortcut_anchor = """    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        if (event.meta && (event.key == 'm' || event.key == 'M')) {
"""
    shortcut = """    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        // Disposable E3 LISTEN toggle. The F08 hardware review established
        // Ctrl+letter as the reliable Cardputer review shortcut path.
        if (event.ctrl && !event.meta &&
            (event.key == 'v' || event.key == 'V')) {
            if (page_index_ == WorkflowPages::kE3Listen) togglePreviousPage();
            else goToPage(WorkflowPages::kE3Listen);
            return true;
        }

        if (event.meta && (event.key == 'm' || event.key == 'M')) {
"""
    replace_once(
        display,
        shortcut_anchor,
        shortcut,
        "display Ctrl+V shortcut",
    )


def patch_review_seam(sketch: Path) -> None:
    migration = sketch / "src/generation/migration/strong_rhythm_migration.cpp"

    replace_once(
        migration,
        '#include "../rhythm/rhythm_realizer.h"\n',
        '#include "../rhythm/rhythm_realizer.h"\n'
        '#include "e3_listen_review_hook.h"\n',
        "migration review hook include",
    )

    composition_old = """  const GenerationCompositionResult composition =
      resolveGenerationComposition(settings, selectionGeneration);
  result.compositionStatus = composition.status;
"""
    composition_new = """  GenerationCompositionResult composition =
      resolveGenerationComposition(settings, selectionGeneration);
  e3ListenOverrideComposition(composition);
  result.compositionStatus = composition.status;
"""
    replace_once(
        migration,
        composition_old,
        composition_new,
        "migration composition boundary",
    )

    realization_old = """  const RhythmRealizationResult realization = realizeRhythmPhrase(request);
  result.realizationStatus = realization.status;
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    result.status = StrongRhythmMigrationStatus::RealizationFailed;
    return result;
  }

  result.chordOnsets = roleOnsets(
"""
    realization_new = """  RhythmRealizationResult realization = realizeRhythmPhrase(request);
  result.realizationStatus = realization.status;
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    result.status = StrongRhythmMigrationStatus::RealizationFailed;
    return result;
  }
  e3ListenOverrideRhythmPlan(realization.plan);

  result.chordOnsets = roleOnsets(
"""
    replace_once(
        migration,
        realization_old,
        realization_new,
        "migration exact RhythmPhrasePlan boundary",
    )

    bass_old = """  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
  result.bassRhythmStatus = bass.status;
  result.bassRhythmId = bass.plan.id;
  if (bass.status != BassRhythmStatus::Ok &&
      bass.status != BassRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  BassPitchBehaviorResult bassPitch{};
"""
    bass_new = """  BassRhythmResult bass = realizeBassRhythm(bassRequest);
  result.bassRhythmStatus = bass.status;
  result.bassRhythmId = bass.plan.id;
  if (bass.status != BassRhythmStatus::Ok &&
      bass.status != BassRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  // E3L injects frozen BassRhythm onset timing here, after the production
  // BassRhythm owner and before pitch behavior / tonal materialization.
  e3ListenOverrideBassPlan(bass.plan);

  BassPitchBehaviorResult bassPitch{};
"""
    replace_once(
        migration,
        bass_old,
        bass_new,
        "migration BassRhythm audition boundary",
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare a disposable Cardputer sketch with E3 DROP/DISPLACE "
            "musical listening UI and review-only seams."
        )
    )
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    args = parser.parse_args()

    sketch = args.root.resolve()
    fixture = args.fixture.resolve()
    if not fixture.is_file():
        raise FileNotFoundError(f"generated fixture missing: {fixture}")

    patch_ui(sketch)
    patch_review_seam(sketch)

    copies = {
        OVERLAY / "e3_listen_page.h":
            sketch / "src/ui/pages/e3_listen_page.h",
        OVERLAY / "e3_listen_page.cpp":
            sketch / "src/ui/pages/e3_listen_page.cpp",
        OVERLAY / "e3_listen_review_hook.h":
            sketch / "src/generation/migration/e3_listen_review_hook.h",
        OVERLAY / "e3_listen_review_hook.cpp":
            sketch / "src/generation/migration/e3_listen_review_hook.cpp",
        OVERLAY / "e3_listen_fixture_player.h":
            sketch / "src/generation/migration/e3_listen_fixture_player.h",
        OVERLAY / "e3_listen_fixture_player.cpp":
            sketch / "src/generation/migration/e3_listen_fixture_player.cpp",
        fixture:
            sketch / "src/generation/migration/e3_listen_fixture_generated.h",
    }
    for source, destination in copies.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    print(f"E3 LISTEN disposable sketch prepared: {sketch}")
    print("Tracked src remains unchanged; all review seams are staged-build only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
