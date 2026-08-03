#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def replace_once(data: bytes, old: bytes, new: bytes, path: str) -> bytes:
    count = data.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one byte anchor, found {count}: {old!r}")
    return data.replace(old, new, 1)


def fix_control_literals(path: str) -> None:
    file_path = ROOT / path
    data = file_path.read_bytes()
    data = data.replace(b"'\x00'", b"'\\0'")
    data = data.replace(b"'\x08'", b"'\\b'")
    data = data.replace(b"'\t'", b"'\\t'")
    data = data.replace(b"'\r'", b"'\\r'")
    data = data.replace(b"'\n'", b"'\\n'")
    data = data.replace(
        b"if (ui_event.key == '\\n' || ui_event.key == '\\n')",
        b"if (ui_event.key == '\\n' || ui_event.key == '\\r')",
    )
    file_path.write_bytes(data)


def harden_player_path() -> None:
    path = "src/platform/cardputer_smf_player.cpp"
    file_path = ROOT / path
    text = file_path.read_text(encoding="utf-8")
    function_start = text.index("bool CardputerSmfPlayerService::loadFile(const char* path) {")
    function_end = text.index("\nbool CardputerSmfPlayerService::scanMetadata()", function_start)
    prefix = text[:function_start]
    function = text[function_start:function_end]
    suffix = text[function_end:]

    open_anchor = '''    if (!source_.open(path)) {
        publishSnapshot(SmfPlayerState::Error, "Cannot open MIDI");
        return false;
    }
'''
    open_replacement = '''    if (!source_.open(path)) {
        publishSnapshot(SmfPlayerState::Error, "Cannot open MIDI");
        return false;
    }
    portENTER_CRITICAL(&snapshotMux_);
    copyText(loadedPath_, sizeof(loadedPath_), path);
    portEXIT_CRITICAL(&snapshotMux_);
'''
    if function.count(open_anchor) != 1:
        raise RuntimeError("loadFile open anchor missing")
    function = function.replace(open_anchor, open_replacement, 1)

    failure_pattern = re.compile(r"(?P<indent>\s*)source_\.close\(\);\n(?P=indent)return false;")

    def failure_replacement(match: re.Match[str]) -> str:
        indent = match.group("indent")
        return (
            f"{indent}source_.close();\n"
            f"{indent}portENTER_CRITICAL(&snapshotMux_);\n"
            f"{indent}loadedPath_[0] = '\\0';\n"
            f"{indent}portEXIT_CRITICAL(&snapshotMux_);\n"
            f"{indent}return false;"
        )

    function, failure_count = failure_pattern.subn(failure_replacement, function)
    if failure_count < 4:
        raise RuntimeError(f"expected multiple guarded SMF scan failures, got {failure_count}")
    file_path.write_text(prefix + function + suffix, encoding="utf-8")


def fix_player_back_navigation() -> None:
    path = ROOT / "src/ui/pages/smf_player_page.cpp"
    text = path.read_text(encoding="utf-8")
    old = '''        if (result == GroovePuterUi::MidiFileManager::EventResult::CloseRequested) {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                if (state.state != SmfPlayerState::Unloaded &&
                    state.state != SmfPlayerState::Error) {
                    browserVisible_ = false;
                }
            }
            return true;
        }
'''
    new = '''        if (result == GroovePuterUi::MidiFileManager::EventResult::CloseRequested) {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                if (state.state != SmfPlayerState::Unloaded &&
                    state.state != SmfPlayerState::Error) {
                    browserVisible_ = false;
                    return true;
                }
            }
            return false;
        }
'''
    if text.count(old) != 1:
        raise RuntimeError("SMF browser close anchor missing")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def strengthen_regression() -> None:
    path = ROOT / "tests/test_midi_file_manager_source_regressions.py"
    text = path.read_text(encoding="utf-8")
    anchor = '''    require("SD.remove" not in player_cpp and "SD.rename" not in player_cpp,
            "SMF Player must not perform file mutations directly")
'''
    addition = '''    for source_path in (
            ROOT / "src/ui/pages/project_page.cpp",
            ROOT / "src/ui/pages/smf_player_page.cpp"):
        data = source_path.read_bytes()
        require(b"\\x00" not in data and b"\\x08" not in data,
                f"{source_path.name} contains a raw control byte")
        require(b"'\\n'" not in data and b"'\\r'" not in data,
                f"{source_path.name} contains a raw line break in a char literal")

    service_cpp = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8")
    require("currentFilePath" in manager_cpp and "loadedPath_" in service_cpp,
            "exact active SMF path protection is missing")
    require(service_cpp.count("loadedPath_[0] = '\\\\0';") >= 2,
            "failed SMF scans must release the protected path")
'''
    if text.count(anchor) != 1:
        raise RuntimeError("source regression extension anchor missing")
    path.write_text(text.replace(anchor, anchor + addition, 1), encoding="utf-8")


def main() -> None:
    fix_control_literals("src/ui/pages/project_page.cpp")
    fix_control_literals("src/ui/pages/smf_player_page.cpp")
    harden_player_path()
    fix_player_back_navigation()
    strengthen_regression()


if __name__ == "__main__":
    main()
