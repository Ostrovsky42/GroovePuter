from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    manager_h = (ROOT / "src/ui/midi_file_manager.h").read_text(encoding="utf-8")
    manager_cpp = (ROOT / "src/ui/midi_file_manager.cpp").read_text(encoding="utf-8")
    project_h = (ROOT / "src/ui/pages/project_page.h").read_text(encoding="utf-8")
    project_cpp = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")
    player_h = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")
    player_cpp = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(encoding="utf-8")

    require("static_assert(sizeof(MidiFileManager) <= 4096" in manager_h,
            "MIDI manager RAM contract is missing")
    require("Mode::ConfirmDelete" in manager_cpp,
            "MIDI delete confirmation mode is missing")
    require("deleteConfirmed_ = false" in manager_cpp,
            "MIDI delete confirmation must default to NO")
    require("SD.rename" in manager_cpp and "SD.remove" in manager_cpp,
            "MIDI rename/delete must be owned by the shared manager")
    require("selectedFileIsInUse" in manager_cpp and "smfPlayerService" in manager_cpp,
            "active SMF file mutation guard is missing")

    require("midi_file_manager.h" in project_h,
            "Project import must include the shared MIDI manager")
    require("midiFileManager().handleEvent" in project_cpp,
            "Project import does not route input through the shared manager")
    require("midiFileManager().draw" in project_cpp,
            "Project import does not draw the shared manager")
    require("const Rect midiBrowserBounds" in project_cpp,
            "Project import must adapt the reserved layout bounds explicitly")
    require("refreshMidiFiles" not in project_h and "midi_dirs_" not in project_h,
            "Project still owns a duplicate MIDI browser model")

    require("midi_file_manager.h" in player_h,
            "SMF Player must include the shared MIDI manager")
    require("midiFileManager().handleEvent" in player_cpp,
            "SMF Player does not route input through the shared manager")
    require("midiFileManager().draw" in player_cpp,
            "SMF Player does not draw the shared manager")
    require("const Rect midiBrowserBounds" in player_cpp,
            "SMF Player must adapt the reserved layout bounds explicitly")
    require("BrowserRow" not in player_h and "refreshFiles" not in player_h,
            "SMF Player still owns a duplicate MIDI browser model")
    require("SD.remove" not in player_cpp and "SD.rename" not in player_cpp,
            "SMF Player must not perform file mutations directly")
    for source_path in (
            ROOT / "src/ui/pages/project_page.cpp",
            ROOT / "src/ui/pages/smf_player_page.cpp"):
        data = source_path.read_bytes()
        require(b"\x00" not in data and b"\x08" not in data,
                f"{source_path.name} contains a raw control byte")
        require(b"'\n'" not in data and b"'\r'" not in data,
                f"{source_path.name} contains a raw line break in a char literal")

    service_cpp = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8")
    require("currentFilePath" in manager_cpp and "loadedPath_" in service_cpp,
            "exact active SMF path protection is missing")
    require(service_cpp.count("loadedPath_[0] = '\\0';") >= 2,
            "failed SMF scans must release the protected path")


if __name__ == "__main__":
    main()
