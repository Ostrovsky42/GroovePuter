#!/usr/bin/env python3
from pathlib import Path
import re

helper_path = Path('scripts/apply_axis_hardware_feedback_fixes.py')
helper = helper_path.read_text(encoding='utf-8')

# The actual Song function declares trackLabel between the buffer and snprintf.
# Remove the old formatting-dependent helper operation; patch the C++ block
# structurally below instead.
start_marker = "replace_once(\n    song,\n    '    char message[48];\\n"
end_marker = "\nreplace_once(\n    song,\n    '    char message[32];\\n"
start = helper.find(start_marker)
end = helper.find(end_marker, start + 1)
if start < 0 or end < 0:
    raise SystemExit('single-cell Song toast helper operation not found')
helper = helper[:start] + helper[end + 1:]

# Keep the stable row-toast prefix while appending genre/variant context.
old_row_format = '"ROW %d %s/%s"'
new_row_format = '"GENERATED ROW %d %s/%s"'
if old_row_format not in helper:
    raise SystemExit('row Song toast helper format not found')
helper_path.write_text(
    helper.replace(old_row_format, new_row_format, 1), encoding='utf-8')

song_path = Path('src/ui/pages/song_page.cpp')
song = song_path.read_text(encoding='utf-8')
pattern = re.compile(
    r'    char patternLabel\[12\];\n'
    r'    formatSongPatternLabel\(.*?'
    r'    showToast\(message, 1100\);',
    re.DOTALL,
)
replacement = '''    char patternLabel[12];
    formatSongPatternLabel(
        result.globalPattern[trackIndex], patternLabel, sizeof(patternLabel));
    const char* trackLabel = track == SongTrack::SynthA
        ? "A"
        : track == SongTrack::SynthB ? "B" : "DR";
    char message[96];
    std::snprintf(
        message, sizeof(message), "GEN %s -> %s %s/%s",
        trackLabel, patternLabel,
        GenreManager::generativeModeName(
            mini_acid_.genreManager().generativeMode()),
        GenreManager::recipeName(mini_acid_.genreManager().recipe()));
    showToast(message, 1400);'''
song, count = pattern.subn(replacement, song, count=1)
if count != 1:
    raise SystemExit('single-cell Song toast C++ block not found')
song_path.write_text(song, encoding='utf-8')

# Hardware feedback made the page copy more explicit. Keep the ownership gate
# semantic rather than pinning it to the superseded wording.
test_path = Path('tests/test_four_axis_ui_source_regressions.py')
test = test_path.read_text(encoding='utf-8')
replacements = (
    ('\'"FORM / DEVELOPMENT"\'', '\'"WRITE ONE SONG BAR"\''),
    ('require(GENERATION, "Phrase length owned by PHRASE CORE",\n'
     '        "GENERATION must disclose the cross-workflow length owner")',
     'require(GENERATION, "ROW OCCUPIED:BLOCK  LEN:PHRASE",\n'
     '        "GENERATION must disclose row and phrase ownership")'),
    ('\'"SOUND SURFACE"\'', '\'"LIVE SOUND SURFACE"\''),
    ('\'"MACRO VIEW 0..127 (READ ONLY)"\'', '\'"AUDIBLE TAPE %s  DELAY %s"\''),
)
for old, new in replacements:
    if old not in test:
        raise SystemExit(f'axis gate anchor missing: {old}')
    test = test.replace(old, new, 1)
test_path.write_text(test, encoding='utf-8')

# A failed occupied-row attempt must not dirty Scene. The revision gate now
# checks the success path in the implementation instead of requiring an
# unconditional helper in the header.
revision_path = Path('tests/test_scene_revision_source_regressions.py')
revision = revision_path.read_text(encoding='utf-8')
old_read = ('    generation_header = (ROOT / "src/ui/pages/generation_page.h")'
            '.read_text(encoding="utf-8")\n')
new_read = old_read + ('    generation_source = (ROOT / "src/ui/pages/generation_page.cpp")'
                       '.read_text(encoding="utf-8")\n')
if old_read not in revision:
    raise SystemExit('generation revision source read anchor missing')
revision = revision.replace(old_read, new_read, 1)
old_gate = ('    require("markSceneMutated();" in generation_header,\n'
            '            "GENERATION materialization must reach the tracker")\n')
new_gate = ('    generation_success = generation_source.index("if (result) {")\n'
            '    generation_failure = generation_source.index("} else {", generation_success)\n'
            '    require("markSceneMutated();" in '
            'generation_source[generation_success:generation_failure],\n'
            '            "successful GENERATION materialization must reach the tracker")\n')
if old_gate not in revision:
    raise SystemExit('generation revision gate anchor missing')
revision_path.write_text(revision.replace(old_gate, new_gate, 1), encoding='utf-8')
