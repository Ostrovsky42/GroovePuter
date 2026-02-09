#include "settings_page.h"
#include "../../../scenes.h"
#include "../ui_colors.h"
#include "../ui_input.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace {
    enum class SettingId {
        Swing = 0,
        VelocityRange,
        GhostProb,
        MicroTiming,
        MinNotes,
        MaxNotes,
        MinOctave,
        MaxOctave,
        ScaleRoot,
        ScaleType,
        PreferDownbeats,
        ScaleQuantize,
        Count
    };

    const char* scaleTypeToString(ScaleType type) {
        switch(type) {
            case ScaleType::MINOR: return "Minor";
            case ScaleType::MAJOR: return "Major";
            case ScaleType::DORIAN: return "Dorian";
            case ScaleType::PHRYGIAN: return "Phrygian";
            case ScaleType::LYDIAN: return "Lydian";
            case ScaleType::MIXOLYDIAN: return "Mixolydian";
            case ScaleType::LOCRIAN: return "Locrian";
            case ScaleType::PENTATONIC_MJ: return "Penta Maj";
            case ScaleType::PENTATONIC_MN: return "Penta Min";
            case ScaleType::CHROMATIC: return "Chromatic";
            default: return "Unknown";
        }
    }
    
    const char* noteToString(int note) {
        static const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        return notes[note % 12];
    }

    static constexpr SettingId kGroupTiming[4] = {
        SettingId::Swing,
        SettingId::VelocityRange,
        SettingId::GhostProb,
        SettingId::MicroTiming
    };

    static constexpr SettingId kGroupNotes[4] = {
        SettingId::MinNotes,
        SettingId::MaxNotes,
        SettingId::MinOctave,
        SettingId::MaxOctave
    };

    static constexpr SettingId kGroupScale[4] = {
        SettingId::ScaleRoot,
        SettingId::ScaleType,
        SettingId::ScaleQuantize,
        SettingId::PreferDownbeats
    };

    static constexpr int kRowsPerGroup = 4;
    static constexpr int kPresetRowIndex = 4;
    static const char* kPresetNames[3] = { "TIGHT", "HUMAN", "LOOSE" };

    void appendDelta(char* buf, size_t bufSize, int& used, bool& any,
                     const char* label, const char* from, const char* to, int& shown, bool& more) {
        if (strcmp(from, to) == 0) return;
        if (shown >= 3) { more = true; return; }
        used += snprintf(buf + used, bufSize - used, "%s%s %s->%s", any ? ", " : "", label, from, to);
        any = true;
        shown++;
    }

    void appendDeltaInt(char* buf, size_t bufSize, int& used, bool& any,
                        const char* label, int from, int to, int& shown, bool& more) {
        if (from == to) return;
        if (shown >= 3) { more = true; return; }
        used += snprintf(buf + used, bufSize - used, "%s%s %d->%d", any ? ", " : "", label, from, to);
        any = true;
        shown++;
    }

    void appendDeltaRange(char* buf, size_t bufSize, int& used, bool& any,
                          const char* label, int f1, int f2, int t1, int t2, int& shown, bool& more) {
        if (f1 == t1 && f2 == t2) return;
        if (shown >= 3) { more = true; return; }
        used += snprintf(buf + used, bufSize - used, "%s%s %d-%d->%d-%d",
                         any ? ", " : "", label, f1, f2, t1, t2);
        any = true;
        shown++;
    }
}

SettingsPage::SettingsPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard) 
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
    (void)gfx;
}

const std::string& SettingsPage::getTitle() const {
    static std::string title = "Generator";
    return title;
}

