#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"anchor mismatch in {path}: {old[:80]!r}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/ui/miniacid_display.h",
    "  void transitionToPage_(int index, int context = 0);\n",
    "  void transitionToPage_(int index, int context = 0);\n"
    "  void switchWorkflow_(int direction);\n",
)

replace_once(
    "src/ui/miniacid_display.cpp",
    "    active_workspace_ = WorkflowPages::workspaceForPage(page_index_);\n"
    "    UI::currentStyle = static_cast<VisualStyle>(ui_session_.visualStyle);\n",
    "    active_workspace_ = WorkflowPages::workspaceForPage(page_index_);\n"
    "    Serial.printf(\"[SESSION] load=%d active=%d mem=%d,%d,%d,%d,%d\\n\",\n"
    "                  ui_session_loaded_ ? 1 : 0, page_index_,\n"
    "                  static_cast<int>(ui_session_.lastPageByWorkflow[0]),\n"
    "                  static_cast<int>(ui_session_.lastPageByWorkflow[1]),\n"
    "                  static_cast<int>(ui_session_.lastPageByWorkflow[2]),\n"
    "                  static_cast<int>(ui_session_.lastPageByWorkflow[3]),\n"
    "                  static_cast<int>(ui_session_.lastPageByWorkflow[4]));\n"
    "    UI::currentStyle = static_cast<VisualStyle>(ui_session_.visualStyle);\n",
)

previous_block = '''void MiniAcidDisplay::previousPage() {
    const bool workflowModifier =
        WorkflowPages::hardwareWorkflowModifierHeld();
    transitionToPage_(GroovePuterState::workflowNavigationTarget(
        ui_session_, page_index_, -1, workflowModifier));
}
'''
replace_once(
    "src/ui/miniacid_display.cpp",
    previous_block,
    previous_block + '''
void MiniAcidDisplay::switchWorkflow_(int direction) {
    const int target = GroovePuterState::rememberedAdjacentWorkflowPage(
        ui_session_, page_index_, direction);
    Serial.printf("[NAV] workflow dir=%d current=%d target=%d mem=%d,%d,%d,%d,%d\\n",
                  direction, page_index_, target,
                  static_cast<int>(ui_session_.lastPageByWorkflow[0]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[1]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[2]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[3]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[4]));
    transitionToPage_(target);
}
''',
)

old_tab = '''        if (event.meta && (event.key == '\\t' || event.scancode == GROOVEPUTER_TAB)) {
            const WorkflowMode current = WorkflowPages::modeForPage(page_index_);
            const int direction = event.shift ? -1 : 1;
            goToPage(GroovePuterState::rememberedWorkflowPage(
                ui_session_, WorkflowPages::nextMode(current, direction)));
            return true;
        }
'''
new_tab = '''        if (event.meta && (event.key == '\\t' || event.scancode == GROOVEPUTER_TAB)) {
            switchWorkflow_(event.shift ? -1 : 1);
            return true;
        }

        // Modified brackets are global workflow navigation. Handle them before
        // the current page gets first refusal so synth/drum local handlers
        // cannot consume Fn+[ / ] as ordinary page-local bracket input.
        if (event.meta && (event.key == '[' || event.key == '{')) {
            switchWorkflow_(-1);
            return true;
        }
        if (event.meta && (event.key == ']' || event.key == '}')) {
            switchWorkflow_(1);
            return true;
        }
'''
replace_once("src/ui/miniacid_display.cpp", old_tab, new_tab)

replace_once(
    "src/ui/miniacid_display.cpp",
    "        if (GroovePuterPlatform::saveCardputerUiSession(ui_session_)) {\n"
    "            ui_session_save_pending_ = false;\n",
    "        if (GroovePuterPlatform::saveCardputerUiSession(ui_session_)) {\n"
    "            ui_session_save_pending_ = false;\n"
    "            Serial.printf(\"[SESSION] saved active=%d mem=%d,%d,%d,%d,%d\\n\",\n"
    "                          static_cast<int>(ui_session_.activePage),\n"
    "                          static_cast<int>(ui_session_.lastPageByWorkflow[0]),\n"
    "                          static_cast<int>(ui_session_.lastPageByWorkflow[1]),\n"
    "                          static_cast<int>(ui_session_.lastPageByWorkflow[2]),\n"
    "                          static_cast<int>(ui_session_.lastPageByWorkflow[3]),\n"
    "                          static_cast<int>(ui_session_.lastPageByWorkflow[4]));\n",
)

