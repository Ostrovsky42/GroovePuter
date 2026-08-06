#!/usr/bin/env python3
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


root = Path(__file__).resolve().parents[1]

# ui_config.h
path = root / "src/ui/ui_config.h"
text = path.read_text()
text = replace_once(text, "kPageCount = 14", "kPageCount = 15", "UI page count")
text = text.replace("Page 13 is the standalone realtime MIDI Player.",
                    "Page 13 is the realtime MIDI Player; page 14 is Phrase Core.")
path.write_text(text)

# workflow_mode.h
path = root / "src/ui/workflow_mode.h"
text = path.read_text()
text = replace_once(text,
    "    // SONG\n    Arrange,\n\n    // SETTINGS",
    "    // SONG\n    Arrange,\n    Phrase,\n\n    // SETTINGS",
    "Phrase workspace enum")
text = replace_once(text,
    "constexpr int kPlayer = 13;",
    "constexpr int kPlayer = 13;\nconstexpr int kPhrase = 14;",
    "Phrase page constant")
text = replace_once(text,
    "           page == kArrange ||\n           isSettingsWorkflowPage(page);",
    "           page == kArrange ||\n           page == kPhrase ||\n           isSettingsWorkflowPage(page);",
    "Phrase workspace membership")
text = replace_once(text,
    "        case kArrange: return Workspace::Arrange;\n        case kProject:",
    "        case kArrange: return Workspace::Arrange;\n        case kPhrase: return Workspace::Phrase;\n        case kProject:",
    "Phrase workspace for page")
text = replace_once(text,
    "        case Workspace::Arrange: return kArrange;\n        case Workspace::Project:",
    "        case Workspace::Arrange: return kArrange;\n        case Workspace::Phrase: return kPhrase;\n        case Workspace::Project:",
    "Phrase page for workspace")
text = replace_once(text,
    "    if (page == kArrange) return WorkflowMode::Song;",
    "    if (page == kArrange || page == kPhrase) return WorkflowMode::Song;",
    "Phrase workflow mode")
text = replace_once(text,
    "        case kArrange: return \"SONG\";\n        case kProject:",
    "        case kArrange: return \"SONG\";\n        case kPhrase: return \"PHRASE CORE\";\n        case kProject:",
    "Phrase page name")
text = replace_once(text,
    "        case WorkflowMode::Song: return 1;",
    "        case WorkflowMode::Song: return 2;",
    "Song workflow page count")
text = replace_once(text,
    "    static constexpr int kSettingsPages[] = {\n        kProject, kGenerator,\n    };",
    "    static constexpr int kSongPages[] = {\n        kArrange, kPhrase,\n    };\n    static constexpr int kSettingsPages[] = {\n        kProject, kGenerator,\n    };",
    "Song page array")
text = replace_once(text,
    "        case WorkflowMode::Song: return kArrange;",
    "        case WorkflowMode::Song: return kSongPages[index];",
    "Song page lookup")
path.write_text(text)

# ui_session_state.h
path = root / "src/state/ui_session_state.h"
text = path.read_text()
text = replace_once(text, "kUiPageCount = 14", "kUiPageCount = 15", "session page count")
text = replace_once(text,
    "constexpr int kPlayer = 13;",
    "constexpr int kPlayer = 13;\nconstexpr int kPhrase = 14;",
    "session Phrase page")
text = replace_once(text,
    "    if (page == SessionPages::kArrange) return SessionWorkflow::Song;",
    "    if (page == SessionPages::kArrange ||\n        page == SessionPages::kPhrase) return SessionWorkflow::Song;",
    "session Phrase workflow")
text = replace_once(text,
    "        case SessionWorkflow::Song: return 1;",
    "        case SessionWorkflow::Song: return 2;",
    "session Song page count")
text = replace_once(text,
    "    static constexpr int kSettingsPages[] = {\n        SessionPages::kProject, SessionPages::kGenerator,\n    };",
    "    static constexpr int kSongPages[] = {\n        SessionPages::kArrange, SessionPages::kPhrase,\n    };\n    static constexpr int kSettingsPages[] = {\n        SessionPages::kProject, SessionPages::kGenerator,\n    };",
    "session Song pages")
text = replace_once(text,
    "        case SessionWorkflow::Song: return SessionPages::kArrange;",
    "        case SessionWorkflow::Song: return kSongPages[index];",
    "session Song lookup")
path.write_text(text)

# miniacid_display.cpp
path = root / "src/ui/miniacid_display.cpp"
text = path.read_text()
text = replace_once(text,
    '#include "pages/song_page.h"\n#include "pages/help_dialog.h"',
    '#include "pages/song_page.h"\n#include "pages/phrase_page.h"\n#include "pages/help_dialog.h"',
    "Phrase page include")