void SettingsPage::draw(IGfx& gfx) {
    UI::drawStandardHeader(gfx, mini_acid_, "GENERATOR");
    LayoutManager::clearContent(gfx);


    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    
    const int y0 = LayoutManager::lineY(0);
    const int leftX = Layout::COL_1;
    const int leftW = Layout::COL_WIDTH;
    const int rightX = Layout::COL_2;
    const int rightW = Layout::CONTENT.w - rightX - 4;

    // Group label (single line)
    gfx.setTextColor(COLOR_LABEL);
    const char* groupName = (group_ == Group::Timing) ? "TIMING" :
                            (group_ == Group::Notes) ? "NOTES" : "SCALE";
    char groupBuf[20];
    std::snprintf(groupBuf, sizeof(groupBuf), "GROUP %s", groupName);
    gfx.drawText(leftX, y0, groupBuf);

    // Info header (right)
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(rightX, y0, "INFO");

    // Rows
    const int rowStart = 1;
    for (int i = 0; i < kRowsPerGroup; ++i) {
        int y = LayoutManager::lineY(rowStart + i);
        char buf[32];
        int id = settingForRow();
        if (i != row_) {
            // Build label for non-selected row too
            switch (group_) {
                case Group::Timing: id = (int)kGroupTiming[i]; break;
                case Group::Notes: id = (int)kGroupNotes[i]; break;
                case Group::Scale: id = (int)kGroupScale[i]; break;
            }
        }
        formatSetting(buf, sizeof(buf), id, params);
        Widgets::drawListRow(gfx, leftX, y, leftW, buf,
                             i == row_);
    }

    // Presets row (below lists)
    const int presetLabelY = LayoutManager::lineY(rowStart + kRowsPerGroup);
    const int presetGridY = LayoutManager::lineY(rowStart + kRowsPerGroup + 1);
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(leftX, presetLabelY, (row_ == kPresetRowIndex) ? "P> PRESETS" : "P  PRESETS");
    const int cellW = 36;
    Widgets::drawButtonGrid(gfx, leftX, presetGridY, cellW, 10, 3, 1, kPresetNames, 3,
                            (row_ == kPresetRowIndex) ? preset_index_ : -1);

    // Compact right-side info
    int infoCount = 0;
    const char* const* infoLines = commentLines(infoCount, params);
    if (infoLines && infoCount > 0) {
        Widgets::drawInfoBox(gfx, rightX, LayoutManager::lineY(1), rightW, infoLines, infoCount);
    }

    UI::drawStandardFooter(gfx, "[TAB] GROUP  [ARROWS] SELECT", hintText());
}

bool SettingsPage::handleEvent(UIEvent& e) {
    if (e.event_type != GROOVEPUTER_KEY_DOWN) return false;

    const bool fast = e.shift || e.ctrl || e.alt;
    const int maxRow = kPresetRowIndex;

    int nav = UIInput::navCode(e);
    if (nav == GROOVEPUTER_UP) {
        row_ = (row_ == 0) ? maxRow : (row_ - 1);
        return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
        row_ = (row_ == maxRow) ? 0 : (row_ + 1);
        return true;
    }
    
    if (nav == GROOVEPUTER_LEFT) { 
        if (row_ == kPresetRowIndex) {
            preset_index_ = (preset_index_ + 2) % 3;
        } else {
            adjustSetting(-1, fast);
        }
        return true; 
    }
    if (nav == GROOVEPUTER_RIGHT) { 
        if (row_ == kPresetRowIndex) {
            preset_index_ = (preset_index_ + 1) % 3;
        } else {
            adjustSetting(1, fast);
        }
        return true; 
    }

    if (e.key == '\t') {
        if (group_ == Group::Timing) group_ = Group::Notes;
        else if (group_ == Group::Notes) group_ = Group::Scale;
        else group_ = Group::Timing;
        return true;
    }

    if (e.key == '\n' || e.key == '\r' || e.key == ' ') {
        if (row_ == kPresetRowIndex) {
            applyPreset(preset_index_);
            return true;
        }
    }

    /*
    // START: Number keys disabled for Mute priority
    if (e.key >= '1' && e.key <= '3') {
        preset_index_ = e.key - '1';
        applyPreset(preset_index_);
        return true;
    }
    // END: Number keys disabled
    */
    
    return false;
}

