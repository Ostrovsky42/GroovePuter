#pragma once

#include "../help_dialog_frames.h"
#include "../ui_core.h"

class IMultiHelpFramesProvider {
    public:
        virtual int getHelpFrameCount() const = 0;
        virtual void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const = 0;
};

class MultiPageHelpDialog : public Frame, public EventHandler {
 public:
    MultiPageHelpDialog(const IMultiHelpFramesProvider& provider) : provider_(provider), current_frame_index_(0) {}
    ~MultiPageHelpDialog() = default;
    
    void setExitRequestedCallback(std::function<void()> callback) {
        exit_requested_callback_ = std::move(callback);
    }

    void draw(IGfx& gfx) override {
        const Rect& bounds = getBoundaries();
        int x = bounds.x;
        int y = bounds.y;
        int w = bounds.w;
        int h = bounds.h;
        if (w <= 4 || h <= 4) return;

        int totalFrames = provider_.getHelpFrameCount();
        if (totalFrames <= 0) return;

        int dialog_margin = 2;
        int dialog_x = x + dialog_margin;
        int dialog_y = y + dialog_margin;
        int dialog_w = w - dialog_margin * 2;
        int dialog_h = h - dialog_margin * 2;
        if (dialog_w <= 4 || dialog_h <= 4) return;

        gfx.fillRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_DARKER);
        gfx.drawRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_WHITE);

        int legend_h = gfx.fontHeight() + 4;
        if (legend_h < 10) legend_h = 10;
        int legend_y = dialog_y + dialog_h - legend_h;
        if (legend_y <= dialog_y + 2) return;

        gfx.setTextColor(COLOR_LABEL);
        gfx.drawLine(dialog_x + 2, legend_y, dialog_x + dialog_w - 3, legend_y, COLOR_GRAY);

        const char* legend = "push ESC to close";
        int legend_x = dialog_x + (dialog_w - textWidth(gfx, legend)) / 2;
        if (legend_x < dialog_x + 4) legend_x = dialog_x + 4;
        int legend_text_y = legend_y + (legend_h - gfx.fontHeight()) / 2;
        gfx.drawText(legend_x, legend_text_y, legend);
        gfx.setTextColor(COLOR_WHITE);

        int body_x = dialog_x + 4;
        int body_y = dialog_y + 4;
        int body_w = dialog_w - 8;
        int body_h = legend_y - body_y - 2;
        if (body_w <= 0 || body_h <= 0) return;

        provider_.drawHelpFrame(gfx, current_frame_index_, Rect(body_x, body_y, body_w, body_h));
        drawHelpScrollbar(gfx, body_x, body_y, body_w, body_h, current_frame_index_, totalFrames);
    }
    
    bool handleEvent(UIEvent& ui_event) override {
        if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
        int totalFrames = provider_.getHelpFrameCount();
        if (totalFrames <= 0) return false;

        int next = current_frame_index_;
        switch (ui_event.scancode) {
            case MINIACID_LEFT:
            case MINIACID_UP:
                next -= 1;
                break;
            case MINIACID_RIGHT:
            case MINIACID_DOWN:
                next += 1;
                break;
            
            case MINIACID_ESCAPE:
                if (exit_requested_callback_) {
                    exit_requested_callback_();
                    return true;
                }
                break;
            default:
                return false;
        }
        if (next < 0) next = 0;
        if (next >= totalFrames) next = totalFrames - 1;
        current_frame_index_ = next;
        return true;
    };
    
 private:
    const IMultiHelpFramesProvider& provider_;
    int current_frame_index_;
    std::function<void()> exit_requested_callback_;
};
