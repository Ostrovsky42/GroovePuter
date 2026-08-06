#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    file_path = ROOT / path
    file_path.parent.mkdir(parents=True, exist_ok=True)
    file_path.write_text(content, encoding="utf-8")


def replace_once(path: str, old: str, new: str, label: str) -> None:
    text = read(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"{label}: anchor not found in {path}")
    write(path, text.replace(old, new, 1))


# ---------------------------------------------------------------------------
# Runtime workflow: retire page 11 without reusing its persisted identifier.
# ---------------------------------------------------------------------------
workflow_path = "src/ui/workflow_mode.h"
workflow = read(workflow_path)
workflow = workflow.replace(
    "// Their page labels are now the fixed four-axis GENERATE addresses.",
    "// Page 11 remains reserved as a legacy GENERATION redirect to SONG.",
)
workflow = workflow.replace(
    "    Generation,\n    Texture,",
    "    Generation,  // retired compatibility value -> SONG\n    Texture,",
)
workflow = workflow.replace(
    "inline bool isGenerateWorkflowPage(int page) {\n"
    "    return page == kGenre ||\n"
    "           page == kFeel ||\n"
    "           page == kGeneration ||\n"
    "           page == kTexture;\n"
    "}\n",
    "inline bool isGenerateWorkflowPage(int page) {\n"
    "    return page == kGenre ||\n"
    "           page == kFeel ||\n"
    "           page == kTexture;\n"
    "}\n",
)
workflow = workflow.replace(
    "           page == kArrange ||\n"
    "           page == kPhrase ||\n",
    "           page == kArrange ||\n"
    "           page == kPhrase ||\n"
    "           page == kGeneration ||\n",
)
workflow = workflow.replace(
    "        case kGeneration: return Workspace::Generation;",
    "        case kGeneration: return Workspace::Arrange;",
)
workflow = workflow.replace(
    "        case Workspace::Generation: return kGeneration;",
    "        case Workspace::Generation: return kArrange;",
)
workflow = workflow.replace(
    "    if (page == kArrange || page == kPhrase) return WorkflowMode::Song;",
    "    if (page == kArrange || page == kPhrase || page == kGeneration) return WorkflowMode::Song;",
)
workflow = workflow.replace(
    "        case kGeneration: return \"GENERATION\";",
    "        case kGeneration: return \"SONG\";",
)
workflow = workflow.replace(
    "        case WorkflowMode::Generate: return 4;",
    "        case WorkflowMode::Generate: return 3;",
)
workflow = workflow.replace(
    "    static constexpr int kGeneratePages[] = {\n"
    "        kGenre, kFeel, kGeneration, kTexture,\n"
    "    };",
    "    static constexpr int kGeneratePages[] = {\n"
    "        kGenre, kFeel, kTexture,\n"
    "    };",
)
for required in (
    "case Workspace::Generation: return kArrange;",
    "case kGeneration: return Workspace::Arrange;",
    "case WorkflowMode::Generate: return 3;",
    "kGenre, kFeel, kTexture",
):
    if required not in workflow:
        raise RuntimeError(f"workflow migration missing: {required}")
write(workflow_path, workflow)

