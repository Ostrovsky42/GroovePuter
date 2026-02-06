#include "cassette_skin.h"
#include <cstdio>
#include <cstring>

CassetteSkin::CassetteSkin(IGfx& gfx, CassetteTheme theme)
    : gfx_(gfx), theme_(theme), palette_(&getPalette(theme)) {}

void CassetteSkin::setTheme(CassetteTheme theme) {
    theme_ = theme;
    palette_ = &getPalette(theme);
}

void CassetteSkin::tick() {
    // Advance reel animation every 4-6 frames
    ++tickCounter_;
    if (tickCounter_ >= 5) {
        tickCounter_ = 0;
        animFrame_ = (animFrame_ + 1) % 3;
    }
}

Rect CassetteSkin::contentBounds() const {
    int margin = 4;
    int x = margin;
    int y = headerHeight() + margin;
    int w = gfx_.width() - margin * 2;
    int h = gfx_.height() - headerHeight() - footerHeight() - margin * 2;
    // Account for panel frame border
    return Rect(x + 2, y + 2, w - 4, h - 4);
}

// ============================================================================
// Background with subtle dither pattern
// ============================================================================
void CassetteSkin::drawBackground() {
    const int w = gfx_.width();
    const int h = gfx_.height();
    uint32_t currentBg = palette_->bg.color24();

    // 1) Handle Line Cache Refresh
    if (linePlain_.empty() || lastBgColor_ != currentBg || linePlain_.size() != (size_t)w) {
        lastBgColor_ = currentBg;
        linePlain_.assign(w, palette_->bg.toCardputerColor());
        lineEven_.resize(w);
        lineOdd_.resize(w);

        uint16_t base = palette_->bg.toCardputerColor();
        uint16_t dark = IGfxColor((palette_->bg.color24() & 0xFEFEFE) - 0x060606).toCardputerColor();

        for (int x = 0; x < w; ++x) {
            lineEven_[x] = (x % 2 == 0) ? dark : base;
            lineOdd_[x]  = (x % 2 == 1) ? dark : base;
        }
    }

    // 2) Line-by-Line Rendering
    for (int y = 0; y < h; ++y) {
        if (y % 2 != 0) {
            gfx_.drawImage(0, y, linePlain_.data(), w, 1);
        } else {
            const auto& activeLine = ((y / 2) % 2 == 0) ? lineEven_ : lineOdd_;
            gfx_.drawImage(0, y, activeLine.data(), w, 1);
        }
    }
}

// ============================================================================
// Header - Cassette label style
// ============================================================================
void CassetteSkin::drawHeader(const HeaderState& state) {
    const int w = gfx_.width();
    const int h = headerHeight();
    const int margin = 2;
    
    // Label background (cream/panel color with border)
    gfx_.fillRect(margin, margin, w - margin * 2, h - margin, palette_->panel);
    gfx_.drawRect(margin, margin, w - margin * 2, h - margin, palette_->shadow);
    
    // Inner divider line
    int divider_y = margin + 8;  // Adjusted for 16px header
    gfx_.drawRect(margin + 2, divider_y, w - margin * 2 - 4, 1, palette_->shadow);
    
    // Top row: SCENE xxx BPM xxx MODE (no LOFI-SEQ branding - wastes space)
    char buf[64];
    int x = margin + 4;
    int y = margin + 1;
    
    // Scene - first element
    gfx_.setTextColor(palette_->ink);
    snprintf(buf, sizeof(buf), "SCENE %s", state.sceneName);
    gfx_.drawText(x, y, buf);
    x += gfx_.textWidth(buf) + 8;
    
    // BPM
    snprintf(buf, sizeof(buf), "BPM %d", state.bpm);
    gfx_.drawText(x, y, buf);
    
    // Mode indicator (right side)
    const char* modeText = state.isNavMode ? "NAV" : "EDIT";
    int mode_x = w - margin - 4 - gfx_.textWidth(modeText);
    if (state.isNavMode) {
        // LED dot for NAV mode
        drawLedDot(mode_x - 6, y + 3, true);
    } else {
        // Accent underline for EDIT mode
        int text_w = gfx_.textWidth(modeText);
        gfx_.fillRect(mode_x, y + gfx_.fontHeight(), text_w, 1, palette_->accent);
    }
    gfx_.setTextColor(palette_->ink);
    gfx_.drawText(mode_x, y, modeText);
    
    // Bottom row: PAT xxx SWG xx% FX:x TAPE:x SAVE
    x = margin + 4;
    y = divider_y + 2;
    
    snprintf(buf, sizeof(buf), "PAT %s", state.patternName);
    gfx_.setTextColor(palette_->ink);
    gfx_.drawText(x, y, buf);
    x += gfx_.textWidth(buf) + 6;
    
    snprintf(buf, sizeof(buf), "SWG %d%%", state.swingPercent);
    gfx_.setTextColor(palette_->muted);
    gfx_.drawText(x, y, buf);
    x += gfx_.textWidth(buf) + 6;
    
    // Status flags
    if (state.fxEnabled) {
        gfx_.setTextColor(palette_->accent);
        gfx_.drawText(x, y, "FX:ON");
    } else {
        gfx_.setTextColor(palette_->muted);
        gfx_.drawText(x, y, "FX:--");
    }
    x += gfx_.textWidth("FX:ON") + 4;
    
    if (state.tapeEnabled) {
        gfx_.setTextColor(palette_->accent);
        gfx_.drawText(x, y, "TAPE");
    }
    x += gfx_.textWidth("TAPE") + 4;
    
    // Recording indicator
    if (state.isRecording) {
        gfx_.setTextColor(palette_->accent);
        gfx_.drawText(x, y, "REC");
        // Blinking dot
        drawLedDot(x + gfx_.textWidth("REC") + 2, y + 3, (animFrame_ % 2) == 0);
    }
    
    gfx_.setTextColor(palette_->ink);
}

