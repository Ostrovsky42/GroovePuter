#include "settings_page.h"
#include "../../../scenes.h"
#include "../ui_colors.h"
#include "../ui_input.h"
#include <cstdio>

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

    static constexpr SettingDesc kSettings[] = {
      { (int)SettingId::Swing,          "Swing",       SettingUIType::Slider01 },
      { (int)SettingId::VelocityRange,  "Vel Range",   SettingUIType::Slider01 },
      { (int)SettingId::GhostProb,      "Ghost Prob",  SettingUIType::Slider01 },
      { (int)SettingId::MicroTiming,    "MicroTime",   SettingUIType::Slider01 },
      { (int)SettingId::MinNotes,       "Min Notes",   SettingUIType::IntRange },
      { (int)SettingId::MaxNotes,       "Max Notes",   SettingUIType::IntRange },
      { (int)SettingId::MinOctave,      "Min Oct",     SettingUIType::IntRange },
      { (int)SettingId::MaxOctave,      "Max Oct",     SettingUIType::IntRange },
      { (int)SettingId::ScaleRoot,      "Scale Root",  SettingUIType::EnumNote },
      { (int)SettingId::ScaleType,      "Scale",       SettingUIType::EnumScale },
      { (int)SettingId::PreferDownbeats,"Downbeats",   SettingUIType::Toggle },
      { (int)SettingId::ScaleQuantize,  "Quantize",    SettingUIType::Toggle },
    };
    static constexpr int kSettingCount = sizeof(kSettings)/sizeof(kSettings[0]);
    static constexpr int kRowHeight = 12;
}

SettingsPage::SettingsPage(IGfx& gfx, MiniAcid& mini_acid) 
    : gfx_(gfx), mini_acid_(mini_acid), list_(kSettingCount, kRowHeight) {}

const std::string& SettingsPage::getTitle() const {
    static std::string title = "Generator Settings";
    return title;
}

void SettingsPage::draw(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    
    // Ensure visibility
    list_.ensureVisible(bounds.h);
    
    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    
    int x = bounds.x + 4;
    int w = bounds.w - 8;
    int vis = list_.visibleRows(bounds.h);
    
    for (int row = 0; row < vis; ++row) {
        int idx = list_.scroll() + row;
        if (idx >= kSettingCount) break;
        
        const auto& d = kSettings[idx];
        int y = bounds.y + row * kRowHeight;
        bool selected = (idx == list_.selected());
        
        drawSettingRow(gfx, x, y, w, params, d, selected);
    }
    
    drawScrollbar(gfx, bounds, list_.scroll(), list_.selected(), kSettingCount, vis);
    
    // Display index counter (Professional touch)
    if (bounds.w > 50) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d/%d", list_.selected() + 1, kSettingCount);
        int cw = gfx.textWidth(buf);
        // Draw in top right corner of the page area, or maybe floating?
        // Let's put it on the right edge of the current row if selected
        // Or better: just assume it's part of the standard UI header (which we don't control here).
        // Let's draw it small at the bottom right if there's space, or top right overlay.
        // For now, let's skip overlay to avoid clutter or draw it relative to selected row?
        // User asked for "3/12 right top".
        // The bounds passed to us are strictly the content area.
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(bounds.x + bounds.w - cw - 4, bounds.y, buf);
    }
}

void SettingsPage::drawScrollbar(IGfx& gfx, const Rect& b, int scroll, int sel, int total, int vis) {
  if (vis >= total) return;
  int sbH = std::max(4, b.h * vis / total);
  int sbY = b.y + (b.h * scroll) / total;
  // Ensure scrollbar stays within bounds
  if (sbY + sbH > b.y + b.h) sbY = b.y + b.h - sbH;
  
  gfx.fillRect(b.x + b.w - 2, sbY, 2, sbH, COLOR_ACCENT);
}

