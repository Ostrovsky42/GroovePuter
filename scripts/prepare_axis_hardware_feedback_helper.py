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
helper_path.write_text(helper[:start] + helper[end + 1:], encoding='utf-8')

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
        message, sizeof(message), "GEN %s %s/%s -> %s",
        trackLabel,
        GenreManager::generativeModeName(
            mini_acid_.genreManager().generativeMode()),
        GenreManager::recipeName(mini_acid_.genreManager().recipe()),
        patternLabel);
    showToast(message, 1400);'''
song, count = pattern.subn(replacement, song, count=1)
if count != 1:
    raise SystemExit('single-cell Song toast C++ block not found')
song_path.write_text(song, encoding='utf-8')
