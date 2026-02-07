#include "feel_texture_page.h"

#include "../layout_manager.h"
#include "../ui_colors.h"
#include "../ui_input.h"
#include "../ui_widgets.h"

#include <cstdio>

namespace {
const int kRowH = Layout::LINE_HEIGHT;
const char* kPresetNames[4] = { "SPACE", "NORM", "WIDE", "GRIT" };

const char* gridLabel(uint8_t steps) {
    switch (steps) {
        case 8: return "1/8";
        case 32: return "1/32";
        case 16:
        default: return "1/16";
    }
}

const char* timebaseLabel(uint8_t tb) {
    if (tb == 0) return "H";
    if (tb == 2) return "D";
    return "N";
}

void formatFeelDelta(const FeelSettings& before, const FeelSettings& after, char* out, size_t outSize) {
    int n = 0;
    bool any = false;
    if (before.gridSteps != after.gridSteps) {
        n += snprintf(out + n, outSize - n, "%sG %s->%s", any ? ", " : "",
                      gridLabel(before.gridSteps), gridLabel(after.gridSteps));
        any = true;
    }
    if (before.timebase != after.timebase) {
        n += snprintf(out + n, outSize - n, "%sT %s->%s", any ? ", " : "",
                      timebaseLabel(before.timebase), timebaseLabel(after.timebase));
        any = true;
    }
    if (before.patternBars != after.patternBars) {
        n += snprintf(out + n, outSize - n, "%sL %dB->%dB", any ? ", " : "",
                      before.patternBars, after.patternBars);
        any = true;
    }
    if (!any) {
        snprintf(out, outSize, "Preset applied");
    }
}
}

FeelTexturePage::FeelTexturePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
    (void)gfx;
}

const char* FeelTexturePage::gridToString(GridResolution g) {
    switch (g) {
        case GridResolution::Eighth: return "1/8";
        case GridResolution::Sixteenth: return "1/16";
        case GridResolution::ThirtySecond: return "1/32";
        default: return "?";
    }
}

const char* FeelTexturePage::lengthToString(PatternLength l) {
    switch (l) {
        case PatternLength::OneBar: return "1B";
        case PatternLength::TwoBars: return "2B";
        case PatternLength::FourBars: return "4B";
        case PatternLength::EightBars: return "8B";
        default: return "?";
    }
}

const char* FeelTexturePage::timebaseToString(Timebase t) {
    switch (t) {
        case Timebase::Half: return "HALF";
        case Timebase::Double: return "DBL";
        case Timebase::Normal:
        default: return "NORM";
    }
}

void FeelTexturePage::syncFromScene() {
    const auto& feel = mini_acid_.sceneManager().currentScene().feel;

    switch (feel.gridSteps) {
        case 8: grid_resolution_ = GridResolution::Eighth; break;
        case 32: grid_resolution_ = GridResolution::ThirtySecond; break;
        case 16:
        default:
            grid_resolution_ = GridResolution::Sixteenth;
            break;
    }

    switch (feel.patternBars) {
        case 2: pattern_length_ = PatternLength::TwoBars; break;
        case 4: pattern_length_ = PatternLength::FourBars; break;
        case 8: pattern_length_ = PatternLength::EightBars; break;
        case 1:
        default:
            pattern_length_ = PatternLength::OneBar;
            break;
    }

    switch (feel.timebase) {
        case 0: timebase_ = Timebase::Half; break;
        case 2: timebase_ = Timebase::Double; break;
        case 1:
        default:
            timebase_ = Timebase::Normal;
            break;
    }

    texture_.lofi_enabled = feel.lofiEnabled;
    texture_.lofi_amount = clamp01(static_cast<float>(feel.lofiAmount) / 100.0f);
    texture_.drive_enabled = feel.driveEnabled;
    texture_.drive_amount = clamp01(static_cast<float>(feel.driveAmount) / 100.0f);
    texture_.tape_enabled = feel.tapeEnabled;
}

