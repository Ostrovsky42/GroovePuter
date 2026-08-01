#pragma once

#include <cstdio>

#include "display.h"
#include "ui_colors.h"
#include "ui_core.h"
#include "ui_input.h"
#include "workflow_mode.h"

class WorkspaceLauncherOverlay {
public:
    WorkspaceLauncherOverlay() = default;

    bool isVisible() const { return visible_; }

    void open(Workspace workspace) {
        visible_ = true;
        selected_ = entryForWorkspace(workspace);
        child_ = 0;
        page_request_ = -1;
        help_request_ = false;
    }

    void close() {
        visible_ = false;
        page_request_ = -1;
        help_request_ = false;
    }

    void toggle(Workspace workspace) {
        if (visible_) close();
        else open(workspace);
    }

    bool takePageRequest(int& page) {
        if (page_request_ < 0) return false;
        page = page_request_;
        page_request_ = -1;
        return true;
    }

    bool takeHelpRequest() {
        if (!help_request_) return false;
        help_request_ = false;
        return true;
    }

    bool handleEvent(UIEvent& event) {
        if (!visible_) return false;
        if (event.event_type != GROOVEPUTER_KEY_DOWN) return true;

        if (event.scancode == GROOVEPUTER_ESCAPE ||
            event.key == '`' || event.key == '\b') {
            close();
            return true;
        }

        if (event.meta && (event.key == 'm' || event.key == 'M')) {
            close();
            return true;
        }

        const int nav = UIInput::navCode(event);
        if (nav == GROOVEPUTER_UP) {
            selected_ = (selected_ + kEntryCount - 1) % kEntryCount;
            child_ = 0;
            return true;
        }
        if (nav == GROOVEPUTER_DOWN) {
            selected_ = (selected_ + 1) % kEntryCount;
            child_ = 0;
            return true;
        }
        if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
            const int count = childCount(selected_);
            if (count > 1) {
                child_ += (nav == GROOVEPUTER_RIGHT) ? 1 : -1;
                while (child_ < 0) child_ += count;
                while (child_ >= count) child_ -= count;
            }
            return true;
        }

        if (event.key == '\n' || event.key == '\r' || event.key == ' ') {
            activateSelection();
            return true;
        }

        return true;
    }

    void draw(IGfx& gfx) const {
        if (!visible_) return;

        const int w = gfx.width();
        const int h = gfx.height();
        const int headerH = 14;
        const int footerH = 12;
        const int leftX = 4;
        const int leftW = 94;
        const int rightX = 104;
        const int rightW = w - rightX - 4;
        const int rowH = 13;
        const int rowsY = headerH + 4;

        gfx.fillRect(0, 0, w, h, COLOR_BLACK);
        gfx.fillRect(0, 0, w, headerH, COLOR_DARKER);
        gfx.setTextColor(COLOR_ACCENT);
        gfx.drawText(4, 2, "GROOVEPUTER / NAV");

        char workspaceBuf[24];
        std::snprintf(workspaceBuf, sizeof(workspaceBuf), "[%s]", workspaceNameForEntry(selected_));
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(w - gfx.textWidth(workspaceBuf) - 4, 2, workspaceBuf);

        for (int i = 0; i < kEntryCount; ++i) {
            const int y = rowsY + i * rowH;
            const bool selected = i == selected_;
            if (selected) {
                gfx.fillRect(leftX, y - 1, leftW, rowH - 1, COLOR_ACCENT);
            }
            gfx.setTextColor(selected ? COLOR_BLACK : COLOR_WHITE);
            gfx.drawText(leftX + 4, y + 1, entryLabel(i));
        }

        const int panelY = rowsY;
        const int panelH = h - panelY - footerH - 2;
        gfx.drawRect(rightX, panelY - 1, rightW, panelH, COLOR_PANEL);

        gfx.setTextColor(COLOR_ACCENT);
        gfx.drawText(rightX + 5, panelY + 4, entryLabel(selected_));

        const int count = childCount(selected_);
        const int safeChild = (child_ < count) ? child_ : 0;
        gfx.setTextColor(COLOR_WHITE);
        gfx.drawText(rightX + 5, panelY + 20, childLabel(selected_, safeChild));

        const char* line1 = nullptr;
        const char* line2 = nullptr;
        descriptionLines(selected_, safeChild, line1, line2);
        gfx.setTextColor(COLOR_LABEL);
        if (line1) gfx.drawText(rightX + 5, panelY + 37, line1);
        if (line2) gfx.drawText(rightX + 5, panelY + 49, line2);

        if (count > 1) {
            char pos[24];
            std::snprintf(pos, sizeof(pos), "L/R  %d/%d", safeChild + 1, count);
            gfx.setTextColor(COLOR_ACCENT);
            gfx.drawText(rightX + 5, panelY + 67, pos);
        }

        gfx.fillRect(0, h - footerH, w, footerH, COLOR_DARKER);
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(4, h - footerH + 2, "UP/DN ENT  L/R SECTION");
        gfx.setTextColor(COLOR_WHITE);
        gfx.drawText(w - gfx.textWidth("ESC CLOSE") - 4, h - footerH + 2, "ESC CLOSE");
    }