void SettingsPage::drawSettingRow(IGfx& gfx, int x, int y, int w,
                                  GeneratorParams& params,
                                  const SettingDesc& d,
                                  bool selected) {
  // Common style
  gfx.setTextColor(selected ? COLOR_ACCENT : COLOR_LABEL);

  // Fixed grid
  const int colLabel = x;
  // const int colValue = x + 80; // Used inside helpers
  const int h = 8;
  int settingW = w - 10; // Scrollbar spacing

  switch ((SettingId)d.id) {
    case SettingId::Swing:
      drawSlider(gfx, colLabel, y, settingW, h, params.swingAmount, d.label, selected); break;
    case SettingId::VelocityRange:
      drawSlider(gfx, colLabel, y, settingW, h, params.velocityRange, d.label, selected); break;
    case SettingId::GhostProb:
      drawSlider(gfx, colLabel, y, settingW, h, params.ghostNoteProbability, d.label, selected); break;
    case SettingId::MicroTiming:
      drawSlider(gfx, colLabel, y, settingW, h, params.microTimingAmount, d.label, selected); break;

    case SettingId::MinNotes: {
      char buf[16]; snprintf(buf, sizeof(buf), "%d", params.minNotes);
      drawValue(gfx, colLabel, y, settingW, h, buf, d.label, selected);
    } break;

    case SettingId::MaxNotes: {
      char buf[16]; snprintf(buf, sizeof(buf), "%d", params.maxNotes);
      drawValue(gfx, colLabel, y, settingW, h, buf, d.label, selected);
    } break;

    case SettingId::MinOctave: {
      char buf[16]; snprintf(buf, sizeof(buf), "%d", params.minOctave);
      drawValue(gfx, colLabel, y, settingW, h, buf, d.label, selected);
    } break;

    case SettingId::MaxOctave: {
      char buf[16]; snprintf(buf, sizeof(buf), "%d", params.maxOctave);
      drawValue(gfx, colLabel, y, settingW, h, buf, d.label, selected);
    } break;

    case SettingId::ScaleRoot:
      drawValue(gfx, colLabel, y, settingW, h, noteToString(params.scaleRoot), d.label, selected);
      break;

    case SettingId::ScaleType:
      drawValue(gfx, colLabel, y, settingW, h, scaleTypeToString(params.scale), d.label, selected);
      break;

    case SettingId::PreferDownbeats:
      drawToggle(gfx, colLabel, y, settingW, h, params.preferDownbeats, d.label, selected);
      break;

    case SettingId::ScaleQuantize:
      drawToggle(gfx, colLabel, y, settingW, h, params.scaleQuantize, d.label, selected);
      break;

    default: break;
  }
}

void SettingsPage::drawSlider(IGfx& gfx, int x, int y, int w, int h, float value, const char* label, bool selected) {
    gfx.drawText(x, y, label);
    
    int barX = x + 80;
    int barW = w - 85; 
    if (barW < 20) barW = 20; 
    
    gfx.drawRect(barX, y, barW, h, selected ? COLOR_ACCENT : COLOR_GRAY);
    int fillW = (int)(value * (barW - 2));
    if (fillW > 0) gfx.fillRect(barX + 1, y + 1, fillW, h - 2, COLOR_WAVE);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", value);
    // Draw value to the right/overlay?
    // Current layout puts it inside or next to it. 
    // Let's stick to simple bar for now or overlay text if selected?
    if (selected) {
         gfx.drawText(barX + barW + 2, y, buf);
    }
}

void SettingsPage::drawToggle(IGfx& gfx, int x, int y, int w, int h, bool value, const char* label, bool selected) {
    gfx.drawText(x, y, label);
    // Align right
    gfx.drawText(x + 80, y, value ? "[ON]" : "[OFF]");
}

void SettingsPage::drawValue(IGfx& gfx, int x, int y, int w, int h, const char* valueStr, const char* label, bool selected) {
    gfx.drawText(x, y, label);
    gfx.drawText(x + 80, y, valueStr);
}

bool SettingsPage::handleEvent(UIEvent& e) {
    if (e.event_type != MINIACID_KEY_DOWN) return false;

    const Rect& b = getBoundaries();
    const bool wrap = true;
    const bool fast = e.shift;
    const int dirUp = -1;
    const int dirDn = +1;

    int nav = UIInput::navCode(e);
    if (nav == MINIACID_UP) {
        if (fast) list_.page(dirUp, b.h, wrap);
        else      list_.move(dirUp, wrap);
        return true;
    }
    if (nav == MINIACID_DOWN) {
        if (fast) list_.page(dirDn, b.h, wrap);
        else      list_.move(dirDn, wrap);
        return true;
    }
    
    if (nav == MINIACID_LEFT) { 
        adjustSetting(-1, fast); 
        return true; 
    }
    if (nav == MINIACID_RIGHT) { 
        adjustSetting(1, fast); 
        return true; 
    }
    
    return false;
}

void SettingsPage::adjustSetting(int delta, bool shift) {
    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    SettingId id = kSettings[list_.selected()].id == (int)SettingId::Swing ? SettingId::Swing : (SettingId)kSettings[list_.selected()].id;
    // The above line is silly, just cast
    id = (SettingId)kSettings[list_.selected()].id;
    
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