void FeelTexturePage::applyGridResolution() {
    withAudioGuard([&]() {
        auto& feel = mini_acid_.sceneManager().currentScene().feel;
        feel.gridSteps = static_cast<uint8_t>(grid_resolution_);
        mini_acid_.applyFeelTimingFromScene_();
    });
}

void FeelTexturePage::applyTimebase() {
    withAudioGuard([&]() {
        auto& feel = mini_acid_.sceneManager().currentScene().feel;
        feel.timebase = static_cast<uint8_t>(timebase_);
        mini_acid_.applyFeelTimingFromScene_();
    });
}

void FeelTexturePage::applyPatternLength() {
    withAudioGuard([&]() {
        auto& feel = mini_acid_.sceneManager().currentScene().feel;
        feel.patternBars = static_cast<uint8_t>(pattern_length_);
    });
}

void FeelTexturePage::applyTextureSettings() {
    withAudioGuard([&]() {
        auto& feel = mini_acid_.sceneManager().currentScene().feel;
        feel.lofiEnabled = texture_.lofi_enabled;
        feel.lofiAmount = static_cast<uint8_t>(clamp01(texture_.lofi_amount) * 100.0f + 0.5f);
        feel.driveEnabled = texture_.drive_enabled;
        feel.driveAmount = static_cast<uint8_t>(clamp01(texture_.drive_amount) * 100.0f + 0.5f);
        feel.tapeEnabled = texture_.tape_enabled;
        mini_acid_.applyTextureFromScene_();
    });
}

void FeelTexturePage::draw(IGfx& gfx) {
    syncFromScene();

    UI::drawStandardHeader(gfx, mini_acid_, title_.c_str());
    UI::drawFeelHeaderHud(gfx, mini_acid_, 166, 9);
    LayoutManager::clearContent(gfx);

    const int col1X = Layout::COL_1;
    const int col2X = Layout::COL_2;
    const int headerY = LayoutManager::lineY(0);

    // Focus markers (GenrePage-like)
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(col1X, headerY, (focus_ == FocusArea::FEEL) ? "F>" : "F ");
    gfx.drawText(col2X, headerY, (focus_ == FocusArea::TEXTURE) ? "T>" : "T ");

    // Lists
    drawGridSelector(gfx, col1X, LayoutManager::lineY(1));
    drawTimebaseSelector(gfx, col1X, LayoutManager::lineY(2));
    drawLengthSelector(gfx, col1X, LayoutManager::lineY(3));
    drawTextureControls(gfx, col2X, LayoutManager::lineY(1));

    // Presets row
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(col1X, LayoutManager::lineY(4), (focus_ == FocusArea::PRESETS) ? "P>" : "P ");
    drawPresets(gfx, col1X + 10, LayoutManager::lineY(5), Layout::CONTENT.w - 20);

    const char* left = "[TAB] FOCUS  [ARROWS] SELECT";
    const char* right = currentHint();
    UI::drawStandardFooter(gfx, left, right);
}

void FeelTexturePage::drawCursor(IGfx& gfx, int x, int y, int w, int h) {
    gfx.drawRect(x, y, w, h, COLOR_ACCENT);
}

void FeelTexturePage::drawGridSelector(IGfx& gfx, int x, int y) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "GRID  %s", gridToString(grid_resolution_));
    Widgets::drawListRow(gfx, x, y, Layout::COL_WIDTH, buf,
                         focus_ == FocusArea::FEEL && feel_row_ == 0);
}

void FeelTexturePage::drawTimebaseSelector(IGfx& gfx, int x, int y) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "TB    %s", timebaseToString(timebase_));
    Widgets::drawListRow(gfx, x, y, Layout::COL_WIDTH, buf,
                         focus_ == FocusArea::FEEL && feel_row_ == 1);
}

