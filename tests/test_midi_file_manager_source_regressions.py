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
    sdl_makefile = (ROOT / "platform_sdl/Makefile").read_text(encoding="utf-8")

    require("kWindowEntries = 8" in manager_h and
            "static_assert(sizeof(MidiFileManager) <= 1024" in manager_h,
            "MIDI manager paged RAM contract is missing")
    require("scanDirectorySummary" in manager_h and "loadWindow" in manager_h,
            "browser capacity must limit memory, not file reachability")
    require("opendir" in manager_cpp and "readdir" in manager_cpp and
            "rewinddir" in manager_cpp and "openNextFile" not in manager_cpp,
            "browser traversal must avoid one heap-backed File per directory entry")
    require("secondDirectoryCount != directoryCount_" in manager_cpp and
            "secondFileCount != fileCount_" in manager_cpp and
            "read incomplete" in manager_cpp,
            "partial directory traversal must not masquerade as an empty list")
    require("union Workspace" in manager_h and
            "EntryWindow entries" in manager_h and
            "MidiImporter::ScanResult importScan" in manager_h and
            "MIDI import scan must fit in the browser workspace" in manager_h,
            "browser rows and import scan must share one bounded workspace")
    require("Mode::ConfirmDelete" in manager_cpp,
            "MIDI delete confirmation mode is missing")
    require("deleteConfirmed_ = false" in manager_cpp,
            "MIDI delete confirmation must default to NO")
    require("SD.rename" in manager_cpp and "SD.remove" in manager_cpp,
            "MIDI rename/delete must be owned by the shared manager")
    require("selectedFileIsInUse" in manager_cpp and "smfPlayerService" in manager_cpp,
            "active SMF file mutation guard is missing")
    require("../src/ui/midi_file_manager.cpp" in sdl_makefile,
            "desktop build must link the shared MIDI manager")

    require("parentCount > 0 && windowStart_ == 0 && windowCount_ > 0" in manager_cpp and
            "workspace_.entries[0].kind = EntryKind::Parent" in manager_cpp and
            'sizeof(workspace_.entries[0].name), ".."' in manager_cpp,
            "initial nested-folder window must materialize the synthetic parent row")
    require("event.scancode == GROOVEPUTER_UP" in manager_cpp and
            "event.scancode == GROOVEPUTER_DOWN" in manager_cpp,
            "MIDI browser must consume repeated canonical arrow events")
    require("serviceHeldNavigation" not in manager_h and
            "serviceHeldNavigation" not in manager_cpp and
            "heldNavigationDirectionFromCardputer" not in manager_cpp and
            "M5Cardputer.h" not in manager_cpp,
            "MIDI browser must not duplicate the central Cardputer repeat path")

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
    require("midi_scan_" not in project_h and
            "midiFileManager().beginImportScan()" in project_cpp and
            "midiFileManager().importScanResult()" in project_cpp,
            "Project import scan must reuse the shared manager workspace")
    require("static_assert(sizeof(ProjectPage) <= 256" in project_h,
            "Project page DRAM budget must protect directory-open headroom")

    constructor = project_cpp[
        project_cpp.index("ProjectPage::ProjectPage"):
        project_cpp.index("void ProjectPage::refreshScenes")
    ]
    on_enter = project_cpp[
        project_cpp.index("void ProjectPage::onEnter"):
        project_cpp.index("bool ProjectPage::importMidiAtSelection")
    ]
    require("refreshScenes()" not in constructor and
            "generateMemorableName()" not in constructor and
            "refreshScenes()" not in on_enter,
            "Project page construction must not eagerly allocate scene data")
    require("returnToMidiBrowser" in project_cpp and
            "midiFileManager().open()" in project_cpp,
            "leaving MIDI routing must reconstruct the overlaid browser window")
    require("MIDI FOLDER OPEN FAILED" in manager_cpp and
            "MIDI FOLDER READ FAILED" in manager_cpp and
            "[MIDI-FILES]" in manager_cpp and "open failed" in manager_cpp,
            "directory allocation failure must not be mislabeled as missing SD")

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
