#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    file_path = ROOT / path
    text = file_path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one anchor, found {count}: {old[:100]!r}")
    file_path.write_text(text.replace(old, new, 1), encoding="utf-8")


def main() -> None:
    replace_once(
        "src/ui/pages/project_page.cpp",
        '    GroovePuterUi::midiFileManager().draw(gfx, Layout::CONTENT, "IMPORT");',
        '''    const Rect midiBrowserBounds(Layout::CONTENT.x, Layout::CONTENT.y,
                                 Layout::CONTENT.w, Layout::CONTENT.h);
    GroovePuterUi::midiFileManager().draw(gfx, midiBrowserBounds, "IMPORT");''',
    )
    replace_once(
        "src/ui/pages/smf_player_page.cpp",
        '    GroovePuterUi::midiFileManager().draw(gfx, Layout::CONTENT, "PLAYER");',
        '''    const Rect midiBrowserBounds(Layout::CONTENT.x, Layout::CONTENT.y,
                                 Layout::CONTENT.w, Layout::CONTENT.h);
    GroovePuterUi::midiFileManager().draw(gfx, midiBrowserBounds, "PLAYER");''',
    )
    replace_once(
        "src/ui/midi_file_manager.cpp",
        '''    char pathLine[96];
    std::snprintf(pathLine, sizeof(pathLine), "%s  D%d F%d%s",
                  currentPath_, directoryCount_, fileCount_,
                  truncated_ ? " +" : "");''',
        '''    char pathLine[kPathBytes + 24]{};
    std::snprintf(pathLine, sizeof(pathLine), "%s  D%d F%d%s",
                  currentPath_, directoryCount_, fileCount_,
                  truncated_ ? " +" : "");''',
    )

    test_path = ROOT / "tests/test_source_regressions.py"
    test_text = test_path.read_text(encoding="utf-8")
    project_anchor = '''    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(
        encoding="utf-8"
    )
'''
    project_replacement = project_anchor + '''    midi_manager = (ROOT / "src/ui/midi_file_manager.cpp").read_text(
        encoding="utf-8"
    )
'''
    if test_text.count(project_anchor) != 1:
        raise RuntimeError("tests/test_source_regressions.py: project source anchor missing")
    test_text = test_text.replace(project_anchor, project_replacement, 1)

    old_assertion = '''    require(project.count("GROOVEPUTER_ESCAPE") >= 2 and
            "navigateUpMidiDir();" in project,
            "Escape must go to the parent MIDI directory and back from matrix")
'''
    new_assertion = '''    require("GROOVEPUTER_ESCAPE" in project and
            "dialog_type_ = DialogType::ImportMidi;" in project,
            "Escape from the routing matrix must return to MIDI browsing")
    require("GROOVEPUTER_ESCAPE" in midi_manager and
            "navigateUp()" in midi_manager and
            "EntryKind::Parent" in midi_manager,
            "the shared MIDI manager must own parent-directory navigation")
'''
    if test_text.count(old_assertion) != 1:
        raise RuntimeError("tests/test_source_regressions.py: legacy MIDI navigation assertion missing")
    test_path.write_text(test_text.replace(old_assertion, new_assertion, 1), encoding="utf-8")


if __name__ == "__main__":
    main()