session_path = "src/state/ui_session_state.h"
session = read(session_path)
session = session.replace(
    "    if (page == SessionPages::kGenre ||\n"
    "        page == SessionPages::kFeel ||\n"
    "        page == SessionPages::kGeneration ||\n"
    "        page == SessionPages::kTexture) {",
    "    if (page == SessionPages::kGenre ||\n"
    "        page == SessionPages::kFeel ||\n"
    "        page == SessionPages::kTexture) {",
)
session = session.replace(
    "    if (page == SessionPages::kArrange ||\n"
    "        page == SessionPages::kPhrase) {",
    "    if (page == SessionPages::kArrange ||\n"
    "        page == SessionPages::kPhrase ||\n"
    "        page == SessionPages::kGeneration) {",
)
session = session.replace(
    "inline bool pageBelongsToWorkflow(int page, SessionWorkflow workflow) {\n"
    "    return validUiPage(page) && sessionWorkflowForPage(page) == workflow;\n"
    "}\n",
    "inline bool pageBelongsToWorkflow(int page, SessionWorkflow workflow) {\n"
    "    if (page == SessionPages::kGeneration) return false;\n"
    "    return validUiPage(page) && sessionWorkflowForPage(page) == workflow;\n"
    "}\n",
)
session = session.replace(
    "inline void sanitizeUiSessionState(UiSessionState& state) {\n"
    "    for (int i = 0; i < kWorkflowSessionCount; ++i) {",
    "inline void sanitizeUiSessionState(UiSessionState& state) {\n"
    "    const int generateIndex = workflowSessionIndex(SessionWorkflow::Generate);\n"
    "    const int songIndex = workflowSessionIndex(SessionWorkflow::Song);\n"
    "    if (state.lastPageByWorkflow[generateIndex] == SessionPages::kGeneration) {\n"
    "        state.lastPageByWorkflow[generateIndex] =\n"
    "            static_cast<int8_t>(SessionPages::kGenre);\n"
    "    }\n"
    "    if (state.activePage == SessionPages::kGeneration) {\n"
    "        state.activePage = static_cast<int8_t>(SessionPages::kArrange);\n"
    "        state.lastPageByWorkflow[songIndex] =\n"
    "            static_cast<int8_t>(SessionPages::kArrange);\n"
    "    }\n\n"
    "    for (int i = 0; i < kWorkflowSessionCount; ++i) {",
)
session = session.replace(
    "inline void rememberWorkflowPage(UiSessionState& state, int page) {\n"
    "    if (!validUiPage(page)) return;",
    "inline void rememberWorkflowPage(UiSessionState& state, int page) {\n"
    "    if (page == SessionPages::kGeneration) page = SessionPages::kArrange;\n"
    "    if (!validUiPage(page)) return;",
)
session = session.replace(
    "        case SessionWorkflow::Generate: return 4;",
    "        case SessionWorkflow::Generate: return 3;",
)
session = session.replace(
    "    static constexpr int kGeneratePages[] = {\n"
    "        SessionPages::kGenre,\n"
    "        SessionPages::kFeel,\n"
    "        SessionPages::kGeneration,\n"
    "        SessionPages::kTexture,\n"
    "    };",
    "    static constexpr int kGeneratePages[] = {\n"
    "        SessionPages::kGenre,\n"
    "        SessionPages::kFeel,\n"
    "        SessionPages::kTexture,\n"
    "    };",
)
session = session.replace(
    "inline int workflowNavigationTarget(const UiSessionState& state,\n"
    "                                    int currentPage,\n"
    "                                    int direction,\n"
    "                                    bool workflowModifier) {\n"
    "    const SessionWorkflow workflow = sessionWorkflowForPage(currentPage);",
    "inline int workflowNavigationTarget(const UiSessionState& state,\n"
    "                                    int currentPage,\n"
    "                                    int direction,\n"
    "                                    bool workflowModifier) {\n"
    "    if (currentPage == SessionPages::kGeneration) {\n"
    "        currentPage = SessionPages::kArrange;\n"
    "    }\n"
    "    const SessionWorkflow workflow = sessionWorkflowForPage(currentPage);",
)
for required in (
    "case SessionWorkflow::Generate: return 3;",
    "if (page == SessionPages::kGeneration) return false;",
    "if (state.activePage == SessionPages::kGeneration)",
    "if (page == SessionPages::kGeneration) page = SessionPages::kArrange;",
):
    if required not in session:
        raise RuntimeError(f"session migration missing: {required}")
write(session_path, session)

# ---------------------------------------------------------------------------
# Display integration and build lists.
# Also repair the stale include/type names left by the previous merge.
# ---------------------------------------------------------------------------
display_path = "src/ui/miniacid_display.cpp"
display = read(display_path)
display = display.replace('#include "pages/feel_texture_page.h"', '#include "pages/texture_page.h"')
display = display.replace('#include "pages/settings_page.h"', '#include "pages/feel_page.h"')
display = display.replace('#include "pages/mode_page.h"\n', '')
display = display.replace(
    "        case 8:  page = std::make_unique<FeelTexturePage>(gfx_, mini_acid_, audio_guard_); break;",
    "        case 8:  page = std::make_unique<TexturePage>(gfx_, mini_acid_, audio_guard_); break;",
)
display = display.replace(
    "        case 9:  page = std::make_unique<SettingsPage>(gfx_, mini_acid_, audio_guard_); break;",
    "        case 9:  page = std::make_unique<FeelPage>(gfx_, mini_acid_, audio_guard_); break;",
)
display = display.replace(
    "        case 11: page = std::make_unique<ModePage>(gfx_, mini_acid_, audio_guard_); break;\n",
    "",
)
display = display.replace(
    "void MiniAcidDisplay::transitionToPage_(int index, int context) {\n"
    "    if (index < 0 || index >= kPageCount) {",
    "void MiniAcidDisplay::transitionToPage_(int index, int context) {\n"
    "    if (index == WorkflowPages::kGeneration) {\n"
    "        Serial.println(\"[UI] retired GENERATION page -> SONG\");\n"
    "        index = WorkflowPages::kArrange;\n"
    "    }\n"
    "    if (index < 0 || index >= kPageCount) {",
)
for forbidden in ("feel_texture_page.h", "settings_page.h", "mode_page.h", "ModePage"):
    if forbidden in display:
        raise RuntimeError(f"stale display integration remains: {forbidden}")
for required in ("TexturePage", "FeelPage", "retired GENERATION page -> SONG"):
    if required not in display:
        raise RuntimeError(f"display integration missing: {required}")
write(display_path, display)

makefile_path = "platform_sdl/Makefile"
makefile = read(makefile_path)
makefile = makefile.replace("\t../src/ui/pages/generation_page.cpp \\\n", "")
if "generation_page.cpp" in makefile:
    raise RuntimeError("generation_page.cpp remains in SDL sources")
