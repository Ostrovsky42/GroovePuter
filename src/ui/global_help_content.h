#pragma once

// ============================================================
// Global Help Content - Embedded from keys_sheet.md & keys_music_grok.md
// ============================================================

namespace HelpContent {

// Help text lines organized by sections
// Each line is short for embedded display (max ~40 chars)

constexpr const char* kGlobalSection[] = {
    "=== GLOBAL CONTROLS ===",
    "",
    "[ / ]      Prev/Next Page",
    "Alt+1..0   Jump to Page 1-10",
    "Alt+V      Voice Page",
    "Alt+W      Toggle Waveform",
    "Alt+M      Toggle Song Mode",
    "1..9, 0    Mute Tracks",
    "Ctrl+H     This Help",
};
constexpr int kGlobalSectionSize = sizeof(kGlobalSection) / sizeof(kGlobalSection[0]);

constexpr const char* kPatternEditSection[] = {
    "",
    "=== PATTERN EDIT (303) ===",
    "",
    "Q..I       Select Pattern 1-8",
    "Arrows     Navigate Steps",
    "A / Z      Note +/- Semitone",
    "S / X      Octave +/-",
    "Shift+Q    Toggle Slide",
    "Shift+W    Toggle Accent",
    "g          Randomize Pattern",
    "Alt+.      Clear Pattern",
};
constexpr int kPatternEditSectionSize = sizeof(kPatternEditSection) / sizeof(kPatternEditSection[0]);

constexpr const char* kDrumSection[] = {
    "",
    "=== DRUM SEQUENCER ===",
    "",
    "Arrows     Navigate Grid",
    "Enter/X    Toggle Hit",
    "W          Toggle Accent",
    "Q..I       Quick Pattern",
    "Space      Play/Stop",
};
constexpr int kDrumSectionSize = sizeof(kDrumSection) / sizeof(kDrumSection[0]);

constexpr const char* kSongSection[] = {
    "",
    "=== SONG PAGE ===",
    "",
    "Arrows     Move Cursor",
    "Shift+Arr  Select Area",
    "0..7       Assign Pattern",
    "Bksp/Tab   Clear Cell",
    "m          Toggle Song Mode",
    "Ctrl+L     Loop Mode",
    "g          Generate Pattern",
    "Alt+G      Batch Generate",
    "Alt+.      Clear Entire Song",
};
constexpr int kSongSectionSize = sizeof(kSongSection) / sizeof(kSongSection[0]);

constexpr const char* kVoiceSection[] = {
    "",
    "=== VOICE PAGE ===",
    "",
    "Arrows     Select/Adjust",
    "Space      Preview Phrase",
    "m          Toggle Mute",
    "s          Stop Speaking",
    "r/h/d      Presets: Robot/Hi/Deep",
    "1..0       Quick Select 1-10",
};
constexpr int kVoiceSectionSize = sizeof(kVoiceSection) / sizeof(kVoiceSection[0]);

constexpr const char* kTapeFXSection[] = {
    "",
    "=== TAPE FX ===",
    "",
    "UP/DOWN    Select Control",
    "LEFT/RIGHT Adjust Value",
    "R          Cycle Tape Mode",
    "P          Cycle Preset", 
    "F          Toggle FX On/Off",
    "1/2/3      Speed 0.5x/1x/2x",
    "Bksp/Del   Eject Tape",
};
constexpr int kTapeFXSectionSize = sizeof(kTapeFXSection) / sizeof(kTapeFXSection[0]);

constexpr const char* kTB303Section[] = {
    "",
    "=== TB-303 PARAMS ===",
    "",
    "Cutoff     Filter frequency",
    "Resonance  Filter resonance",
    "EnvAmount  Envelope depth",
    "EnvDecay   Decay time",
    "Osc: Saw/Square waveform",
};
constexpr int kTB303SectionSize = sizeof(kTB303Section) / sizeof(kTB303Section[0]);

constexpr const char* kTipsSection[] = {
    "",
    "=== TIPS ===",
    "",
    "Slide+Accent = Classic Acid!",
    "Alt+. = Panic/Reset button",
    "Shift+Arrows = Fast nav",
    "",
    "MiniAcid v0.0.7 - Acid Never Dies!",
};
constexpr int kTipsSectionSize = sizeof(kTipsSection) / sizeof(kTipsSection[0]);

// Total lines for scrolling calculation
inline int getTotalLines() {
    return kGlobalSectionSize + kPatternEditSectionSize + kDrumSectionSize +
           kSongSectionSize + kVoiceSectionSize + kTapeFXSectionSize +
           kTB303SectionSize + kTipsSectionSize;
}

// Get line by absolute index
inline const char* getLine(int index) {
    if (index < 0) return nullptr;
    
    int offset = 0;
    
    if (index < offset + kGlobalSectionSize) return kGlobalSection[index - offset];
    offset += kGlobalSectionSize;
    
    if (index < offset + kPatternEditSectionSize) return kPatternEditSection[index - offset];
    offset += kPatternEditSectionSize;
    
    if (index < offset + kDrumSectionSize) return kDrumSection[index - offset];
    offset += kDrumSectionSize;
    
    if (index < offset + kSongSectionSize) return kSongSection[index - offset];
    offset += kSongSectionSize;
    
    if (index < offset + kVoiceSectionSize) return kVoiceSection[index - offset];
    offset += kVoiceSectionSize;
    
    if (index < offset + kTapeFXSectionSize) return kTapeFXSection[index - offset];
    offset += kTapeFXSectionSize;
    
    if (index < offset + kTB303SectionSize) return kTB303Section[index - offset];
    offset += kTB303SectionSize;
    
    if (index < offset + kTipsSectionSize) return kTipsSection[index - offset];
    
    return nullptr;
}

} // namespace HelpContent
