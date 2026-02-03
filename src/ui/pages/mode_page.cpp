#include "mode_page.h"
#include <cstdio>
#include "../ui_colors.h"

ModePage::ModePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
}

void ModePage::draw(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    GrooveboxMode currentMode = mini_acid_.grooveboxMode();
    const ModeConfig& cfg = mini_acid_.modeManager().config();

    // Clear background
    gfx.fillRect(x, y, w, h, COLOR_BLACK);

    // Big Mode Indicators
    int centerY = y + 25;  // Reduced from 30
    int boxW = 50;  // Reduced from 60
    int boxH = 24;  // Reduced from 30
    
    // Acid Box
    drawModeBox(gfx, x + 25, centerY, "ACID", currentMode == GrooveboxMode::Acid, kAcidConfig.accentColor, boxW, boxH);
    
    // Minimal Box
    drawModeBox(gfx, x + 160, centerY, "MNML", currentMode == GrooveboxMode::Minimal, kMinimalConfig.accentColor, boxW, boxH);

    // Arrow
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 105, centerY + 8, (currentMode == GrooveboxMode::Acid) ? "->" : "<-");

    // Mode Info
    int infoY = y + 55;  // Reduced from 75
    gfx.setTextColor(cfg.accentColor);
    gfx.drawText(x + 10, infoY, cfg.displayName);

    gfx.setTextColor(COLOR_WHITE);
    if (currentMode == GrooveboxMode::Acid) {
        gfx.drawText(x + 10, infoY + 6, "CHARACTER: AGGRESSIVE");
        
        gfx.setTextColor(kAcidConfig.accentColor);
       // gfx.drawText(x + 10, infoY + 20, "FOCUS: FREAK . RES . ACCENT");
    } else {
        gfx.drawText(x + 10, infoY + 6, "CHARACTER: HYPNOTIC");
        
        gfx.setTextColor(kMinimalConfig.accentColor);
       // gfx.drawText(x + 10, infoY + 20, "FOCUS: TIME . SPACE . DUB");
    }

    // Hints
    int hintY = y + 75;  // Reduced from 115
    gfx.setTextColor(COLOR_LABEL);
    //gfx.drawText(x + 10, hintY, "[ENTER] Toggle   [A] Apply to 303A");
    gfx.drawText(x + 10, hintY + 10, "[SPACE] Preview  [B] Apply to 303B");
}

bool ModePage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

    switch (ui_event.key) {
        case '\n':
        case '\r':
            toggleMode();
            return true;
        case 'a':
        case 'A':
            applyTo303(0);
            return true;
        case 'b':
        case 'B':
            applyTo303(1);
            return true;
        case ' ':
            previewMode();
            return true;
    }

    return false;
}

const std::string& ModePage::getTitle() const {
    return title_;
}

void ModePage::toggleMode() {
    withAudioGuard([&]() {
        mini_acid_.toggleGrooveboxMode();
        // Presets are now applied manually via 'A' / 'B' to avoid overwriting genre sound
        applyToTape();
    });
}

void ModePage::applyTo303(int voiceIdx) {
    withAudioGuard([&]() {
        mini_acid_.modeManager().apply303Preset(voiceIdx, voiceIdx == 0 ? 0 : 1);
    });
}

void ModePage::applyToTape() {
    withAudioGuard([&]() {
        int count;
        const TapeModePreset* presets = mini_acid_.modeManager().getTapePresets(count);
        if (count > 0) {
            mini_acid_.sceneManager().currentScene().tape.macro = presets[0].macro;
        }
    });
}

void ModePage::previewMode() {
    withAudioGuard([&]() {
        mini_acid_.randomize303Pattern(0);
        mini_acid_.randomize303Pattern(1);
        mini_acid_.randomizeDrumPattern();
        if (!mini_acid_.isPlaying()) {
            mini_acid_.start();
        }
    });
}

void ModePage::drawModeBox(IGfx& gfx, int x, int y, const char* name, bool active, uint16_t color, int w, int h) {
    if (active) {
        gfx.fillRect(x, y, w, h, (IGfxColor)color);
        gfx.setTextColor(COLOR_BLACK);
        gfx.drawText(x + 10, y + 7, name);
    } else {
        gfx.drawRect(x, y, w, h, (IGfxColor)color);
        gfx.setTextColor((IGfxColor)color);
        gfx.drawText(x + 10, y + 7, name);
    }
}