write(makefile_path, makefile)

ui_config_path = "src/ui/ui_config.h"
ui_config = read(ui_config_path)
ui_config = ui_config.replace(
    "    // Fourteen established pages plus Phrase Core at page 14.\n"
    "    // Keeping the expression explicit documents the additive integration.",
    "    // Page ids remain 0..14 for persisted compatibility. Page 11 is a\n"
    "    // retired GENERATION redirect; Phrase Core remains page 14.",
)
write(ui_config_path, ui_config)

# ---------------------------------------------------------------------------
# Three visible GENERATE pages; generation destination ownership moves to SONG.
# ---------------------------------------------------------------------------
for path, old, new in (
    ("src/ui/pages/genre_page.cpp", "GENRE 1/4", "GENRE 1/3"),
    ("src/ui/pages/feel_page.cpp", "FEEL 2/4", "FEEL 2/3"),
    ("src/ui/pages/texture_page.cpp", "TEXTURE 4/4", "TEXTURE 3/3"),
):
    text = read(path)
    if old not in text:
        raise RuntimeError(f"axis label missing in {path}: {old}")
    write(path, text.replace(old, new))

help_path = "src/ui/global_help_content.h"
help_text = read(help_path)
help_text = help_text.replace("GENRE 1/4", "GENRE 1/3")
help_text = help_text.replace("FEEL 2/4", "FEEL 2/3")
help_text = help_text.replace("TEXTURE 4/4", "TEXTURE 3/3")
help_text = help_text.replace(
    '    "G           Generate/materialize cell",\n'
    '    "G x2        Generate current row",\n'
    '    "Alt+G       Generate selection",\n',
    '    "G           Generate selected cell",\n'
    '    "Fn+G        Generate current row",\n'
    '    "Alt+G       Generate selection",\n',
)
help_text, removed = re.subn(
    r"\nconstexpr const char\* kGenerationLines\[\] = \{.*?\n\};\n",
    "\n",
    help_text,
    count=1,
    flags=re.DOTALL,
)
if removed != 1:
    raise RuntimeError("generation help block not removed")
help_text = help_text.replace(
    "        case WorkflowPages::kGeneration:\n"
    "            count = sizeof(kGenerationLines) / sizeof(kGenerationLines[0]); return kGenerationLines;\n",
    "",
)
if "kGenerationLines" in help_text or "GENERATION 3/4" in help_text:
    raise RuntimeError("retired generation help remains")
write(help_path, help_text)

song_path = "src/ui/pages/song_page.cpp"
song = read(song_path)
old_g_block = '''  if (lowerKey == 'g') {
    if (ui_event.ctrl) {
        // Ctrl+G - Cycle Mode
        cycleGeneratorMode();
        show_genre_hint_ = true;
        hint_timer_ = millis() + 2000;
        return true;
    } else if (ui_event.alt && has_selection_) {
        // Alt+G with selection - Batch generate
        int min_row, max_row, min_track, max_track;
        getSelectionBounds(min_row, max_row, min_track, max_track);
        int maxCol = maxEditableTrackColumn();
        if (max_track > maxCol) max_track = maxCol;
        for (int r = min_row; r <= max_row; ++r) {
            for (int t = min_track; t <= max_track; ++t) {
                bool valid = false;
                SongTrack track = trackForColumn(t, valid);
                if (!valid) continue;
                // Each cell remains an independent copy-on-write mutation.
                int savedRow = cursor_row_;
                int savedTrack = cursor_track_;
                cursor_row_ = r;
                cursor_track_ = t;
                generateCurrentCellPattern();
                cursor_row_ = savedRow;
                cursor_track_ = savedTrack;
            }
        }
        return true;
    } else {
        // G - Generate
        // Check for double tap
        uint32_t now = millis();
        if (last_g_press_ != 0 && now - last_g_press_ < 300) {
            // Double tap is one logical row mutation. Undo the provisional
            // single-cell result before preparing the row transaction.
            if (rollbackPendingCellGeneration(cursorRow())) {
                generateEntireRow();
            } else {
                showToast("GENERATION FAILED", 1200);
            }
            last_g_press_ = 0;
        } else {
            // The first tap is committed immediately for responsive hardware
            // feedback, but its receipt allows an exact revision/data rollback
            // if a second tap turns the gesture into row generation.
            if (generateCurrentCellPattern(true)) {
                last_g_press_ = now;
            }
        }
        return true;
    }
  }
'''
new_g_block = '''  if (lowerKey == 'g') {
    if (ui_event.ctrl) {
        // Ctrl+G: choose the generation algorithm without writing Song data.
        cycleGeneratorMode();
        show_genre_hint_ = true;
        hint_timer_ = millis() + 2000;
        return true;
    }

    if (ui_event.meta && !ui_event.alt) {
        // Fn+G: explicit whole-row generation. No timing-sensitive double tap.
        last_g_press_ = 0;
        return generateEntireRow();
    }

    if (ui_event.alt) {
        if (!has_selection_) {
            showToast("SELECT AREA FIRST", 1000);
            return true;
        }

        int min_row, max_row, min_track, max_track;
        getSelectionBounds(min_row, max_row, min_track, max_track);
        int maxCol = maxEditableTrackColumn();
        if (max_track > maxCol) max_track = maxCol;
        for (int r = min_row; r <= max_row; ++r) {
            for (int t = min_track; t <= max_track; ++t) {
                bool valid = false;
                (void)trackForColumn(t, valid);
                if (!valid) continue;
                // Each selected cell remains an independent copy-on-write mutation.
                int savedRow = cursor_row_;
                int savedTrack = cursor_track_;
                cursor_row_ = r;
                cursor_track_ = t;
                generateCurrentCellPattern(false);
                cursor_row_ = savedRow;
                cursor_track_ = savedTrack;
            }
        }
        return true;
    }

    // Plain G always targets exactly the selected A/B/DR cell.
    last_g_press_ = 0;
    return generateCurrentCellPattern(false);
  }
'''
if old_g_block not in song:
    raise RuntimeError("Song G handler anchor not found")