void FeelTexturePage::drawLengthSelector(IGfx& gfx, int x, int y) {
    char buf[32];
    int bars = mini_acid_.cycleBarCount();
    if (bars < 1) bars = 1;
    if (bars > 8) bars = 8;
    if (bars > 1) {
        int barIdx = mini_acid_.cycleBarIndex() + 1;
        if (barIdx < 1) barIdx = 1;
        if (barIdx > bars) barIdx = bars;
        std::snprintf(buf, sizeof(buf), "LEN   %s %d/%d", lengthToString(pattern_length_), barIdx, bars);
    } else {
        std::snprintf(buf, sizeof(buf), "LEN   %s", lengthToString(pattern_length_));
    }
    Widgets::drawListRow(gfx, x, y, Layout::COL_WIDTH, buf,
                         focus_ == FocusArea::FEEL && feel_row_ == 2);
}

void FeelTexturePage::drawTextureControls(IGfx& gfx, int x, int y) {
    char buf[24];
    int lofiPct = static_cast<int>(clamp01(texture_.lofi_amount) * 100.0f + 0.5f);
    if (texture_.lofi_enabled) {
        std::snprintf(buf, sizeof(buf), "LOFI  ON  %d%%", lofiPct);
    } else {
        std::snprintf(buf, sizeof(buf), "LOFI  OFF");
    }
    Widgets::drawListRow(gfx, x, y, Layout::COL_WIDTH, buf,
                         focus_ == FocusArea::TEXTURE && texture_row_ == 0);

    y += kRowH;
    int drivePct = static_cast<int>(clamp01(texture_.drive_amount) * 100.0f + 0.5f);
    if (texture_.drive_enabled) {
        std::snprintf(buf, sizeof(buf), "DRIVE ON  %d%%", drivePct);
    } else {
        std::snprintf(buf, sizeof(buf), "DRIVE OFF");
    }
    Widgets::drawListRow(gfx, x, y, Layout::COL_WIDTH, buf,
                         focus_ == FocusArea::TEXTURE && texture_row_ == 1);

    y += kRowH;
    std::snprintf(buf, sizeof(buf), "TAPE  %s", texture_.tape_enabled ? "ON" : "OFF");
    Widgets::drawListRow(gfx, x, y, Layout::COL_WIDTH, buf,
                         focus_ == FocusArea::TEXTURE && texture_row_ == 2);
}

void FeelTexturePage::drawPresets(IGfx& gfx, int x, int y, int width) {
    (void)width;
    Widgets::drawButtonGrid(gfx, x, y, 52, 10, 4, 1, kPresetNames, 4,
                            focus_ == FocusArea::PRESETS ? preset_index_ : -1);
}

int FeelTexturePage::maxRowForFocus(FocusArea focus) const {
    if (focus == FocusArea::FEEL) return 2;
    if (focus == FocusArea::TEXTURE) return 2;
    return 0;
}

