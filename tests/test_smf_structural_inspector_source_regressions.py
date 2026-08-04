#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stream_h = (ROOT / "src/midi/smf_stream.h").read_text(encoding="utf-8")
stream_cpp = (ROOT / "src/midi/smf_stream.cpp").read_text(encoding="utf-8")
structural_h = (ROOT / "src/midi/smf_structural_inspector.h").read_text(encoding="utf-8")
track_h = (ROOT / "src/midi/smf_track_inspector.h").read_text(encoding="utf-8")
page_h = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")
page_wrapper = (ROOT / "src/ui/pages/smf_player_page_structural.cpp").read_text(encoding="utf-8")
player_cpp = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(encoding="utf-8")
workflow = (ROOT / ".github/workflows/core-regressions.yml").read_text(encoding="utf-8")
doc = (ROOT / "docs/stages/SMF_STRUCTURAL_INSPECTOR_STAGE_1B.md").read_text(encoding="utf-8")

assert '#include "smf_structural_inspector.h"' in stream_cpp
assert 'smfStructuralInspectorState().reset(index.division, index.trackCount);' in stream_cpp
assert 'smfStructuralInspectorState().observe(out.trackIndex, out.event);' in stream_cpp
assert 'smfStructuralInspectorState().finalize();' in stream_cpp
assert 'captureStructuralAnalysis_{false}' in stream_h
assert stream_cpp.count('captureStructuralAnalysis_ = true;') == 1
assert player_cpp.count('SmfFileIndexer::build(source_)') == 1

assert 'inferSwing(source, out.gridDenominator)' in structural_h
assert 'event.data2' not in structural_h.split('const uint32_t relativeBar', 1)[1].split('observeNoteOff', 1)[0]
assert 'return fourBar ? 4u : 0u;' not in structural_h
assert 'return 4u;' in structural_h and 'return 0u;' in structural_h
assert 'static_cast<uint64_t>(source.activeTicks)' in structural_h
assert 'smfTrackInspectorState().freeze();' in structural_h
assert 'union Storage' not in structural_h
assert 'std::atomic<uint32_t> publishedWords_' in structural_h

assert 'constexpr std::size_t kSmfMaxTracks = 32;' in stream_h
assert 'uint16_t declaredTrackCount{0};' in stream_h
assert 'bool tracksTruncated() const' in stream_h
assert 'retainedTracks < kSmfMaxTracks' in stream_cpp
assert 'result.index.declaredTrackCount = foundTracks;' in stream_cpp
assert 'smfTrackInspectorState().reset(index.trackCount, index.declaredTrackCount);' in stream_cpp

assert 'constexpr std::size_t kSmfTrackInspectorMaxTracks = 32;' in track_h
assert 'std::atomic<uint16_t> state_' in track_h
assert 'std::atomic<uint16_t> declaredTrackCount_' in track_h
assert 'kFrozenBit' in track_h
assert 'if ((state & kFrozenBit) != 0u) return;' in track_h
assert 'nameWords' not in track_h
assert 'sizeof(SmfTrackInspectorState) <= 136' in track_h
assert 'programLabel(output.firstProgram)' in track_h
assert 'case 33: return "Finger Bass";' in track_h

assert 'class SmfPlayerPageBase' in page_h
assert 'class SmfPlayerPage final : public SmfPlayerPageBase' in page_h
assert 'MIDI STRUCTURE' in page_wrapper
assert 'NOTE REGISTER' in page_wrapper
assert 'ACTIVE %u%%' in page_wrapper
assert 'RESEMBLES' in page_wrapper
assert 'PARTIAL 64' in page_wrapper
assert 'LOOP --' in page_wrapper
assert 'K is intentionally not a MIDI mute command' in page_wrapper
assert '1-9 Hot ENT Sel A AllOn' in page_wrapper
assert 'TRACKS %u / %u PLAYED' in page_wrapper
assert 'tracks.tracksTruncated()' in page_wrapper
assert 'ENT/K' not in page_wrapper
assert 'TinyUSB' not in page_wrapper and 'USBMIDI' not in page_wrapper
assert 'Enter/K' not in doc
assert '`K` is not a MIDI mute command' in doc
assert 'check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf' in workflow

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
print("smf structural inspector source regressions: OK")