song = song.replace(old_g_block, new_g_block, 1)
song = song.replace(
    '"Q-I:P G:GEN B:BNK V:LANE X:SPLIT"',
    '"G:CELL FN+G:ROW A+G:AREA"',
)
if "rollbackPendingCellGeneration(cursorRow())" in song:
    raise RuntimeError("double-tap row gesture still active")
for required in (
    "if (ui_event.meta && !ui_event.alt)",
    "return generateEntireRow();",
    "SELECT AREA FIRST",
    "return generateCurrentCellPattern(false);",
    "G:CELL FN+G:ROW A+G:AREA",
):
    if required not in song:
        raise RuntimeError(f"Song generation UX missing: {required}")
write(song_path, song)

# Retire source files only after all integration references are removed.
for retired in (
    "src/ui/pages/generation_page.cpp",
    "src/ui/pages/generation_page.h",
):
    path = ROOT / retired
    if path.exists():
        path.unlink()

for helper in (
    "scripts/apply_axis_hardware_feedback_fixes.py",
    "scripts/prepare_axis_hardware_feedback_helper.py",
):
    path = ROOT / helper
    if path.exists():
        path.unlink()

# ---------------------------------------------------------------------------
# Regression contracts.
# ---------------------------------------------------------------------------
write("tests/test_four_axis_ui_source_regressions.py", r'''#!/usr/bin/env python3
"""Ownership gate for the three-page GENERATE workflow and Song generation."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


genre = read("src/ui/pages/genre_page.cpp")
feel = read("src/ui/pages/feel_page.cpp")
texture = read("src/ui/pages/texture_page.cpp")
song = read("src/ui/pages/song_page.cpp")
workflow = read("src/ui/workflow_mode.h")
session = read("src/state/ui_session_state.h")
help_text = read("src/ui/global_help_content.h")
display = read("src/ui/miniacid_display.cpp")
makefile = read("platform_sdl/Makefile")

for retired in ("generation_page.h", "generation_page.cpp"):
    require(not (ROOT / "src/ui/pages" / retired).exists(),
            f"retired GENERATION source remains: {retired}")

for text, token, owner in (
    (genre, '"GENRE 1/3"', "GENRE"),
    (feel, '"FEEL 2/3"', "FEEL"),
    (texture, '"TEXTURE 3/3"', "TEXTURE"),
):
    require(token in text, f"{owner} visible page count is stale")

require("case WorkflowMode::Generate: return 3;" in workflow,
        "GENERATE must expose exactly three pages")
require("kGenre, kFeel, kTexture" in workflow,
        "GENERATE order must be GENRE -> FEEL -> TEXTURE")
require("case Workspace::Generation: return kArrange;" in workflow,
        "legacy Workspace::Generation must redirect to SONG")
require("case kGeneration: return Workspace::Arrange;" in workflow,
        "legacy page 11 must resolve as SONG workspace")
require("page == kGeneration" not in workflow.split(
            "inline bool isGenerateWorkflowPage", 1)[1].split("}", 1)[0],
        "retired page 11 still belongs to GENERATE")

require("case SessionWorkflow::Generate: return 3;" in session,
        "persisted GENERATE count must be three")
require("if (state.activePage == SessionPages::kGeneration)" in session,
        "legacy active page migration is missing")
require("if (page == SessionPages::kGeneration) page = SessionPages::kArrange;" in session,
        "runtime page 11 migration is missing")

require("retired GENERATION page -> SONG" in display,
        "display transition does not redirect page 11")
for forbidden in ("generation_page.h", "GenerationPage", "generation_page.cpp"):
    require(forbidden not in display + makefile,
            f"retired GENERATION integration remains: {forbidden}")

for token in (
    "if (ui_event.meta && !ui_event.alt)",
    "return generateEntireRow();",
    "SELECT AREA FIRST",
    "return generateCurrentCellPattern(false);",
    "G:CELL FN+G:ROW A+G:AREA",
):
    require(token in song, f"Song generation UX missing: {token}")
require("rollbackPendingCellGeneration(cursorRow())" not in song,
        "timing-sensitive double-G row gesture is still active")

for token in ("GENRE 1/3", "FEEL 2/3", "TEXTURE 3/3",
              "Fn+G        Generate current row"):
    require(token in help_text, f"Alt+H contract missing: {token}")
require("GENERATION 3/4" not in help_text and "kGenerationLines" not in help_text,
        "retired GENERATION help remains")

print("Generate/Song ownership source regressions: PASS")
''')

