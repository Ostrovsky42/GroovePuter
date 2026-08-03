#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

launcher_path = ROOT / "src/ui/workspace_launcher_overlay.h"
launcher = launcher_path.read_text(encoding="utf-8")

launcher = launcher.replace(
    '#include <cstdio>\n',
    '#include <cstdio>\n#include <cstdint>\n',
    1,
)

old_open = '''    void open(Workspace workspace) {
        visible_ = true;
        selected_ = entryForWorkspace(workspace);
        child_ = WorkflowPages::pageIndexInMode(
            WorkflowPages::pageForWorkspace(workspace));
        page_request_ = -1;
        help_request_ = false;
    }
'''
new_open = '''    void open(Workspace workspace,
              const int8_t* rememberedPages = nullptr,
              int rememberedPageCount = 0) {
        loadRememberedPages_(rememberedPages, rememberedPageCount);
        visible_ = true;
        selected_ = entryForWorkspace(workspace);
        const int currentPage = WorkflowPages::pageForWorkspace(workspace);
        child_ = WorkflowPages::pageIndexInMode(currentPage);
        if (selected_ >= 0 && selected_ < kWorkflowEntryCount) {
            child_by_workflow_[selected_] = child_;
        }
        page_request_ = -1;
        help_request_ = false;
    }
'''
if launcher.count(old_open) != 1:
    raise RuntimeError("launcher open anchor missing")
launcher = launcher.replace(old_open, new_open, 1)

old_toggle = '''    void toggle(Workspace workspace) {
        if (visible_) close();
        else open(workspace);
    }
'''
new_toggle = '''    void toggle(Workspace workspace,
                const int8_t* rememberedPages = nullptr,
                int rememberedPageCount = 0) {
        if (visible_) close();
        else open(workspace, rememberedPages, rememberedPageCount);
    }
'''
if launcher.count(old_toggle) != 1:
    raise RuntimeError("launcher toggle anchor missing")
launcher = launcher.replace(old_toggle, new_toggle, 1)

old_up = '''        if (nav == GROOVEPUTER_UP) {
            selected_ = (selected_ + kEntryCount - 1) % kEntryCount;
            child_ = 0;
            return true;
        }
        if (nav == GROOVEPUTER_DOWN) {
            selected_ = (selected_ + 1) % kEntryCount;
            child_ = 0;
            return true;
        }
'''
new_up = '''        if (nav == GROOVEPUTER_UP) {
            selected_ = (selected_ + kEntryCount - 1) % kEntryCount;
            child_ = rememberedChild_(selected_);
            return true;
        }
        if (nav == GROOVEPUTER_DOWN) {
            selected_ = (selected_ + 1) % kEntryCount;
            child_ = rememberedChild_(selected_);
            return true;
        }
'''
if launcher.count(old_up) != 1:
    raise RuntimeError("launcher vertical navigation anchor missing")
launcher = launcher.replace(old_up, new_up, 1)

old_lr = '''                while (child_ < 0) child_ += count;
                while (child_ >= count) child_ -= count;
            }
            return true;
        }
'''
new_lr = '''                while (child_ < 0) child_ += count;
                while (child_ >= count) child_ -= count;
                if (selected_ < kWorkflowEntryCount) {
                    child_by_workflow_[selected_] = child_;
                }
            }
            return true;
        }
'''
if launcher.count(old_lr) != 1:
    raise RuntimeError("launcher horizontal navigation anchor missing")
launcher = launcher.replace(old_lr, new_lr, 1)

private_anchor = '''private:
    static constexpr int kWorkflowEntryCount = 5;
    static constexpr int kEntryCount = 6;
'''
private_replacement = '''private:
    static constexpr int kWorkflowEntryCount = 5;
    static constexpr int kEntryCount = 6;

    void loadRememberedPages_(const int8_t* pages, int count) {
        if (!pages || count <= 0) return;
        const int safeCount = count < kWorkflowEntryCount
            ? count
            : kWorkflowEntryCount;
        for (int entry = 0; entry < safeCount; ++entry) {
            const int page = static_cast<int>(pages[entry]);
            if (WorkflowPages::modeForPage(page) != entryMode(entry)) continue;
            child_by_workflow_[entry] =
                WorkflowPages::pageIndexInMode(page);
        }
    }

    int rememberedChild_(int entry) const {
        if (entry < 0 || entry >= kWorkflowEntryCount) return 0;
        const int count = childCount(entry);
        const int child = child_by_workflow_[entry];
        return child >= 0 && child < count ? child : 0;
    }
'''
if launcher.count(private_anchor) != 1:
    raise RuntimeError("launcher private anchor missing")