void SettingsPage::adjustSetting(int delta, bool shift) {
    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    SettingId id = static_cast<SettingId>(settingForRow());
    
    // Professional touch: accelerated steps
    float fStep = shift ? 0.15f : 0.05f;
    int iStep = shift ? 5 : 1;
    
    float fDelta = delta * fStep;
    int iDelta = delta * iStep;
    
    switch (id) {
        case SettingId::Swing:
            params.swingAmount = std::clamp(params.swingAmount + fDelta, 0.0f, 1.0f);
            break;
        case SettingId::VelocityRange:
            params.velocityRange = std::clamp(params.velocityRange + fDelta, 0.0f, 1.0f);
            break;
        case SettingId::GhostProb:
            params.ghostNoteProbability = std::clamp(params.ghostNoteProbability + fDelta, 0.0f, 1.0f);
            break;
        case SettingId::MicroTiming:
            params.microTimingAmount = std::clamp(params.microTimingAmount + fDelta, 0.0f, 1.0f);
            break;
        case SettingId::MinNotes: 
            params.minNotes = std::clamp(params.minNotes + iDelta, 1, params.maxNotes);
            break;
        case SettingId::MaxNotes:
            params.maxNotes = std::clamp(params.maxNotes + iDelta, params.minNotes, 16);
            break;
        case SettingId::MinOctave:
            params.minOctave = std::clamp(params.minOctave + iDelta, 0, params.maxOctave);
            break;
        case SettingId::MaxOctave:
            params.maxOctave = std::clamp(params.maxOctave + iDelta, params.minOctave, 10);
            break;
        case SettingId::ScaleRoot:
            params.scaleRoot = (params.scaleRoot + iDelta + 1200) % 12; // safe positive mod
            break;
        case SettingId::ScaleType:
            {
                int val = (int)params.scale + delta; // just step by 1 for enum
                int max = 9; // Approx ScaleType count - 1 (CHROMATIC=9)
                if (val < 0) val = max;
                if (val > max) val = 0;
                params.scale = (ScaleType)val;
            }
            break;
        case SettingId::PreferDownbeats:
            if (delta != 0) params.preferDownbeats = !params.preferDownbeats;
            break;
        case SettingId::ScaleQuantize:
            if (delta != 0) params.scaleQuantize = !params.scaleQuantize;
            break;
        default: break;
    }
}

int SettingsPage::settingForRow() const {
    if (row_ >= kRowsPerGroup) {
        return (int)SettingId::Swing;
    }
    switch (group_) {
        case Group::Timing: return (int)kGroupTiming[row_];
        case Group::Notes: return (int)kGroupNotes[row_];
        case Group::Scale: return (int)kGroupScale[row_];
    }
    return (int)SettingId::Swing;
}

void SettingsPage::formatSetting(char* buf, size_t bufSize, int id, const GeneratorParams& params) const {
    SettingId sid = static_cast<SettingId>(id);
    switch (sid) {
        case SettingId::Swing:
            std::snprintf(buf, bufSize, "Swing     %d%%", (int)(params.swingAmount * 100.0f + 0.5f));
            break;
        case SettingId::VelocityRange:
            std::snprintf(buf, bufSize, "Vel Range %d%%", (int)(params.velocityRange * 100.0f + 0.5f));
            break;
        case SettingId::GhostProb:
            std::snprintf(buf, bufSize, "Ghost Prob %d%%", (int)(params.ghostNoteProbability * 100.0f + 0.5f));
            break;
        case SettingId::MicroTiming:
            std::snprintf(buf, bufSize, "MicroTime %d%%", (int)(params.microTimingAmount * 100.0f + 0.5f));
            break;
        case SettingId::MinNotes:
            std::snprintf(buf, bufSize, "Min Notes %d", params.minNotes);
            break;
        case SettingId::MaxNotes:
            std::snprintf(buf, bufSize, "Max Notes %d", params.maxNotes);
            break;
        case SettingId::MinOctave:
            std::snprintf(buf, bufSize, "Min Oct  %d", params.minOctave);
            break;
        case SettingId::MaxOctave:
            std::snprintf(buf, bufSize, "Max Oct  %d", params.maxOctave);
            break;
        case SettingId::ScaleRoot:
            std::snprintf(buf, bufSize, "Scale Root %s", noteToString(params.scaleRoot));
            break;
        case SettingId::ScaleType:
            std::snprintf(buf, bufSize, "Scale %s", scaleTypeToString(params.scale));
            break;
        case SettingId::PreferDownbeats:
            std::snprintf(buf, bufSize, "Downbeats %s", params.preferDownbeats ? "ON" : "OFF");
            break;
        case SettingId::ScaleQuantize:
            std::snprintf(buf, bufSize, "Quantize %s", params.scaleQuantize ? "ON" : "OFF");
            break;
        default:
            std::snprintf(buf, bufSize, "Unknown");
            break;
    }
}