write("tests/test_axis_hardware_feedback_source_regressions.py", r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


song = read("src/ui/pages/song_page.cpp")
feel = read("src/ui/pages/feel_page.cpp")
texture = read("src/ui/pages/texture_page.cpp")
genre_manager = read("src/dsp/genre_manager.cpp")
ui_input = read("src/ui/ui_input.h")

for token in (
    "AtlasRuntime::hasRecipe(activeRecipe)",
    "AtlasRuntime::applyRecipe(activeRecipe",
    "GenreManager::grooveboxModeForRecipe",
    "generator.setFlavorLocal(0)",
    "genreTag * 17u + recipeTag * 5u",
):
    assert token in song, f"Song genre materialization contract missing: {token}"

for token in (
    "tape.fxEnabled = tapeOn;",
    "currentScene().feel.tapeEnabled = tapeOn;",
):
    assert token in genre_manager, f"Texture audible path missing: {token}"

for token in (
    "applyTexture(false);",
    "LIVE / ENTER REAPPLY",
    "AUDIBLE TAPE",
    "HOLD L/R:ACCEL",
):
    assert token in texture, f"Texture feedback contract missing: {token}"

for token in (
    "LIVE: offbeat playback delay",
    "NEXT GEN: note timing spread",
    "NEXT GEN: note velocity spread",
    "HOLD L/R:ACCEL",
):
    assert token in feel, f"FEEL causality contract missing: {token}"

for token in ("class HoldAccelerator", "streak_ >= 10", "streak_ >= 4"):
    assert token in ui_input, f"Hold acceleration missing: {token}"

for token in (
    "if (ui_event.meta && !ui_event.alt)",
    "return generateEntireRow();",
    "SELECT AREA FIRST",
    "generateCurrentCellPattern(false)",
    "G:CELL FN+G:ROW A+G:AREA",
):
    assert token in song, f"Song generation control missing: {token}"

assert "rollbackPendingCellGeneration(cursorRow())" not in song, (
    "double-tap G must not remain the primary whole-row gesture"
)

print("Axis hardware feedback source regressions: PASS")
''')

write("tests/test_song_generation_source_regressions.py", r'''#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def main() -> None:
    song_page = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    materializer = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")

    cell = function_body(song_page, "bool SongPage::generateCurrentCellPattern(")
    row = function_body(song_page, "bool SongPage::generateEntireRow()")
    adapter = function_body(
        song_page,
        "SongPatternMaterializer::Result SongPage::materializeSongTracks")

    require("qwertyToPatternIndex(key)" in song_page,
            "Q..I assignment no longer uses the canonical pattern mapping")
    require("return assignPattern(patternIdx);" in song_page,
            "Q..I no longer assigns existing Song references")
    require("GROOVEPUTER_APP_EVENT_COPY" in song_page and
            "GROOVEPUTER_APP_EVENT_PASTE" in song_page,
            "Song copy/paste paths disappeared")

    require("materializeSongTracks(row, trackMask)" in cell,
            "plain G no longer materializes the selected track")
    require("kEditableTrackMask" in row,
            "Fn+G no longer materializes all editable row tracks")
    require("GEN %s -> %s" in cell,
            "single-cell generation toast lost destination reference")
    require("GENERATED ROW %d" in row,
            "row generation toast is missing")
    require("NO EMPTY PATTERN SLOTS" in cell and
            "NO EMPTY PATTERN SLOTS" in row,
            "no-free-slot error is not surfaced")

    require("if (ui_event.meta && !ui_event.alt)" in song_page and
            "return generateEntireRow();" in song_page,
            "Fn+G explicit row generation is missing")
    require("SELECT AREA FIRST" in song_page,
            "Alt+G without selection is not explained")
    require("return generateCurrentCellPattern(false);" in song_page,
            "plain G does not have an explicit cell-only path")
    require("rollbackPendingCellGeneration(cursorRow())" not in song_page,
            "timing-sensitive double-G row gesture remains active")

    combined = adapter + cell + row
    require(re.search(r"\b(?:s?rand)\s*\(", combined) is None,
            "Song generation path still uses rand()/srand()")
    require("GrooveboxModeManager generator(mini_acid_)" in adapter,
            "Song generation bypasses the production generator")
    require("setModeLocal" in adapter and "setFlavorLocal" in adapter,
            "Song generation does not inherit current mode/flavor")
    require("getCompiledGenerativeParams" in adapter and
            "getBehavior" in adapter,
            "Song generation does not use current genre constraints")
    require("withRuntimeAudioGuard" in adapter,
            "Song materialization commit is not protected by the audio guard")

    require("globalPatternIsReferenced" in materializer,
            "copy-on-write does not inspect Song references")
    require("slotContentIsEmpty" in materializer,
            "free-slot allocation does not inspect destination content")
    require("PreparedMaterial prepared{}" in materializer,
            "row generation lost its fixed-size preparation buffer")
    require(materializer.index("commitPrepared([&]()") >
            materializer.index("generateTrack("),
            "materializer writes before all generation is prepared")
    require(materializer.count("markSceneMutated()") == 1,
            "materializer must own exactly one successful dirty mutation")

    print("Song generation source regressions passed")


if __name__ == "__main__":
    main()
''')

scene_test_path = "tests/test_scene_revision_source_regressions.py"
scene_test = read(scene_test_path)
scene_test = scene_test.replace(
    '    generation_header = (ROOT / "src/ui/pages/generation_page.h").read_text(encoding="utf-8")\n'
    '    generation_source = (ROOT / "src/ui/pages/generation_page.cpp").read_text(encoding="utf-8")\n',
    '    materializer = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")\n',
)
scene_test = re.sub(
    r"    generation_success = generation_source\.index\(\"if \(result\) \{\"\)\n"
    r"    generation_failure = generation_source\.index\(\"\} else \{\", generation_success\)\n"
    r"    require\(\"markSceneMutated\(\);\" in generation_source\[generation_success:generation_failure\],\n"
    r"            \"successful GENERATION materialization must reach the tracker\"\)\n",
    '    require(materializer.count("markSceneMutated()") == 1,\n'
    '            "Song materializer must own one successful dirty mutation")\n',
    scene_test,
    count=1,
)
if "generation_source" in scene_test or "generation_header" in scene_test:
    raise RuntimeError("scene revision test still reads retired GENERATION source")
write(scene_test_path, scene_test)

write("tests/test_global_help_content.cpp", r'''#include <cassert>
#include <cstring>
#include <iostream>

#include "src/ui/global_help_content.h"

namespace {

bool sectionContains(int page, const char* needle) {
    const int pageLines = HelpContent::getPageLineCount(page);
    for (int i = 0; i < pageLines; ++i) {
        const char* line = HelpContent::getLine(page, i);
        if (line && std::strstr(line, needle)) return true;
    }
    return false;
}

bool globalContains(const char* needle) {
    const int globalCount = static_cast<int>(sizeof(HelpContent::kGlobalLines) /
                                             sizeof(HelpContent::kGlobalLines[0]));
    for (int i = 0; i < globalCount; ++i) {
        if (std::strstr(HelpContent::kGlobalLines[i], needle)) return true;
    }
    return false;
}

}  // namespace

int main() {
    constexpr int kFirstPage = WorkflowPages::kGenre;
    constexpr int kLastPage = WorkflowPages::kPhrase;

    for (int page = kFirstPage; page <= kLastPage; ++page) {
        if (page == WorkflowPages::kGeneration) continue;
        const int pageLineCount = HelpContent::getPageLineCount(page);
        assert(pageLineCount > 0);
        assert(std::strcmp(HelpContent::pageTitle(page), "PAGE") != 0);

        const char* first = HelpContent::getLine(page, 0);
        assert(first != nullptr);
        assert(std::strncmp(first, "===", 3) == 0);

        const int total = HelpContent::getTotalLines(page);
        assert(total > pageLineCount);
        assert(HelpContent::getLine(page, total - 1) != nullptr);
        assert(HelpContent::getLine(page, total) == nullptr);

        for (int line = 0; line < total; ++line) {
            const char* text = HelpContent::getLine(page, line);
            assert(text != nullptr);
            assert(std::strlen(text) <= 38u);
        }
    }

    assert(HelpContent::getPageLineCount(WorkflowPages::kGeneration) == 0);
    assert(globalContains("Alt+H"));
    assert(globalContains("Fn+M"));

    assert(sectionContains(WorkflowPages::kArrange, "Generate selected cell"));
    assert(sectionContains(WorkflowPages::kArrange, "Fn+G"));
    assert(sectionContains(WorkflowPages::kArrange, "Generate selection"));
    assert(sectionContains(WorkflowPages::kPhrase, "PHRASE CORE"));
    assert(sectionContains(WorkflowPages::kPhrase, "Mutable pattern references"));
    assert(sectionContains(WorkflowPages::kPerform, "PERFORMANCE TOOLS"));
    assert(sectionContains(WorkflowPages::kPlayer, "Physical track mute"));

    assert(sectionContains(WorkflowPages::kGenre, "GENRE 1/3"));
    assert(sectionContains(WorkflowPages::kFeel, "FEEL 2/3"));
    assert(sectionContains(WorkflowPages::kTexture, "TEXTURE 3/3"));
    assert(!sectionContains(WorkflowPages::kTexture, "GENERATION"));

    std::cout << "global help content tests passed\n";
    return 0;
}
''')

write("tests/test_ui_session_state.cpp", r'''#include <cassert>

#include "src/state/ui_session_state.h"

using namespace GroovePuterState;

int main() {
    UiSessionState state = defaultUiSessionState();
    sanitizeUiSessionState(state);

    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kGenre);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Song) ==
           SessionPages::kArrange);

    assert(pageCountForWorkflow(SessionWorkflow::Generate) == 3);
    assert(pageAt(SessionWorkflow::Generate, 0) == SessionPages::kGenre);
    assert(pageAt(SessionWorkflow::Generate, 1) == SessionPages::kFeel);
    assert(pageAt(SessionWorkflow::Generate, 2) == SessionPages::kTexture);
    assert(pageCountForWorkflow(SessionWorkflow::Song) == 2);
    assert(pageAt(SessionWorkflow::Song, 0) == SessionPages::kArrange);
    assert(pageAt(SessionWorkflow::Song, 1) == SessionPages::kPhrase);

    rememberWorkflowPage(state, SessionPages::kFeel);
    rememberWorkflowPage(state, SessionPages::kPhrase);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kFeel);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Song) ==
           SessionPages::kPhrase);

    assert(workflowNavigationTarget(
               state, SessionPages::kGenre, 1, false) == SessionPages::kFeel);
    assert(workflowNavigationTarget(
               state, SessionPages::kFeel, 1, false) == SessionPages::kTexture);
    assert(workflowNavigationTarget(
               state, SessionPages::kTexture, 1, false) == SessionPages::kGenre);
    assert(workflowNavigationTarget(
               state, SessionPages::kArrange, 1, false) == SessionPages::kPhrase);

    UiSessionState legacy = defaultUiSessionState();
    legacy.activePage = static_cast<int8_t>(SessionPages::kGeneration);
    legacy.lastPageByWorkflow[workflowSessionIndex(SessionWorkflow::Generate)] =
        static_cast<int8_t>(SessionPages::kGeneration);
    sanitizeUiSessionState(legacy);
    assert(legacy.activePage == SessionPages::kArrange);
    assert(rememberedWorkflowPage(legacy, SessionWorkflow::Generate) ==
           SessionPages::kGenre);
    assert(rememberedWorkflowPage(legacy, SessionWorkflow::Song) ==
           SessionPages::kArrange);

    rememberWorkflowPage(legacy, SessionPages::kGeneration);
    assert(legacy.activePage == SessionPages::kArrange);
    assert(rememberedWorkflowPage(legacy, SessionWorkflow::Song) ==
           SessionPages::kArrange);

    legacy.activePage = 99;
    legacy.visualStyle = 1;
    legacy.waveformOverlayEnabled = 7;
    legacy.masterVolumePermille = 60000;
    sanitizeUiSessionState(legacy);
    assert(legacy.activePage == SessionPages::kGenre);
    assert(legacy.visualStyle == 0);
    assert(legacy.waveformOverlayEnabled == 1);
    assert(legacy.masterVolumePermille == kMaxMasterVolumePermille);

    assert(masterVolumeToPermille(0.6f) == 600);
    assert(masterVolumeToPermille(2.5f) == 1800);

    UiSessionState copy = legacy;
    assert(copy == legacy);
    copy.activePage = SessionPages::kPerform;
    assert(copy != legacy);

    return 0;
}
''')

# ---------------------------------------------------------------------------
# Dedicated CI and concise hardware runbook.
# ---------------------------------------------------------------------------
write(".github/workflows/four-axis-ui.yml", r'''name: Generate and Song UI