text = replace_once(text,
    "        case 12: page = std::make_unique<PerformPage>(gfx_, mini_acid_, performance_keyboard_); break;\n        case kSmfPlayerPage:",
    "        case 12: page = std::make_unique<PerformPage>(gfx_, mini_acid_, performance_keyboard_); break;\n        case WorkflowPages::kPhrase:\n            page = std::make_unique<PhrasePage>(gfx_, mini_acid_, audio_guard_);\n            break;\n        case kSmfPlayerPage:",
    "Phrase page factory")
path.write_text(text)

# global_help_content.h
path = root / "src/ui/global_help_content.h"
text = path.read_text()
phrase_lines = '''constexpr const char* kPhraseLines[] = {
    "=== PHRASE CORE ===",
    "1..4        Select Phrase A/B/C/D",
    "Up/Down     Capture length 1/2/4/8",
    "Left/Right  Preview Phrase bar",
    "R           Cycle capture role",
    "Shift+R     Previous role",
    "P           Cycle derive parent",
    "Enter       Capture current Song row",
    "D           Derive parent into slot",
    "W           Write to empty Song row",
    "Alt+W       Overwrite Song row",
    "Bksp/Del    Clear selected Phrase",
    "REF         Mutable pattern references",
};

'''
text = replace_once(text,
    "constexpr const char* kHubLines[] = {",
    phrase_lines + "constexpr const char* kHubLines[] = {",
    "Phrase help lines")
text = replace_once(text,
    "        case WorkflowPages::kArrange:\n            count = sizeof(kSongLines) / sizeof(kSongLines[0]); return kSongLines;\n        case WorkflowPages::kPattern:",
    "        case WorkflowPages::kArrange:\n            count = sizeof(kSongLines) / sizeof(kSongLines[0]); return kSongLines;\n        case WorkflowPages::kPhrase:\n            count = sizeof(kPhraseLines) / sizeof(kPhraseLines[0]); return kPhraseLines;\n        case WorkflowPages::kPattern:",
    "Phrase help switch")
path.write_text(text)

# SDL source list
path = root / "platform_sdl/Makefile"
text = path.read_text()
text = replace_once(text,
    "\t../src/ui/pages/song_page.cpp \\\n\t../src/ui/pages/project_page.cpp \\",
    "\t../src/ui/pages/song_page.cpp \\\n\t../src/ui/pages/phrase_page.cpp \\\n\t../src/ui/pages/project_page.cpp \\",
    "SDL Phrase source")
path.write_text(text)

# UI session host test
path = root / "tests/test_ui_session_state.cpp"
text = path.read_text()
text = replace_once(text,
    "    rememberWorkflowPage(state, SessionPages::kGenerator);",
    "    rememberWorkflowPage(state, SessionPages::kGenerator);\n    rememberWorkflowPage(state, SessionPages::kPhrase);",
    "remember Phrase page")
text = replace_once(text,
    "    assert(rememberedWorkflowPage(state, SessionWorkflow::Settings) ==\n           SessionPages::kGenerator);",
    "    assert(rememberedWorkflowPage(state, SessionWorkflow::Settings) ==\n           SessionPages::kGenerator);\n    assert(rememberedWorkflowPage(state, SessionWorkflow::Song) ==\n           SessionPages::kPhrase);\n\n    const int localSongWrap = workflowNavigationTarget(\n        state, SessionPages::kPhrase, 1, false);\n    assert(localSongWrap == SessionPages::kArrange);",
    "Phrase session assertions")
path.write_text(text)

# Global help host test
path = root / "tests/test_global_help_content.cpp"
text = path.read_text()
text = replace_once(text,
    "    constexpr int kLastPage = WorkflowPages::kPlayer;",
    "    constexpr int kLastPage = WorkflowPages::kPhrase;",
    "global help last page")
text = replace_once(text,
    "    assert(sectionContains(WorkflowPages::kArrange, \"Generate current row\"));",
    "    assert(sectionContains(WorkflowPages::kArrange, \"Generate current row\"));\n    assert(sectionContains(WorkflowPages::kPhrase, \"Select Phrase A/B/C/D\"));\n    assert(sectionContains(WorkflowPages::kPhrase, \"Capture current Song row\"));\n    assert(sectionContains(WorkflowPages::kPhrase, \"Mutable pattern references\"));",
    "Phrase help assertions")
path.write_text(text)

print("Phrase UI registration applied")