private:
    static constexpr int kEntryCount = 7;

    static int entryForWorkspace(Workspace workspace) {
        switch (workspace) {
            case Workspace::Perform: return 0;
            case Workspace::Pattern: return 1;
            case Workspace::Arrange: return 2;
            case Workspace::Player: return 3;
            case Workspace::Groove: return 4;
        }
        return 0;
    }

    static const char* workspaceNameForEntry(int entry) {
        switch (entry) {
            case 0: return "PERFORM";
            case 1: return "PATTERN";
            case 2: return "ARRANGE";
            case 3: return "PLAYER";
            case 4: return "GROOVE";
            case 5: return "PROJECT";
            case 6: return "HELP";
            default: return "NAV";
        }
    }

    static const char* entryLabel(int entry) {
        switch (entry) {
            case 0: return "PERFORM";
            case 1: return "PATTERN";
            case 2: return "ARRANGE";
            case 3: return "MIDI PLAYER";
            case 4: return "GROOVE";
            case 5: return "PROJECT";
            case 6: return "HELP";
            default: return "?";
        }
    }

    static int childCount(int entry) {
        switch (entry) {
            case 1: return 4;
            case 4: return 4;
            default: return 1;
        }
    }

    static int childPage(int entry, int child) {
        switch (entry) {
            case 0: return WorkflowPages::kPerform;
            case 1:
                switch (child) {
                    case 1: return WorkflowPages::kSynthA;
                    case 2: return WorkflowPages::kSynthB;
                    case 3: return WorkflowPages::kDrums;
                    default: return WorkflowPages::kPattern;
                }
            case 2: return WorkflowPages::kArrange;
            case 3: return WorkflowPages::kPlayer;
            case 4:
                switch (child) {
                    case 1: return WorkflowPages::kMode;
                    case 2: return WorkflowPages::kFeelTexture;
                    case 3: return WorkflowPages::kGenerator;
                    default: return WorkflowPages::kGenre;
                }
            case 5: return WorkflowPages::kProject;
            default: return -1;
        }
    }

    static const char* childLabel(int entry, int child) {
        switch (entry) {
            case 0: return "LIVE INSTRUMENT";
            case 1:
                switch (child) {
                    case 1: return "SYNTH A";
                    case 2: return "SYNTH B";
                    case 3: return "DRUMS";
                    default: return "OVERVIEW";
                }
            case 2: return "SONG";
            case 3: return "NOW PLAYING";
            case 4:
                switch (child) {
                    case 1: return "MODE / FLAVOR";
                    case 2: return "FEEL / TEXTURE";
                    case 3: return "GENERATOR";
                    default: return "GENRE";
                }
            case 5: return "SCENES / SETUP";
            case 6: return "CONTROLS";
            default: return "";
        }
    }

    static void descriptionLines(int entry,
                                 int child,
                                 const char*& line1,
                                 const char*& line2) {
        line1 = "";
        line2 = "";
        switch (entry) {
            case 0:
                line1 = "A / B / DX / DRUMS";
                line2 = "live instrument";
                return;
            case 1:
                if (child == 0) {
                    line1 = "all lanes / 16 steps";
                    line2 = "open editor from hub";
                } else if (child == 1 || child == 2) {
                    line1 = "pattern + synth setup";
                    line2 = "TAB local section";
                } else {
                    line1 = "pattern / sound / auto";
                    line2 = "TAB local section";
                }
                return;
            case 2:
                line1 = "arrange / loop / marks";
                line2 = "song-level edit";
                return;
            case 3:
                line1 = "SD MIDI playback";
                line2 = "seek / tempo / route";
                return;
            case 4:
                if (child == 0) {
                    line1 = "genre/texture/recipe";
                    line2 = "musical direction";
                } else if (child == 1) {
                    line1 = "groove family/flavor";
                    line2 = "sound macro setup";
                } else if (child == 2) {
                    line1 = "grid/timebase/length";
                    line2 = "timing character";
                } else {
                    line1 = "notes/scale/timing";
                    line2 = "advanced generator";
                }
                return;
            case 5:
                line1 = "load / save / MIDI";
                line2 = "UI / groove / LED";
                return;
            case 6:
                line1 = "page-aware help";
                line2 = "shortcuts / controls";
                return;
            default:
                return;
        }
    }

    void activateSelection() {
        if (selected_ == 6) {
            help_request_ = true;
            visible_ = false;
            return;
        }
        page_request_ = childPage(selected_, child_);
        visible_ = false;
    }

    bool visible_ = false;
    int selected_ = 0;
    int child_ = 0;
    int page_request_ = -1;
    bool help_request_ = false;
};