on:
  push:
    branches:
      - dev
      - agent/song-generation-ux
    paths:
      - 'src/ui/pages/genre_page.*'
      - 'src/ui/pages/feel_page.*'
      - 'src/ui/pages/texture_page.*'
      - 'src/ui/pages/song_page.*'
      - 'src/ui/miniacid_display.*'
      - 'src/ui/workflow_mode.h'
      - 'src/ui/global_help_content.h'
      - 'src/state/ui_session_state.h'
      - 'src/dsp/song_pattern_materializer.h'
      - 'tests/test_four_axis_ui_source_regressions.py'
      - 'tests/test_axis_hardware_feedback_source_regressions.py'
      - 'tests/test_song_generation_source_regressions.py'
      - 'tests/test_scene_revision_source_regressions.py'
      - 'tests/test_global_help_content.cpp'
      - 'tests/test_ui_session_state.cpp'
      - 'docs/stages/SONG_GENERATION_UX.md'
      - '.github/workflows/four-axis-ui.yml'
  pull_request:
    paths:
      - 'src/ui/pages/genre_page.*'
      - 'src/ui/pages/feel_page.*'
      - 'src/ui/pages/texture_page.*'
      - 'src/ui/pages/song_page.*'
      - 'src/ui/miniacid_display.*'
      - 'src/ui/workflow_mode.h'
      - 'src/ui/global_help_content.h'
      - 'src/state/ui_session_state.h'
      - 'src/dsp/song_pattern_materializer.h'
      - 'tests/test_four_axis_ui_source_regressions.py'
      - 'tests/test_axis_hardware_feedback_source_regressions.py'
      - 'tests/test_song_generation_source_regressions.py'
      - 'tests/test_scene_revision_source_regressions.py'
      - 'tests/test_global_help_content.cpp'
      - 'tests/test_ui_session_state.cpp'
      - 'docs/stages/SONG_GENERATION_UX.md'
      - '.github/workflows/four-axis-ui.yml'