bool FeelTexturePage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

    int nav = UIInput::navCode(ui_event);
    if (nav == GROOVEPUTER_LEFT) {
        if (focus_ == FocusArea::PRESETS) {
            preset_index_ = (preset_index_ + 3) % 4;
            return true;
        }
        if (focus_ == FocusArea::FEEL) {
            if (feel_row_ == 0) {
                grid_resolution_ = (grid_resolution_ == GridResolution::Eighth) ? GridResolution::ThirtySecond :
                                   (grid_resolution_ == GridResolution::Sixteenth) ? GridResolution::Eighth :
                                                                                      GridResolution::Sixteenth;
                applyGridResolution();
            } else if (feel_row_ == 1) {
                timebase_ = (timebase_ == Timebase::Half) ? Timebase::Double :
                            (timebase_ == Timebase::Normal) ? Timebase::Half : Timebase::Normal;
                applyTimebase();
            } else if (feel_row_ == 2) {
                pattern_length_ = (pattern_length_ == PatternLength::OneBar) ? PatternLength::EightBars :
                                  (pattern_length_ == PatternLength::TwoBars) ? PatternLength::OneBar :
                                  (pattern_length_ == PatternLength::FourBars) ? PatternLength::TwoBars :
                                                                               PatternLength::FourBars;
                applyPatternLength();
            }
            return true;
        }
        if (focus_ == FocusArea::TEXTURE) {
            if (texture_row_ == 0) texture_.lofi_enabled = !texture_.lofi_enabled;
            else if (texture_row_ == 1) texture_.drive_enabled = !texture_.drive_enabled;
            else if (texture_row_ == 2) texture_.tape_enabled = !texture_.tape_enabled;
            applyTextureSettings();
            return true;
        }
    }
    if (nav == GROOVEPUTER_RIGHT) {
        if (focus_ == FocusArea::PRESETS) {
            preset_index_ = (preset_index_ + 1) % 4;
            return true;
        }
        if (focus_ == FocusArea::FEEL) {
            if (feel_row_ == 0) {
                grid_resolution_ = (grid_resolution_ == GridResolution::Eighth) ? GridResolution::Sixteenth :
                                   (grid_resolution_ == GridResolution::Sixteenth) ? GridResolution::ThirtySecond :
                                                                                      GridResolution::Eighth;
                applyGridResolution();
            } else if (feel_row_ == 1) {
                timebase_ = (timebase_ == Timebase::Half) ? Timebase::Normal :
                            (timebase_ == Timebase::Normal) ? Timebase::Double : Timebase::Half;
                applyTimebase();
            } else if (feel_row_ == 2) {
                pattern_length_ = (pattern_length_ == PatternLength::OneBar) ? PatternLength::TwoBars :
                                  (pattern_length_ == PatternLength::TwoBars) ? PatternLength::FourBars :
                                  (pattern_length_ == PatternLength::FourBars) ? PatternLength::EightBars :
                                                                               PatternLength::OneBar;
                applyPatternLength();
            }
            return true;
        }
        if (focus_ == FocusArea::TEXTURE) {
            if (texture_row_ == 0) texture_.lofi_enabled = !texture_.lofi_enabled;
            else if (texture_row_ == 1) texture_.drive_enabled = !texture_.drive_enabled;
            else if (texture_row_ == 2) texture_.tape_enabled = !texture_.tape_enabled;
            applyTextureSettings();
            return true;
        }
    }
    if (nav == GROOVEPUTER_UP) {
        if (focus_ == FocusArea::FEEL) {
            if (feel_row_ > 0) feel_row_--;
            return true;
        }
        if (focus_ == FocusArea::TEXTURE) {
            if (texture_row_ > 0) texture_row_--;
            return true;
        }
        return false;
    }
    if (nav == GROOVEPUTER_DOWN) {
        if (focus_ == FocusArea::FEEL) {
            if (feel_row_ < maxRowForFocus(focus_)) feel_row_++;
            return true;
        }
        if (focus_ == FocusArea::TEXTURE) {
            if (texture_row_ < maxRowForFocus(focus_)) texture_row_++;
            return true;
        }
        return false;
    }

    char key = ui_event.key;
    if (key == '\t') {
        if (focus_ == FocusArea::FEEL) focus_ = FocusArea::TEXTURE;
        else if (focus_ == FocusArea::TEXTURE) focus_ = FocusArea::PRESETS;
        else focus_ = FocusArea::FEEL;
        return true;
    }
    if (key == '\n' || key == '\r' || key == ' ') {
        if (focus_ == FocusArea::FEEL) {
            if (feel_row_ == 0) {
                grid_resolution_ = (grid_resolution_ == GridResolution::Eighth) ? GridResolution::Sixteenth :
                                   (grid_resolution_ == GridResolution::Sixteenth) ? GridResolution::ThirtySecond :
                                                                                      GridResolution::Eighth;
                applyGridResolution();
            } else if (feel_row_ == 1) {
                timebase_ = (timebase_ == Timebase::Half) ? Timebase::Normal :
                            (timebase_ == Timebase::Normal) ? Timebase::Double : Timebase::Half;
                applyTimebase();
            } else if (feel_row_ == 2) {
                pattern_length_ = (pattern_length_ == PatternLength::OneBar) ? PatternLength::TwoBars :
                                  (pattern_length_ == PatternLength::TwoBars) ? PatternLength::FourBars :
                                  (pattern_length_ == PatternLength::FourBars) ? PatternLength::EightBars :
                                                                               PatternLength::OneBar;
                applyPatternLength();
            }
            return true;
        }
        if (focus_ == FocusArea::TEXTURE) {
            if (texture_row_ == 0) texture_.lofi_enabled = !texture_.lofi_enabled;
            else if (texture_row_ == 1) texture_.drive_enabled = !texture_.drive_enabled;
            else if (texture_row_ == 2) texture_.tape_enabled = !texture_.tape_enabled;
            applyTextureSettings();
            return true;
        }
        if (focus_ == FocusArea::PRESETS) {
            applyPreset(preset_index_);
            return true;
        }
    }

    if (key == '+' || key == '=') {
        if (focus_ == FocusArea::TEXTURE && texture_row_ == 0 && texture_.lofi_enabled) {
            texture_.lofi_amount = clamp01(texture_.lofi_amount + 0.1f);
            applyTextureSettings();
            return true;
        }
        if (focus_ == FocusArea::TEXTURE && texture_row_ == 1 && texture_.drive_enabled) {
            texture_.drive_amount = clamp01(texture_.drive_amount + 0.1f);
            applyTextureSettings();
            return true;
        }
    }

    if (key == '-' || key == '_') {
        if (focus_ == FocusArea::TEXTURE && texture_row_ == 0 && texture_.lofi_enabled) {
            texture_.lofi_amount = clamp01(texture_.lofi_amount - 0.1f);
            applyTextureSettings();
            return true;
        }
        if (focus_ == FocusArea::TEXTURE && texture_row_ == 1 && texture_.drive_enabled) {
            texture_.drive_amount = clamp01(texture_.drive_amount - 0.1f);
            applyTextureSettings();
            return true;
        }
    }

    if (key >= '1' && key <= '4') {
        preset_index_ = key - '1';
        applyPreset(preset_index_);
        return true;
    }

    return false;
}