// ============================================================================
// Panel Frame - Double border with shadow
// ============================================================================
void CassetteSkin::drawPanelFrame(const Rect& bounds) {
    // Outer frame
    gfx_.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, palette_->shadow);
    
    // Inner frame (1px inset)
    gfx_.drawRect(bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2, palette_->shadow);
    
    // Shadow (2-3px on bottom/right)
    int shadow_offset = 2;
    gfx_.fillRect(bounds.x + shadow_offset, bounds.y + bounds.h, 
                  bounds.w, shadow_offset, palette_->shadow);
    gfx_.fillRect(bounds.x + bounds.w, bounds.y + shadow_offset,
                  shadow_offset, bounds.h, palette_->shadow);
    
    // Corner screws
    drawCornerScrew(bounds.x + 4, bounds.y + 4);
    drawCornerScrew(bounds.x + bounds.w - 6, bounds.y + 4);
    drawCornerScrew(bounds.x + 4, bounds.y + bounds.h - 6);
    drawCornerScrew(bounds.x + bounds.w - 6, bounds.y + bounds.h - 6);
}

void CassetteSkin::drawCornerScrew(int x, int y) {
    // Small cross pattern for screw
    gfx_.drawPixel(x, y, palette_->shadow);
    gfx_.drawPixel(x - 1, y, palette_->shadow);
    gfx_.drawPixel(x + 1, y, palette_->shadow);
    gfx_.drawPixel(x, y - 1, palette_->shadow);
    gfx_.drawPixel(x, y + 1, palette_->shadow);
}

// ============================================================================
// Footer - Minimal status bar (no more tape reels - every pixel matters!)
// ============================================================================
void CassetteSkin::drawFooterReels(const FooterState& state) {
    const int w = gfx_.width();
    const int h = footerHeight();
    const int y = gfx_.height() - h;
    const int margin = 4;
    
    // Simple background strip
    gfx_.fillRect(0, y, w, h, palette_->panel);
    gfx_.drawRect(0, y, w, 1, palette_->shadow);
    
    char buf[32];
    int text_y = y + 2;
    int x = margin;
    
    // Step counter with visual indicator
    gfx_.setTextColor(palette_->muted);
    gfx_.drawText(x, text_y, "=");  // Simple arrow
    x += gfx_.textWidth("=") + 1;
    
    snprintf(buf, sizeof(buf), "STEP %02d/%d", state.currentStep + 1, state.totalSteps);
    gfx_.setTextColor(palette_->ink);
    gfx_.drawText(x, text_y, buf);
    x += gfx_.textWidth(buf) + 12;
    
    // Song position
    snprintf(buf, sizeof(buf), "BAR: %02d", state.songPosition + 1);
    gfx_.drawText(x, text_y, buf);
    
    // Play/Stop state (right-aligned)
    const char* playText = state.isPlaying ? "PLAY" : "STOP";
    int play_x = w - margin - gfx_.textWidth(playText);
    
    if (state.isPlaying) {
        gfx_.setTextColor(palette_->led);
        // Draw tiny play indicator
        gfx_.drawPixel(play_x - 4, text_y + 2, palette_->led);
        gfx_.drawPixel(play_x - 4, text_y + 3, palette_->led);
        gfx_.drawPixel(play_x - 4, text_y + 4, palette_->led);
    } else {
        gfx_.setTextColor(palette_->muted);
    }
    gfx_.drawText(play_x, text_y, playText);
    
    gfx_.setTextColor(palette_->ink);
}

