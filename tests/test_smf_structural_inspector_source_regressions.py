#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stream_h = (ROOT / "src/midi/smf_stream.h").read_text()
stream_cpp = (ROOT / "src/midi/smf_stream.cpp").read_text()
page_cpp = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text()
player_cpp = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text()

assert '#include "smf_structural_inspector.h"' in stream_cpp
assert 'smfStructuralInspectorState().reset(index.division, index.trackCount);' in stream_cpp
assert 'smfStructuralInspectorState().observe(out.trackIndex, out.event);' in stream_cpp
assert 'smfStructuralInspectorState().finalize();' in stream_cpp
assert 'captureStructuralAnalysis_{false}' in stream_h
assert stream_cpp.count('captureStructuralAnalysis_ = true;') == 1
assert player_cpp.count('SmfFileIndexer::build(source_)') == 1
assert 'MIDI STRUCTURE' in page_cpp
assert 'NOTE REGISTER' in page_cpp
assert 'ACTIVE %u%%' in page_cpp
assert 'RESEMBLES' in page_cpp
assert 'PARTIAL 64' in page_cpp
print('smf structural inspector source regressions: OK')