void FeelTexturePage::applyPreset(int index) {
    if (index < 0 || index > 3) return;
    FeelSettings before = mini_acid_.sceneManager().currentScene().feel;
    withAudioGuard([&]() {
        auto& feel = mini_acid_.sceneManager().currentScene().feel;
        switch (index) {
            case 0: // SPACE (dub/slow baseline)
                feel.gridSteps = 16;
                feel.timebase = static_cast<uint8_t>(Timebase::Half);
                feel.patternBars = 4;
                feel.driveEnabled = false;
                feel.lofiEnabled = false;
                feel.tapeEnabled = false;
                break;
            case 1: // NORM
                feel.gridSteps = 16;
                feel.timebase = static_cast<uint8_t>(Timebase::Normal);
                feel.patternBars = 1;
                feel.driveEnabled = false;
                feel.lofiEnabled = false;
                feel.tapeEnabled = false;
                break;
            case 2: // WIDE
                feel.patternBars = 8;
                feel.gridSteps = 16;
                feel.timebase = static_cast<uint8_t>(Timebase::Normal);
                feel.driveEnabled = false;
                feel.lofiEnabled = false;
                feel.tapeEnabled = false;
                break;
            case 3: // GRIT
                feel.driveEnabled = true;
                feel.driveAmount = 45;
                feel.lofiEnabled = true;
                feel.lofiAmount = 35;
                break;
        }
        mini_acid_.applyFeelTimingFromScene_();
        mini_acid_.applyTextureFromScene_();
    });
    syncFromScene();

    char toast[64];
    const FeelSettings& after = mini_acid_.sceneManager().currentScene().feel;
    formatFeelDelta(before, after, toast, sizeof(toast));
    UI::showToast(toast, 1800);
}

const char* FeelTexturePage::currentHint() const {
    if (focus_ == FocusArea::FEEL) {
        if (feel_row_ == 0) return "GRID: 1/32 for low BPM";
        if (feel_row_ == 1) return "TB: DBL densifies feel";
        return "LEN: longer cycle, same 16 steps";
    }
    if (focus_ == FocusArea::TEXTURE) {
        if (texture_row_ == 0) return "LOFI: bitcrush texture";
        if (texture_row_ == 1) return "DRIVE: saturation";
        return "TAPE: FX only";
    }
    return (focus_ == FocusArea::PRESETS) ? "SPACE + LOOSE = Dub" : "1-4 apply preset";
}