void CassetteSkin::drawReel(int cx, int cy, int radius) {
    // Outer circle
    gfx_.drawCircle(cx, cy, radius, palette_->shadow);
    
    // Inner hub
    gfx_.fillRect(cx - 1, cy - 1, 3, 3, palette_->shadow);
    
    // Three holes that rotate based on animFrame_
    // Positions at 0°, 120°, 240° plus rotation offset
    const int hole_dist = radius - 2;
    for (int i = 0; i < 3; ++i) {
        // Each frame rotates by ~40 degrees
        int angle_deg = i * 120 + animFrame_ * 40;
        // Simple approximation using lookup for sin/cos at 0,40,80,120...
        int dx = 0, dy = 0;
        switch (angle_deg % 360 / 40) {
            case 0: dx = hole_dist; dy = 0; break;
            case 1: dx = hole_dist * 3 / 4; dy = hole_dist * 2 / 3; break;
            case 2: dx = hole_dist / 4; dy = hole_dist; break;
            case 3: dx = -hole_dist / 4; dy = hole_dist; break;
            case 4: dx = -hole_dist * 3 / 4; dy = hole_dist * 2 / 3; break;
            case 5: dx = -hole_dist; dy = 0; break;
            case 6: dx = -hole_dist * 3 / 4; dy = -hole_dist * 2 / 3; break;
            case 7: dx = -hole_dist / 4; dy = -hole_dist; break;
            case 8: dx = hole_dist / 4; dy = -hole_dist; break;
            default: dx = hole_dist * 3 / 4; dy = -hole_dist * 2 / 3; break;
        }
        gfx_.drawPixel(cx + dx, cy + dy, palette_->bg);
    }
}

void CassetteSkin::drawTapeProgress(int x, int y, int width, int progress, int total) {
    if (total <= 0) total = 1;
    
    // Background tape line
    gfx_.fillRect(x, y, width, 3, palette_->shadow);
    
    // Progress fill
    int fill_w = (width * progress) / total;
    gfx_.fillRect(x, y, fill_w, 3, palette_->led);
    
    // Tick marks every 4 steps
    int step_w = width / total;
    for (int i = 4; i < total; i += 4) {
        int tick_x = x + (width * i) / total;
        gfx_.drawPixel(tick_x, y - 1, palette_->ink);
        gfx_.drawPixel(tick_x, y + 3, palette_->ink);
    }
}

void CassetteSkin::drawLedDot(int x, int y, bool active) {
    IGfxColor color = active ? palette_->led : palette_->muted;
    gfx_.fillRect(x, y, 3, 3, color);
}

// ============================================================================
// Focus Rectangle - LED-style highlight
// ============================================================================
void CassetteSkin::drawFocusRect(const Rect& rect, bool editMode) {
    // Slightly inverted/darker background
    // (just draw the outline, content already rendered)
    
    // LED-colored thin border
    IGfxColor borderColor = editMode ? palette_->accent : palette_->led;
    gfx_.drawRect(rect.x, rect.y, rect.w, rect.h, borderColor);
    
    // Small LED dot indicator on the left
    drawLedDot(rect.x - 5, rect.y + rect.h / 2 - 1, true);
}

void CassetteSkin::drawDoubleBorder(const Rect& bounds) {
    gfx_.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, palette_->shadow);
    gfx_.drawRect(bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2, palette_->shadow);
}
