#pragma once

#include "workflow_mode.h"

namespace HelpContent {

constexpr const char* kGlobalLines[] = {
    "=== GLOBAL ===",
    "Alt+H       Toggle this help",
    "Ctrl+U      Undo last edit",
    "Up/Down     Scroll help",
    "Left/Right  Help top/end",
    "Fn+M        Workspace launcher",
    "Fn+Tab      Next workflow",
    "Fn+Sh+Tab   Previous workflow",
    "[ / ]       Prev/next page",
    "Fn+[ / ]    Prev/next workflow",
    "Alt+[ / ]   Prev/next pattern page",
    "Alt/Fn+1..0 Direct page jump",
    "Space       Active transport",
    "Alt+P       MIDI Player",
    "Alt+V       Groove Lab",
    "Alt+W       Waveform except PHRASE",
    "Alt+\\       Theme CARBON/CYBER",
    "Alt+X       LiveMix ON/OFF",
    "Alt+M       Song mode ON/OFF",
    "1..0        Track mute fallback",
    "Esc/Bksp/`  Back / previous page",
    "Ctrl+Alt+Bksp Project reset",
    "",
};

constexpr const char* kGenreLines[] = {
    "=== GENRE 1/2 ===",
    "Genre = corridor/vocabulary",
    "Tab/Up/Dn   Select field",
    "Left/Right  Genre/variant/rhythm/apply",
    "Alt+L/R     Morph selected variant",
    "Enter       Apply profile/materialize",
    "M           Cycle apply mode",
    "PROFILE     Keep existing patterns",
    "MATERIALIZE Regenerate patterns",
    "RHYTHM AUTO or fixed identity",
    "No texture or feel changes",
};

constexpr const char* kSynthALines[] = {
    "=== SYNTH A PATTERN ===",
    "Tab         Pattern/automation",
    "Q..I        Select pattern 1..8",
    "B           Toggle bank A/B",
    "Ctrl+1/2    Bank A/B (direct)",
    "Alt+[ / ]   Pattern page",
    "Arrows      Move cursor",
    "Shift/Ctrl+Arrows Select area",
    "A/Z         Note +/-",
    "S/X         Octave +/-",
    "Alt+Arrows  Rotate/parameter edit",
    "Alt/Ctrl+A  Accent",
    "Alt/Ctrl+S  Slide",
    "F           Cycle step FX",
    "R/Bksp/Del  Clear step (REST)",
    "Alt+Bksp    Clear whole pattern",
    "G           Randomize pattern",
    "Ctrl+C/V    Copy/Paste",
    "Esc/`       Clear selection",
};

constexpr const char* kSynthBLines[] = {
    "=== SYNTH B PATTERN ===",
    "Tab         Pattern/automation",
    "Q..I        Select pattern 1..8",
    "B           Toggle bank A/B",
    "Ctrl+1/2    Bank A/B (direct)",
    "Alt+[ / ]   Pattern page",
    "Arrows      Move cursor",
    "Shift/Ctrl+Arrows Select area",
    "A/Z         Note +/-",
    "S/X         Octave +/-",
    "Alt+Arrows  Rotate/parameter edit",
    "Alt/Ctrl+A  Accent",
    "Alt/Ctrl+S  Slide",
    "F           Cycle step FX",
    "R/Bksp/Del  Clear step (REST)",
    "Alt+Bksp    Clear whole pattern",
    "G           Randomize pattern",
    "Ctrl+C/V    Copy/Paste",
    "Esc/`       Clear selection",
};

constexpr const char* kSynthASoundLines[] = {
    "=== SYNTH A SOUND ===",
    "Tab         Main/More parameters",
    "Left/Right  Focus or change value",
    "Up/Down     Value or row",
    "Shift/Ctrl  Fine adjustment",
    "Ctrl+1/2    Pattern bank",
    "Q..I        Pattern when NOTE off",
    "A/Z S/X D/C F/V Quick controls",
    "T/G         Oscillator +/-",
    "Y/H         Filter type +/-",
    "N/M         Distortion/Delay",
    "Ctrl+Z/X/C/V Reset parameter",
};

constexpr const char* kSynthBSoundLines[] = {
    "=== SYNTH B SOUND ===",
    "Tab         Main/More parameters",
    "Left/Right  Focus or change value",
    "Up/Down     Value or row",
    "Shift/Ctrl  Fine adjustment",
    "Ctrl+1/2    Pattern bank",
    "Q..I        Pattern when NOTE off",
    "A/Z S/X D/C F/V Quick controls",
    "T/G         Oscillator +/-",
    "Y/H         Filter type +/-",
    "N/M         Distortion/Delay",
    "Ctrl+Z/X/C/V Reset parameter",
};

constexpr const char* kDrumLines[] = {
    "=== DRUMS ===",
    "Tab         Grid/feel/auto/samples",
    "Q..I        Select pattern 1..8",
    "B           Toggle bank A/B",
    "Ctrl+1/2    Bank A/B (direct)",
    "Alt+[ / ]   Pattern page",
    "Arrows      Move cursor",
    "Shift/Ctrl+Arrows Select area",
    "Enter       Toggle hit",
    "A           Toggle accent",
    "G           Randomize pattern",
    "Ctrl+G      Randomize voice",
    "Alt+G       Chaos randomize all",
    "Bksp/Del    Clear hit/selection",
    "Alt+Bksp    Clear whole pattern",
    "Ctrl+C/V    Copy/Paste",
    "SAMPLES M   Layer ON/OFF",
    "SAMPLES Bksp Clear pad sample",
    "SAMPLES Enter Preview pad",
    "SAMPLES Q-I Audition pads 1..8",
    "Space       Transport on every tab",
    "Esc/`       Clear selection",
};

constexpr const char* kSongLines[] = {
    "=== SONG ===",
    "Arrows      Move cursor",
    "Shift/Ctrl+Arrows Select area",
    "Enter       Jump to pattern editor",
    "Q..I        Assign existing pattern",
    "G           Generate/materialize cell",
    "G x2        Generate current row",
    "Alt+G       Generate selection",
    "Ctrl+G      Cycle generator mode",
    "B           Flip pattern bank",
    "Ctrl+N/M    Insert/delete row",
    "Alt+B       Edit Song slot A/B",
    "Ctrl+B      Play Song slot A/B",
    "V           Toggle DR/VO lane",
    "X           Split compare",
    "L           Loop-lock playhead",
    "Ctrl+L      Toggle loop mode",
    "Ctrl+R      Reverse playback",
    "Alt+X       LiveMix ON/OFF",
    "Ctrl+C/V    Copy/Paste",
    "Ctrl+1..8   Jump edit page 1..8",
    "P           Cursor to playhead",
    "Ctrl+W/S    Jump 8 rows",
    "Ctrl+Alt+W/S Jump 32 rows",
    "Alt+Q/E/R/T Save markers 1..4",
    "Ctrl+Alt+Q/E/R/T Jump markers",
    "Alt+,/.     Song top/end",
    "Bksp/Tab    Clear cell",
    "Alt+Bksp    Clear full Song",
};

constexpr const char* kPhraseLines[] = {
    "=== PHRASE CORE ===",
    "1..4        Select Phrase A/B/C/D",
    "Up/Down     Capture length 1/2/4/8",
    "Left/Right  Preview Phrase bar",
    "Ctrl+L/R    Move TO row +/-1",
    "Ctrl+U/D    Move TO row +/-8",
    "R           Cycle capture role",
    "Shift+R     Previous role",
    "P           Cycle derive parent",
    "Enter       Capture current Song row",
    "D           Derive parent into slot",
    "W           INSERT before TO row",
    "            Shifts following rows",
    "Alt+W       REPLACE at TO row",
    "            No row shift",
    "Bksp/Del    Clear selected Phrase",
    "REF         Mutable pattern references",
};

constexpr const char* kHubLines[] = {
    "=== OVERVIEW / SEQUENCER HUB ===",
    "Up/Down     Select track",
    "Left/Right  Select step",
    "-/=         Track volume",
    "X           Toggle hit/note",
    "A           Toggle accent",
    "Enter       Open track detail",
    "Esc/Bksp    Return to overview",
    "Space       Transport",
    "Q..I        Select local pattern",
    "B           Toggle pattern bank",
    "Ctrl+C/V    Copy/Paste",
    "H           Player/HUB MIDI return",
    "1..9        Physical SMF track mute",
    "C           Edit saved per-file route",
};

constexpr const char* kFeelLines[] = {
    "=== FEEL 2/2 ===",
    "Feel = timing/velocity only",
    "Tab/Up/Dn   Select field",
    "Left/Right  Adjust value/preset",
    "Shift/Ctrl  Fast adjustment",
    "Enter/Space Apply FEEL preset",
    "Profile     Straight/Swing/Laid/Push",
    "Swing       Runtime offbeat timing",
    "Feel Amount Bounded role offset",
    "Vel Human   Velocity deviation",
    "Next gen; clock/pitch unchanged",
    "No notes, roles or sound changes",
};

constexpr const char* kProjectLines[] = {
    "=== PROJECT / SETUP ===",
    "Tab         Next section",
    "Up/Down     Select row",
    "Left/Right  Adjust value/focus",
    "Enter       Open or activate",
    "G           Jump to Genre",
    "Esc/Bksp    Close dialog/go up",
    "X           Delete selected scene",
    "Import: Tab Open MIDI matrix",
};

constexpr const char* kPerformLines[] = {
    "=== MIDI KEYBOARD / PERFORM ===",
    "QWERTYUIOP  Upper note manual",
    "ASDFGHJKL   Lower note manual",
    "N           NOTE mode ON/OFF",
    "\\           Cycle output target",
    ", / .       Previous/next scale",
    "- / =       Octave down/up",
    "X           Panic live target",
    "Tab         PERFORMANCE TOOLS",
    "1..8        Select performance tool",
    "Shift+1..8  Cycle tool backward",
    "Esc/`       Close tools layer",
};

constexpr const char* kPlayerLines[] = {
    "=== MIDI PLAYER ===",
    "Enter       Open selected MIDI file",
    "Space       MIDI transport",
    "H           Open/return HUB MIDI",
    "1..9        Physical track mute",
    "U           Physical mute mixer",
    "I           Channel inspector",
    "S           Structural inspector",
    "D           Performance panel",
    "B/Bksp      Files or previous panel",
    "Arrows      Select/seek/adjust BPM",
    "C           Clock source",
    "T           Tempo mode",
    "M           RAW/SEQTRAK routing",
    "G           Groove transport/follow",
    "R           Restart file",
    "V           Velocity boost",
    "X           Panic SMF notes",
};

inline const char* const* pageLines(int pageIndex, int& count) {
    pageIndex = WorkflowPages::normalizeLegacyPage(pageIndex);
    switch (pageIndex) {
        case WorkflowPages::kGenre:
            count = sizeof(kGenreLines) / sizeof(kGenreLines[0]); return kGenreLines;
        case WorkflowPages::kSynthA:
            count = sizeof(kSynthALines) / sizeof(kSynthALines[0]); return kSynthALines;
        case WorkflowPages::kSynthB:
            count = sizeof(kSynthBLines) / sizeof(kSynthBLines[0]); return kSynthBLines;
        case WorkflowPages::kSynthAParameters:
            count = sizeof(kSynthASoundLines) / sizeof(kSynthASoundLines[0]); return kSynthASoundLines;
        case WorkflowPages::kSynthBParameters:
            count = sizeof(kSynthBSoundLines) / sizeof(kSynthBSoundLines[0]); return kSynthBSoundLines;
        case WorkflowPages::kDrums:
            count = sizeof(kDrumLines) / sizeof(kDrumLines[0]); return kDrumLines;
        case WorkflowPages::kArrange:
            count = sizeof(kSongLines) / sizeof(kSongLines[0]); return kSongLines;
        case WorkflowPages::kPhrase:
            count = sizeof(kPhraseLines) / sizeof(kPhraseLines[0]); return kPhraseLines;
        case WorkflowPages::kPattern:
            count = sizeof(kHubLines) / sizeof(kHubLines[0]); return kHubLines;
        case WorkflowPages::kTexture:
        case WorkflowPages::kGeneration:
        case WorkflowPages::kFeel:
            count = sizeof(kFeelLines) / sizeof(kFeelLines[0]); return kFeelLines;
        case WorkflowPages::kProject:
            count = sizeof(kProjectLines) / sizeof(kProjectLines[0]); return kProjectLines;
        case WorkflowPages::kPerform:
            count = sizeof(kPerformLines) / sizeof(kPerformLines[0]); return kPerformLines;
        case WorkflowPages::kPlayer:
            count = sizeof(kPlayerLines) / sizeof(kPlayerLines[0]); return kPlayerLines;
        default:
            count = 0; return nullptr;
    }
}

inline const char* pageTitle(int pageIndex) {
    return WorkflowPages::pageName(pageIndex);
}

inline int getPageLineCount(int pageIndex) {
    int count = 0;
    (void)pageLines(pageIndex, count);
    return count;
}

inline int getTotalLines(int pageIndex) {
    const int globalCount = sizeof(kGlobalLines) / sizeof(kGlobalLines[0]);
    return getPageLineCount(pageIndex) + globalCount;
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