#!/usr/bin/env python3
import argparse
import shutil
from pathlib import Path

SOURCE_ROOT = Path(__file__).resolve().parents[1]
OVERLAY = SOURCE_ROOT / "tests/f08_listen_overlay"


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise AssertionError(f"{label}: expected exactly one patch anchor, got {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prepare a disposable Cardputer sketch tree with the F08 LISTEN review UI."
    )
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    args = parser.parse_args()

    sketch = args.root.resolve()
    fixture = args.fixture.resolve()
    if not fixture.is_file():
        raise FileNotFoundError(f"generated fixture missing: {fixture}")

    ui_config = sketch / "src/ui/ui_config.h"
    workflow = sketch / "src/ui/workflow_mode.h"
    display = sketch / "src/ui/miniacid_display.cpp"

    replace_once(
        ui_config,
        "    // Fourteen established pages plus Phrase Core and standalone Sampler.\n"
        "    static constexpr int kPageCount = 16;\n",
        "    // Disposable F08 review build adds one non-persisted listening page.\n"
        "    static constexpr int kPageCount = 17;\n",
        "ui_config page count",
    )

    replace_once(
        workflow,
        "constexpr int kSampler = 15;\n",
        "constexpr int kSampler = 15;\n"
        "// Test-only F08 review page. The normal repository never persists this id.\n"
        "constexpr int kF08Listen = 16;\n",
        "workflow F08 page id",
    )
    replace_once(
        workflow,
        '        case kSampler: return "SAMPLER";\n'
        '        default: return "PAGE";\n',
        '        case kSampler: return "SAMPLER";\n'
        '        case kF08Listen: return "F08 LISTEN";\n'
        '        default: return "PAGE";\n',
        "workflow F08 page name",
    )

    replace_once(
        display,
        '#include "pages/phrase_page.h"\n',
        '#include "pages/phrase_page.h"\n#include "pages/f08_listen_page.h"\n',
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
        "        case WorkflowPages::kF08Listen:\n"
        "            page = std::make_unique<F08ListenPage>(gfx_, mini_acid_, audio_guard_);\n"
        "            break;\n"
        "        case WorkflowPages::kSampler:\n",
        "display page factory",
    )

    replace_once(
        display,
        "void MiniAcidDisplay::captureUiSession_() {\n"
        "    GroovePuterState::UiSessionState next = ui_session_;\n",
        "void MiniAcidDisplay::captureUiSession_() {\n"
        "    // F08 LISTEN is a disposable review surface. Never persist page id 16;\n"
        "    // a later normal firmware still has only the established 16 pages.\n"
        "    if (page_index_ == WorkflowPages::kF08Listen) return;\n"
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
    if (index != WorkflowPages::kF08Listen) {
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
        // Test-only F08 LISTEN toggle. Cardputer's hardware input path has a
        // proven Ctrl+letter route; requiring Ctrl+Alt together is not a
        // reliable physical chord on the device. Accept Ctrl+F regardless of
        // Alt state so an accidental Alt hold cannot block the review page.
        if (event.ctrl && !event.meta &&
            (event.key == 'f' || event.key == 'F')) {
            if (page_index_ == WorkflowPages::kF08Listen) togglePreviousPage();
            else goToPage(WorkflowPages::kF08Listen);
            return true;
        }

        if (event.meta && (event.key == 'm' || event.key == 'M')) {
"""
    replace_once(display, shortcut_anchor, shortcut, "display Ctrl+F shortcut")

    copies = {
        OVERLAY / "f08_listen_page.h": sketch / "src/ui/pages/f08_listen_page.h",
        OVERLAY / "f08_listen_page.cpp": sketch / "src/ui/pages/f08_listen_page.cpp",
        OVERLAY / "f08_listen_fixture_player.h":
            sketch / "src/generation/migration/f08_listen_fixture_player.h",
        OVERLAY / "f08_listen_fixture_player.cpp":
            sketch / "src/generation/migration/f08_listen_fixture_player.cpp",
        fixture: sketch / "src/generation/migration/f08_listen_fixture_generated.h",
    }
    for source, destination in copies.items():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    print(f"F08 LISTEN disposable sketch prepared: {sketch}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
