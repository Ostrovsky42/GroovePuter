#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stream_h = (ROOT / "src/midi/smf_stream.h").read_text(encoding="utf-8")
stream_cpp = (ROOT / "src/midi/smf_stream.cpp").read_text(encoding="utf-8")
page_h = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")
page_wrapper = (ROOT / "src/ui/pages/smf_player_page_structural.cpp").read_text(encoding="utf-8")
player_cpp = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(encoding="utf-8")

assert '#include "smf_structural_inspector.h"' in stream_cpp
assert 'smfStructuralInspectorState().reset(index.division, index.trackCount);' in stream_cpp
assert 'smfStructuralInspectorState().observe(out.trackIndex, out.event);' in stream_cpp
assert 'smfStructuralInspectorState().finalize();' in stream_cpp
assert 'captureStructuralAnalysis_{false}' in stream_h
assert stream_cpp.count('captureStructuralAnalysis_ = true;') == 1
assert player_cpp.count('SmfFileIndexer::build(source_)') == 1
assert 'class SmfPlayerPageBase' in page_h
assert 'class SmfPlayerPage final : public SmfPlayerPageBase' in page_h
assert 'MIDI STRUCTURE' in page_wrapper
assert 'NOTE REGISTER' in page_wrapper
assert 'ACTIVE %u%%' in page_wrapper
assert 'RESEMBLES' in page_wrapper
assert 'PARTIAL 64' in page_wrapper
assert 'TinyUSB' not in page_wrapper and 'USBMIDI' not in page_wrapper

build_dir = ROOT / "build" / "host-tests"
build_dir.mkdir(parents=True, exist_ok=True)
binary = build_dir / "test_smf_structural_inspector"
cxx = os.environ.get("CXX", "g++")
subprocess.run([
    cxx,
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-Werror",
    f"-I{ROOT}",
    str(ROOT / "tests/test_smf_structural_inspector.cpp"),
    "-o",
    str(binary),
], check=True)
subprocess.run([str(binary)], check=True)
print('smf structural inspector source regressions: OK')
