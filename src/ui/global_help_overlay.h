#pragma once

#include "display.h"
#include "ui_colors.h"
#include "ui_core.h"
#include "global_help_content.h"

class GlobalHelpOverlay {
public:
    GlobalHelpOverlay() = default;
    void setPageContext(int pageIndex) { page_index_ = pageIndex; }
    
    bool isVisible() const { return visible_; }
    
    void toggle() { 
        visible_ = !visible_; 
        if (visible_) scroll_line_ = 0;  // Reset scroll on open
    }
    
    void close() { visible_ = false; }
    
    void draw(IGfx& gfx) {
        if (!visible_) return;
        
        int w = gfx.width();
        int h = gfx.height();
        
        // Fullscreen dark overlay
        gfx.fillRect(0, 0, w, h, COLOR_BLACK);
        
        // Title bar
        int title_h = 14;
        gfx.fillRect(0, 0, w, title_h, COLOR_DARKER);
        gfx.setTextColor(COLOR_ACCENT);
        const char* title = "HELP (ESC or Ctrl+H)";
        int title_x = (w - gfx.textWidth(title)) / 2;
        gfx.drawText(title_x, 2, title);
        
        // Content area
        int content_y = title_h + 2;
        int content_h = h - content_y - 2;
        int line_h = gfx.fontHeight() + 2;
        if (line_h < 10) line_h = 10;
        
        int visible_lines = content_h / line_h;
        int total_lines = HelpContent::getTotalLines(page_index_);
        
        // Clamp scroll
        int max_scroll = total_lines - visible_lines;
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_line_ > max_scroll) scroll_line_ = max_scroll;
        if (scroll_line_ < 0) scroll_line_ = 0;
        
        // Draw lines
        int y = content_y;
        for (int i = 0; i < visible_lines && (scroll_line_ + i) < total_lines; i++) {
            const char* line = HelpContent::getLine(page_index_, scroll_line_ + i);
            if (!line) continue;
            
            // Section headers in accent color
            if (line[0] == '=') {
                gfx.setTextColor(COLOR_ACCENT);
            } else {
                gfx.setTextColor(COLOR_WHITE);
            }
            
            gfx.drawText(4, y, line);
            y += line_h;
        }
        
        // Scrollbar
        if (total_lines > visible_lines) {
            int bar_x = w - 4;
            int bar_h = content_h;
            int thumb_h = (visible_lines * bar_h) / total_lines;
            if (thumb_h < 8) thumb_h = 8;
            int thumb_y = content_y + (scroll_line_ * (bar_h - thumb_h)) / max_scroll;
            
            gfx.drawLine(bar_x, content_y, bar_x, content_y + bar_h - 1, COLOR_GRAY);
            gfx.fillRect(bar_x - 1, thumb_y, 3, thumb_h, COLOR_LABEL);
        }
        
        gfx.setTextColor(COLOR_WHITE);
    }
    
    bool handleEvent(UIEvent& event) {
        if (!visible_) return false;
        
        if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
        
        // Close on Escape or Ctrl+H
        if (event.scancode == GROOVEPUTER_ESCAPE) {
            close();
            return true;
        }
        
        if (event.ctrl && (event.key == 'h' || event.key == 'H')) {
            close();
            return true;
        }
        
        // Scroll
        int scroll_step = 1;
        
        switch (event.scancode) {
            case GROOVEPUTER_UP:
                scroll_line_ -= scroll_step;
                if (scroll_line_ < 0) scroll_line_ = 0;
                return true;
                
            case GROOVEPUTER_DOWN:
                scroll_line_ += scroll_step;
                return true;
                
            case GROOVEPUTER_LEFT:
                scroll_line_ = 0;  // Go to top
                return true;
                
            case GROOVEPUTER_RIGHT:
                scroll_line_ = HelpContent::getTotalLines(page_index_);  // Go to bottom
                return true;
                
            default:
                break;
        }
        
        // Consume all other keys while help is open
        return true;
    }
    
private:
    bool visible_ = false;
    int scroll_line_ = 0;
    int page_index_ = -1;
};