launcher = launcher.replace(private_anchor, private_replacement, 1)

old_activate = '''        page_request_ = childPage(selected_, child_);
        visible_ = false;
'''
new_activate = '''        if (selected_ < kWorkflowEntryCount) {
            child_by_workflow_[selected_] = child_;
        }
        page_request_ = childPage(selected_, child_);
        visible_ = false;
'''
if launcher.count(old_activate) != 1:
    raise RuntimeError("launcher activate anchor missing")
launcher = launcher.replace(old_activate, new_activate, 1)

old_members = '''    bool visible_ = false;
    int selected_ = 0;
    int child_ = 0;
    int page_request_ = -1;
    bool help_request_ = false;
'''
new_members = '''    bool visible_ = false;
    int selected_ = 0;
    int child_ = 0;
    int child_by_workflow_[kWorkflowEntryCount]{0, 0, 0, 0, 0};
    int page_request_ = -1;
    bool help_request_ = false;
'''
if launcher.count(old_members) != 1:
    raise RuntimeError("launcher members anchor missing")
launcher = launcher.replace(old_members, new_members, 1)
launcher_path.write_text(launcher, encoding="utf-8")

# Feed the persisted per-workflow pages into the launcher every time it opens.
display_path = ROOT / "src/ui/miniacid_display.cpp"
display = display_path.read_text(encoding="utf-8")
old_call = '''            workspace_launcher_.toggle(active_workspace_);
'''
new_call = '''            workspace_launcher_.toggle(
                active_workspace_,
                ui_session_.lastPageByWorkflow,
                GroovePuterState::kWorkflowSessionCount);
'''
if display.count(old_call) != 1:
    raise RuntimeError("display launcher toggle anchor missing")
display_path.write_text(display.replace(old_call, new_call, 1), encoding="utf-8")

# Add a source-level regression for the actual hardware launcher route.
test_path = ROOT / "tests/test_ui_session_source_regressions.py"
test = test_path.read_text(encoding="utf-8")
old_read = '''    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
'''
new_read = old_read + '''    launcher = (ROOT / "src/ui/workspace_launcher_overlay.h").read_text(encoding="utf-8")
'''
if test.count(old_read) != 1:
    raise RuntimeError("test launcher read anchor missing")
test = test.replace(old_read, new_read, 1)

old_assert = '''    require(display.count("workflowNavigationTarget") >= 2 and
            "rememberedWorkflowPage" in display,
            "workflow navigation must restore remembered pages")
'''
new_assert = old_assert + '''    require("ui_session_.lastPageByWorkflow" in display and
            "kWorkflowSessionCount" in display,
            "Fn+M launcher must receive all persisted workflow pages")
    require("child_by_workflow_" in launcher and
            "rememberedChild_(selected_)" in launcher and
            "loadRememberedPages_" in launcher,
            "launcher workflow selection must restore remembered child pages")
    require("child_ = 0;" not in launcher[launcher.index("GROOVEPUTER_UP"):launcher.index("GROOVEPUTER_LEFT")],
            "vertical launcher navigation must not reset every workflow to page zero")
'''
if test.count(old_assert) != 1:
    raise RuntimeError("test launcher assertion anchor missing")
test_path.write_text(test.replace(old_assert, new_assert, 1), encoding="utf-8")

# Clarify the hardware acceptance route in the stage documentation.
doc_path = ROOT / "docs/stages/WORKFLOW_SESSION_PERSISTENCE_STAGE.md"
doc = doc_path.read_text(encoding="utf-8")
old_doc = '''- Fn+Tab and Fn+[ / ] return to that page instead of the workflow's first page.
'''
new_doc = '''- Fn+Tab, Fn+[ / ] and the Fn+M launcher return to that page instead of the workflow's first page.
'''
if doc.count(old_doc) != 1:
    raise RuntimeError("documentation behavior anchor missing")
doc = doc.replace(old_doc, new_doc, 1)
old_check = '''- [ ] Leave PERFORM on MIDI Player, switch away and back, and remain on Player.
'''
new_check = '''- [ ] Leave PERFORM on MIDI Player, switch away and back with Fn+M, and remain on Player.
'''
if doc.count(old_check) != 1:
    raise RuntimeError("documentation checklist anchor missing")
doc_path.write_text(doc.replace(old_check, new_check, 1), encoding="utf-8")

print("launcher workflow memory fix applied")
