#include "settings_page.h"
#include "../../../scenes.h"
#include "../ui_colors.h"
#include <cstdio>

namespace {
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
}

SettingsPage::SettingsPage(IGfx& gfx, MiniAcid& mini_acid) 
    : gfx_(gfx), mini_acid_(mini_acid) {}

const std::string& SettingsPage::getTitle() const {
    static std::string title = "Generator Settings";
    return title;
}

void SettingsPage::draw(IGfx& gfx) {
    gfx.clear(IGfxColor::Black()); 
    
    const int startY = 25;
    const int rowHeight = 12; // Compact rows
    const int labelWidth = 80;
    const int valueWidth = 140;
    
    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    
    for (int i = 0; i < (int)SettingId::Count; ++i) {
        SettingId id = (SettingId)i;
        int y = startY + i * rowHeight;
        bool selected = (i == selection_index_);
        
        switch (id) {
            case SettingId::Swing:
                drawSlider(gfx, 10, y, 220, 8, params.swingAmount, "Swing", selected);
                break;
            case SettingId::VelocityRange:
                drawSlider(gfx, 10, y, 220, 8, params.velocityRange, "Vel Range", selected);
                break;
            case SettingId::GhostProb:
                drawSlider(gfx, 10, y, 220, 8, params.ghostNoteProbability, "Ghost Prob", selected);
                break;
            case SettingId::MicroTiming:
                drawSlider(gfx, 10, y, 220, 8, params.microTimingAmount, "MicroTime", selected);
                break;
            case SettingId::MinNotes: 
                {
                    char buf[16]; snprintf(buf, sizeof(buf), "%d", params.minNotes);
                    drawValue(gfx, 10, y, 220, 8, buf, "Min Notes", selected);
                }
                break;
            case SettingId::MaxNotes:
                {
                    char buf[16]; snprintf(buf, sizeof(buf), "%d", params.maxNotes);
                    drawValue(gfx, 10, y, 220, 8, buf, "Max Notes", selected);
                }
                break;
            case SettingId::MinOctave:
                {
                    char buf[16]; snprintf(buf, sizeof(buf), "%d", params.minOctave);
                    drawValue(gfx, 10, y, 220, 8, buf, "Min Oct", selected);
                }
                break;
            case SettingId::MaxOctave:
                {
                    char buf[16]; snprintf(buf, sizeof(buf), "%d", params.maxOctave);
                    drawValue(gfx, 10, y, 220, 8, buf, "Max Oct", selected);
                }
                break;
            case SettingId::ScaleRoot:
                drawValue(gfx, 10, y, 220, 8, noteToString(params.scaleRoot), "Scale Root", selected);
                break;
            case SettingId::ScaleType:
                drawValue(gfx, 10, y, 220, 8, scaleTypeToString(params.scale), "Scale", selected);
                break;
            case SettingId::PreferDownbeats:
                drawToggle(gfx, 10, y, 220, 8, params.preferDownbeats, "Downbeats", selected);
                break;
            case SettingId::ScaleQuantize:
                drawToggle(gfx, 10, y, 220, 8, params.scaleQuantize, "Quantize", selected);
                break;
            default: break;
        }
    }
}

void SettingsPage::drawSlider(IGfx& gfx, int x, int y, int w, int h, float value, const char* label, bool selected) {
    if (selected) gfx.setTextColor(COLOR_ACCENT);
    else gfx.setTextColor(COLOR_LABEL);
    
    gfx.drawText(x, y, label);
    
    int barX = x + 80;
    int barW = w - 120;
    gfx.drawRect(barX, y, barW, h, selected ? COLOR_ACCENT : COLOR_GRAY);
    int fillW = (int)(value * (barW - 2));
    if (fillW > 0) gfx.fillRect(barX + 1, y + 1, fillW, h - 2, COLOR_WAVE);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", value);
    gfx.drawText(barX + barW + 5, y, buf);
}

void SettingsPage::drawToggle(IGfx& gfx, int x, int y, int w, int h, bool value, const char* label, bool selected) {
    if (selected) gfx.setTextColor(COLOR_ACCENT);
    else gfx.setTextColor(COLOR_LABEL);
    
    gfx.drawText(x, y, label);
    gfx.drawText(x + 80, y, value ? "[ON]" : "[OFF]");
}

