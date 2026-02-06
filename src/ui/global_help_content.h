#pragma once

namespace HelpContent {

constexpr const char* kGlobalLines[] = {
    "=== GLOBAL ===",
    "[ / ]      Prev/Next page",
    "Alt/Ctrl+1..0  Jump to page",
    "Alt+M      Song mode ON/OFF",
    "Alt+W      Waveform overlay",
    "Ctrl+H     Toggle this help",
    "ESC/Bksp   Back",
    "",
};

constexpr const char* kGenreLines[] = {
    "=== GENRE PAGE ===",
    "Arrows     Move in active lane",
    "Tab        Genre/Texture/Presets/Apply",
    "Enter      Apply selected",
    "Space      Toggle Apply mode",
    "1..8       Apply preset",
    "0          Random Genre+Texture",
    "M          Apply mode S+P / SND",
    "C          Curated/Advanced (recommended/all)",
    "G          Groove mode Acid/Minimal",
};

constexpr const char* kPatternLines[] = {
    "=== PATTERN EDIT (303) ===",
    "Q..I       Select pattern 1..8",
    "Arrows     Move cursor",
    "A/Z        Note +/-",
    "S/X        Octave +/-",
    "Alt+A      Accent toggle",
    "Alt+S      Slide toggle",
    "R          Clear step (REST)",
    "Bksp/Del   Clear step",
    "Alt+Bksp   Clear whole pattern",
    "G          Randomize pattern",
    "Ctrl+C/V   Copy/Paste",
    "Alt+Esc    Chain mode",
};

constexpr const char* kTB303Lines[] = {
    "=== TB303 PARAMS ===",
    "Left/Right Focus control",
    "Up/Down    Adjust value",
    "Ctrl       Fine adjust (arrows)",
    "A/Z S/X D/C F/V  Quick adjust",
    "T/G        Oscillator +/-",
    "Y/H        Filter type +/-",
    "N/M        Distortion/Delay",
    "Ctrl+Z/X/C/V  Reset parameter",
};

constexpr const char* kDrumLines[] = {
    "=== DRUM PAGE ===",
    "Q..I       Select pattern 1..8",
    "Arrows     Move cursor",
    "Enter      Toggle hit",
    "A          Toggle accent",
    "G          Randomize",
    "Ctrl+G     Randomize voice",
    "Alt+G      Chaos random",
    "Ctrl+C/V   Copy/Paste",
    "Alt+Esc    Chain mode",
};

constexpr const char* kSongLines[] = {
    "=== SONG PAGE ===",
    "Arrows     Move cursor",
    "Ctrl+Arrows Select area",
    "Q..U       Assign pattern 1..7",
    "Bksp/Tab   Clear cell",
    "M          Toggle song mode",
    "Ctrl+L     Loop mode",
    "G          Generate cell",
    "G x2       Generate row",
    "Alt+G      Generate selection",
    "Alt+.      Clear full song",
};

constexpr const char* kProjectLines[] = {
    "=== PROJECT PAGE ===",
    "Arrows     Navigate",
    "Enter      Activate control",
    "G          Jump to Genre page",
};

constexpr const char* kFeelLines[] = {
    "=== FEEL/TEXTURE PAGE ===",
    "Tab        Focus lane",
    "Up/Down    Select row",
    "Left/Right Change value",
    "Enter      Toggle/apply",
    "+/-        LoFi/Drive amount",
};

constexpr const char* kHubLines[] = {
    "=== SEQUENCER HUB ===",
    "Arrows     Navigate",
    "Enter      Open selected page",
    "Bksp/ESC   Back",
};

constexpr const char* kSettingsLines[] = {
    "=== SETTINGS PAGE ===",
    "Tab        Next group",
    "Up/Down    Select row",
    "Left/Right Adjust value",
    "Ctrl/Alt   Fast adjust",
    "1..3       Apply preset (regen)",
};

constexpr const char* kVoiceLines[] = {
    "=== VOICE PAGE ===",
    "Arrows     Select/Adjust",
    "Space      Preview phrase",
    "M          Toggle mute",
    "S          Stop speaking",
    "R/H/D      Voice presets",
    "1..0       Quick phrases",
};

inline const char* const* pageLines(int pageIndex, int& count) {
    switch (pageIndex) {
        case 0: count = sizeof(kSettingsLines) / sizeof(kSettingsLines[0]); return kSettingsLines;
        case 1:
        case 2: count = sizeof(kPatternLines) / sizeof(kPatternLines[0]); return kPatternLines;
        case 3:
        case 4: count = sizeof(kTB303Lines) / sizeof(kTB303Lines[0]); return kTB303Lines;
        case 5: count = sizeof(kDrumLines) / sizeof(kDrumLines[0]); return kDrumLines;
        case 6: count = sizeof(kSongLines) / sizeof(kSongLines[0]); return kSongLines;
        case 7: count = sizeof(kGenreLines) / sizeof(kGenreLines[0]); return kGenreLines;
        case 8: count = sizeof(kFeelLines) / sizeof(kFeelLines[0]); return kFeelLines;
        case 9: count = sizeof(kHubLines) / sizeof(kHubLines[0]); return kHubLines;
        case 10: count = sizeof(kProjectLines) / sizeof(kProjectLines[0]); return kProjectLines;
        case 11: count = sizeof(kVoiceLines) / sizeof(kVoiceLines[0]); return kVoiceLines;
        default: count = 0; return nullptr;
    }
}

inline int getTotalLines(int pageIndex) {
    const int globalCount = sizeof(kGlobalLines) / sizeof(kGlobalLines[0]);
    int pageCount = 0;
    (void)pageLines(pageIndex, pageCount);
    return pageCount + globalCount;
}

inline const char* getLine(int pageIndex, int index) {
    if (index < 0) return nullptr;
    int pageCount = 0;
    const char* const* lines = pageLines(pageIndex, pageCount);
    if (lines && index < pageCount) return lines[index];

    const int globalCount = sizeof(kGlobalLines) / sizeof(kGlobalLines[0]);
    const int globalIdx = index - pageCount;
    if (globalIdx < 0 || globalIdx >= globalCount) return nullptr;
    return kGlobalLines[globalIdx];
}

} // namespace HelpContent