replace_once(
    "GroovePuter.ino",
    "        if (u == '\\n' || u == '\\r' || u == '\\b') continue;\n",
    "        if (u == '\\n' || u == '\\r' || u == '\\b' || u == '\\t') continue;\n",
)

replace_once(
    "src/ui/workspace_launcher_overlay.h",
    "        gfx.drawText(4, 2, \"GROOVEPUTER / NAV\");\n",
    "        gfx.drawText(4, 2, \"GROOVEPUTER / NAV R3\");\n",
)

launcher_anchor = '''        if (selected_ < kWorkflowEntryCount) {
            char pos[24];
            std::snprintf(pos, sizeof(pos), "PAGE %d/%d", safeChild + 1, count);
            gfx.setTextColor(p.accent2);
            gfx.drawText(rightX + 5, panelY + 67, pos);

            gfx.setTextColor(p.dim);
            gfx.drawText(rightX + 5, panelY + 80,
                         count > 1 ? "[ ] PAGE  FN+[ ] FLOW" : "FN+[ ] WORKFLOW");
        }
'''
launcher_replacement = launcher_anchor + '''
        char memory[32];
        std::snprintf(memory, sizeof(memory), "MEM %d %d %d %d %d",
                      child_by_workflow_[0] + 1,
                      child_by_workflow_[1] + 1,
                      child_by_workflow_[2] + 1,
                      child_by_workflow_[3] + 1,
                      child_by_workflow_[4] + 1);
        gfx.setTextColor(p.dim);
        gfx.drawText(rightX + 5, panelY + 92, memory);
'''
replace_once(
    "src/ui/workspace_launcher_overlay.h",
    launcher_anchor,
    launcher_replacement,
)

source_test = ROOT / "tests/test_ui_session_source_regressions.py"
test = source_test.read_text(encoding="utf-8")
test = test.replace(
    '    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")\n',
    '    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")\n'
    '    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")\n',
    1,
)
needle = '''    require("child_ = 0;" not in launcher[launcher.index("GROOVEPUTER_UP"):launcher.index("GROOVEPUTER_LEFT")],
            "vertical launcher navigation must not reset every workflow to page zero")
'''
insert = needle + '''    require("GROOVEPUTER / NAV R3" in launcher and
            "MEM %d %d %d %d %d" in launcher,
            "hardware retest build must expose launcher revision and memory")
    page_dispatch = display.index("currentPage->handleEvent(event)")
    fn_left = display.index("event.meta && (event.key == '['")
    fn_right = display.index("event.meta && (event.key == ']'")
    require(fn_left < page_dispatch and fn_right < page_dispatch and
            "switchWorkflow_" in display,
            "Fn brackets must be handled globally before page first refusal")
    require("u == '\\t'" in sketch,
            "Cardputer input loop must not dispatch TAB from both HID and word paths")
'''
if needle not in test:
    raise RuntimeError("source regression insertion anchor missing")
source_test.write_text(test.replace(needle, insert, 1), encoding="utf-8")

doc = ROOT / "docs/stages/WORKFLOW_SESSION_PERSISTENCE_STAGE.md"
doc_text = doc.read_text(encoding="utf-8")
doc_text += '''

## Hardware retest R3

The second hardware retest still reproduced the reset. R3 therefore makes the
runtime route observable instead of relying only on source-level assertions:

- Fn+Tab and Fn+[ / ] share `switchWorkflow_()`;
- modified brackets are intercepted before page-local handlers;
- Cardputer TAB is emitted once, not from both HID and word paths;
- the Fn+M title contains `NAV R3` to prove the flashed binary;
- Fn+M displays `MEM P G H S C` as one-based child positions.

Expected memory after selecting Player, Feel/Texture and Synth B Sound:

```text
MEM 2 3 6 1 1
```

Serial additionally prints `[SESSION] load`, `[SESSION] saved` and `[NAV]`
records with exact page IDs.
'''
doc.write_text(doc_text, encoding="utf-8")