jobs:
  host-contract:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check ownership and generation contracts
        run: |
          python3 tests/test_four_axis_ui_source_regressions.py
          python3 tests/test_axis_hardware_feedback_source_regressions.py
          python3 tests/test_song_generation_source_regressions.py
          python3 tests/test_scene_revision_source_regressions.py

      - name: Build active GENERATE pages and Song
        run: |
          mkdir -p build/host-tests/generate-song
          COMMON_FLAGS='-std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h'
          g++ ${COMMON_FLAGS} -c src/ui/pages/genre_page.cpp -o build/host-tests/generate-song/genre_page.o
          g++ ${COMMON_FLAGS} -c src/ui/pages/feel_page.cpp -o build/host-tests/generate-song/feel_page.o
          g++ ${COMMON_FLAGS} -c src/ui/pages/texture_page.cpp -o build/host-tests/generate-song/texture_page.o
          g++ ${COMMON_FLAGS} -c src/ui/pages/song_page.cpp -o build/host-tests/generate-song/song_page.o

      - name: Build and run navigation/help tests
        run: |
          g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_global_help_content.cpp -o build/host-tests/generate-song/test_global_help_content
          g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_ui_session_state.cpp -o build/host-tests/generate-song/test_ui_session_state
          build/host-tests/generate-song/test_global_help_content
          build/host-tests/generate-song/test_ui_session_state