const char* SettingsPage::hintText() const {
    if (row_ == kPresetRowIndex) return "1-3 preset (regen)";
    return "[L/R] ADJ  [CTRL/ALT] FAST";
}

const char* const* SettingsPage::commentLines(int& count, const GeneratorParams& params) const {
    static char line1[48];
    static char line2[48];
    static char line3[48];
    static char line4[48];
    static const char* lines[4] = { line1, line2, line3, line4 };

    auto set3 = [&](const char* a, const char* b, const char* c) -> const char* const* {
        std::snprintf(line1, sizeof(line1), "%s", a ? a : "");
        std::snprintf(line2, sizeof(line2), "%s", b ? b : "");
        std::snprintf(line3, sizeof(line3), "%s", c ? c : "");
        count = 3;
        return lines;
    };
    auto set4 = [&](const char* a, const char* b, const char* c, const char* d) -> const char* const* {
        std::snprintf(line1, sizeof(line1), "%s", a ? a : "");
        std::snprintf(line2, sizeof(line2), "%s", b ? b : "");
        std::snprintf(line3, sizeof(line3), "%s", c ? c : "");
        std::snprintf(line4, sizeof(line4), "%s", d ? d : "");
        count = 4;
        return lines;
    };

    if (row_ == kPresetRowIndex) {
        return set4(
            "Presets:",
            "T-lower swing/ghost",
            "H-balanced",
            "L-more groove");
    }

    SettingId id = static_cast<SettingId>(settingForRow());
    switch (id) {
        case SettingId::Swing: {
            int v = (int)(params.swingAmount * 100.0f + 0.5f);
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current: %d%% swing", v);
            return set3(cur, "Shifts offbeats","later");
        }
        case SettingId::VelocityRange: {
            int v = (int)(params.velocityRange * 100.0f + 0.5f);
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current: %d%% vel range", v);
            return set3(cur, "Higher = more","dynamics");
        }
        case SettingId::GhostProb: {
            int v = (int)(params.ghostNoteProbability * 100.0f + 0.5f);
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current: %d%% ghosts", v);
            return set3(cur, "Adds low-velocity","notes");
        }
        case SettingId::MicroTiming: {
            int v = (int)(params.microTimingAmount * 100.0f + 0.5f);
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current: %d%% microtiming", v);
            return set3(cur, "Random timing","offsets");
        }
        case SettingId::MinNotes: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current min: %d notes", params.minNotes);
            std::snprintf(cur, sizeof(cur), "Current min: %d notes", params.minNotes);
            return set3(cur, "Lower floor", "<Max Notes");
        }
        case SettingId::MaxNotes: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current max: %d notes", params.maxNotes);
            return set3(cur, "Upper ceiling", ">Min Notes");
        }
        case SettingId::MinOctave: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current min octave: %d", params.minOctave);
            return set3(cur, "Lowest octave", "<Max Oct");
        }
        case SettingId::MaxOctave: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current max octave: %d", params.maxOctave);
            return set3(cur, "Highest octave", ">Min Oct");
        }
        case SettingId::ScaleRoot: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current root: %s", noteToString(params.scaleRoot));
            return set3(cur, "Transposes note palette", "Affects regeneration");
        }
        case SettingId::ScaleType: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Current scale: %s", scaleTypeToString(params.scale));
            return set3(cur, "Sets note collection", "Affects regeneration");
        }
        case SettingId::ScaleQuantize: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Quantize: %s", params.scaleQuantize ? "ON" : "OFF");
            return set3(cur, "Locks notes to scale", "Affects regeneration");
        }
        case SettingId::PreferDownbeats: {
            char cur[48];
            std::snprintf(cur, sizeof(cur), "Downbeats: %s", params.preferDownbeats ? "ON" : "OFF");
            return set3(cur, "Biases accents on 1/5/9/13", "Affects regeneration");
        }
        default:
            break;
    }
    count = 0;
    return nullptr;
}