void SettingsPage::drawValue(IGfx& gfx, int x, int y, int w, int h, const char* valueStr, const char* label, bool selected) {
    if (selected) gfx.setTextColor(COLOR_ACCENT);
    else gfx.setTextColor(COLOR_LABEL);
    
    gfx.drawText(x, y, label);
    gfx.drawText(x + 80, y, valueStr);
}

bool SettingsPage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type == MINIACID_KEY_DOWN) {
        if (ui_event.scancode == MINIACID_UP) {
            selection_index_--;
            if (selection_index_ < 0) selection_index_ = (int)SettingId::Count - 1;
            return true;
        } else if (ui_event.scancode == MINIACID_DOWN) {
            selection_index_++;
            if (selection_index_ >= (int)SettingId::Count) selection_index_ = 0;
            return true;
        } else if (ui_event.scancode == MINIACID_LEFT) {
            adjustSetting(-1);
            return true;
        } else if (ui_event.scancode == MINIACID_RIGHT) {
            adjustSetting(1);
            return true;
        } else if (ui_event.scancode == MINIACID_ESCAPE) {
            // Navigation handled by display manager
        }
    }
    return false;
}

void SettingsPage::adjustSetting(int delta) {
    Scene& scene = mini_acid_.sceneManager().currentScene();
    GeneratorParams& params = scene.generatorParams;
    SettingId id = (SettingId)selection_index_;
    
    float fDelta = delta * 0.05f;
    
    switch (id) {
        case SettingId::Swing:
            params.swingAmount += fDelta;
            if(params.swingAmount < 0) params.swingAmount = 0;
            if(params.swingAmount > 1) params.swingAmount = 1;
            break;
        case SettingId::VelocityRange:
            params.velocityRange += fDelta;
            if(params.velocityRange < 0) params.velocityRange = 0;
            if(params.velocityRange > 1) params.velocityRange = 1;
            break;
        case SettingId::GhostProb:
            params.ghostNoteProbability += fDelta;
            if(params.ghostNoteProbability < 0) params.ghostNoteProbability = 0;
            if(params.ghostNoteProbability > 1) params.ghostNoteProbability = 1;
            break;
        case SettingId::MicroTiming:
            params.microTimingAmount += fDelta;
            if(params.microTimingAmount < 0) params.microTimingAmount = 0;
            if(params.microTimingAmount > 1) params.microTimingAmount = 1;
            break;
        case SettingId::MinNotes: 
            params.minNotes += delta;
            if(params.minNotes < 1) params.minNotes = 1;
            if(params.minNotes > params.maxNotes) params.minNotes = params.maxNotes;
            break;
        case SettingId::MaxNotes:
            params.maxNotes += delta;
            if(params.maxNotes < params.minNotes) params.maxNotes = params.minNotes;
            if(params.maxNotes > 16) params.maxNotes = 16;
            break;
        case SettingId::MinOctave:
            params.minOctave += delta;
            if(params.minOctave < 0) params.minOctave = 0;
            if(params.minOctave > params.maxOctave) params.minOctave = params.maxOctave;
            break;
        case SettingId::MaxOctave:
            params.maxOctave += delta;
            if(params.maxOctave < params.minOctave) params.maxOctave = params.minOctave;
            if(params.maxOctave > 10) params.maxOctave = 10;
            break;
        case SettingId::ScaleRoot:
            params.scaleRoot = (params.scaleRoot + delta + 12) % 12;
            break;
        case SettingId::ScaleType:
            {
                int val = (int)params.scale + delta;
                if (val < 0) val = 9; // Approx, verify enum
                if (val > 9) val = 0;
                params.scale = (ScaleType)val;
            }
            break;
        case SettingId::PreferDownbeats:
            params.preferDownbeats = !params.preferDownbeats;
            break;
        case SettingId::ScaleQuantize:
            params.scaleQuantize = !params.scaleQuantize;
            break;
        default: break;
    }
}