''')

write("docs/stages/SONG_GENERATION_UX.md", r'''# Song generation UX

## Purpose

Remove the duplicate `GENERATION` page and make `SONG` the only owner of generation destination and scope. `GENERATE` now contains `GENRE -> FEEL -> TEXTURE`.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data/power cable;
- optional SEQTRAK for MIDI-output verification.

## Wiring

No external wiring is required. Cardputer ADV uses its built-in 240x135 display and keyboard. PORT.A remains untouched: GPIO2 SDA, GPIO1 SCL, `Wire` bus.

## Build / flash

```bash
gp-branch agent/song-generation-ux
arduino-cli compile --fqbn esp32:esp32:esp32s3 GroovePuter.ino
arduino-cli upload --fqbn esp32:esp32:esp32s3 -p /dev/ttyACM0 GroovePuter.ino
```

Use the repository's pinned board/profile command instead when it differs from the generic example above.

## Expected behavior

- GENERATE cycles through `GENRE`, `FEEL`, `TEXTURE`; no `GENERATION` page appears.
- A saved session that points to retired page 11 opens `SONG` safely.
- On `SONG`, arrows choose row and `A/B/DR` cell.
- `G` generates only the selected cell.
- `Fn+G` generates the whole current row.
- `Alt+G` generates the selected rectangle; without a selection it shows `SELECT AREA FIRST`.
- `Ctrl+G` changes the generator mode without writing Song data.
- TEXTURE still affects GroovePuter's internal audio engine only; no SEQTRAK CC/SysEx texture mapping is added.

## Troubleshooting

- Old GENERATION page after flashing: clean build artifacts and confirm the branch SHA.
- `Fn+G` does nothing: inspect Serial for `fn=1` on the `G` key event.
- `Alt+G` reports `SELECT AREA FIRST`: create a rectangular selection with Ctrl/Shift plus arrows first.
- `NO EMPTY PATTERN SLOTS`: free a pattern slot or switch pattern page/bank.

## Acceptance checklist

- [ ] Page order is `GENRE -> FEEL -> TEXTURE` in both themes.
- [ ] Retired page 11 redirects to SONG after restoring an older session.
- [ ] `G` changes one selected A/B/DR cell only.
- [ ] `Fn+G` changes all three editable cells in one row.
- [ ] `Alt+G` changes only the selected area.
- [ ] `Ctrl+G` changes mode but does not alter Song references.
- [ ] Serial shows no invalid page creation for page 11.
- [ ] USB MIDI note routing remains unchanged.
''')

old_doc = ROOT / "docs/stages/FOUR_AXIS_GENERATE_UI.md"
if old_doc.exists():
    old_text = old_doc.read_text(encoding="utf-8")
    notice = (
        "> Superseded for runtime navigation: GENERATION was retired as a separate page. "
        "Current acceptance is in `docs/stages/SONG_GENERATION_UX.md`.\n\n"
    )
    if not old_text.startswith("> Superseded for runtime navigation"):
        old_doc.write_text(notice + old_text, encoding="utf-8")

print("Song generation UX refactor applied")