void SettingsPage::applyPreset(int index) {
    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    GeneratorParams before = params;

    switch (index) {
        case 0: // TIGHT
            params.swingAmount = 0.10f;
            params.ghostNoteProbability = 0.05f;
            params.minNotes = 8;
            params.maxNotes = 10;
            params.minOctave = 2;
            params.maxOctave = 3;
            params.scaleQuantize = true;
            params.preferDownbeats = true;
            break;
        case 1: // HUMAN
            params.swingAmount = 0.35f;
            params.ghostNoteProbability = 0.10f;
            params.minNotes = 7;
            params.maxNotes = 11;
            params.minOctave = 2;
            params.maxOctave = 4;
            params.scaleQuantize = true;
            params.preferDownbeats = true;
            break;
        case 2: // LOOSE (dub/broken)
            params.swingAmount = 0.60f;
            params.ghostNoteProbability = 0.15f;
            params.minNotes = 6;
            params.maxNotes = 12;
            params.minOctave = 2;
            params.maxOctave = 4;
            params.scaleQuantize = true;
            params.preferDownbeats = true;
            break;
        default:
            break;
    }

    // Delta toast (REGEN)
    char toast[96];
    int used = 0;
    bool any = false;
    int shown = 0;
    bool more = false;

    appendDeltaInt(toast, sizeof(toast), used, any, "Sw",
                   (int)(before.swingAmount * 100.0f + 0.5f),
                   (int)(params.swingAmount * 100.0f + 0.5f),
                   shown, more);
    appendDeltaInt(toast, sizeof(toast), used, any, "Gh",
                   (int)(before.ghostNoteProbability * 100.0f + 0.5f),
                   (int)(params.ghostNoteProbability * 100.0f + 0.5f),
                   shown, more);
    appendDeltaRange(toast, sizeof(toast), used, any, "Nt",
                     before.minNotes, before.maxNotes,
                     params.minNotes, params.maxNotes,
                     shown, more);
    appendDeltaRange(toast, sizeof(toast), used, any, "Oct",
                     before.minOctave, before.maxOctave,
                     params.minOctave, params.maxOctave,
                     shown, more);
    appendDelta(toast, sizeof(toast), used, any, "Q",
                before.scaleQuantize ? "ON" : "OFF",
                params.scaleQuantize ? "ON" : "OFF",
                shown, more);
    appendDelta(toast, sizeof(toast), used, any, "Db",
                before.preferDownbeats ? "ON" : "OFF",
                params.preferDownbeats ? "ON" : "OFF",
                shown, more);

    if (!any) {
        snprintf(toast, sizeof(toast), "Preset applied (regen)");
    } else {
        if (more && used < (int)sizeof(toast) - 4) {
            used += snprintf(toast + used, sizeof(toast) - used, "…");
        }
        snprintf(toast + used, sizeof(toast) - used, " (regen)");
    }

    UI::showToast(toast, 2000);
}
