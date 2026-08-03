#pragma once

#include <cstdio>
#include <cstdint>

#include "display.h"
#include "ui_core.h"
#include "ui_input.h"
#include "ui_theme.h"
#include "workflow_mode.h"

class WorkspaceLauncherOverlay {
public:
    WorkspaceLauncherOverlay() = default;

    bool isVisible() const { return visible_; }

    void open(Workspace workspace,
              const int8_t* rememberedPages = nullptr,
              int rememberedPageCount = 0) {
        loadRememberedPages_(rememberedPages, rememberedPageCount);
        visible_ = true;
        selected_ = entryForWorkspace(workspace);
        const int currentPage = WorkflowPages::pageForWorkspace(workspace);
        child_ = WorkflowPages::pageIndexInMode(currentPage);
        if (selected_ >= 0 && selected_ < kWorkflowEntryCount) {
            child_by_workflow_[selected_] = child_;
        }
        page_request_ = -1;
        help_request_ = false;
    }

    void close() {
        visible_ = false;
        page_request_ = -1;
        help_request_ = false;
    }

    void toggle(Workspace workspace,
                const int8_t* rememberedPages = nullptr,
                int rememberedPageCount = 0) {
        if (visible_) close();
        else open(workspace, rememberedPages, rememberedPageCount);
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

        if (event.alt && (event.key == '\\' || event.key == '|')) {
            UI::currentStyle = UI::nextThemeStyle(UI::currentStyle);
            return true;
        }

        const int nav = UIInput::navCode(event);
        if (nav == GROOVEPUTER_UP) {
            selected_ = (selected_ + kEntryCount - 1) % kEntryCount;
            child_ = rememberedChild_(selected_);
            return true;
        }
        if (nav == GROOVEPUTER_DOWN) {
            selected_ = (selected_ + 1) % kEntryCount;
            child_ = rememberedChild_(selected_);
            return true;
        }
        if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
            const int count = childCount(selected_);
            if (count > 1) {
                child_ += (nav == GROOVEPUTER_RIGHT) ? 1 : -1;
                while (child_ < 0) child_ += count;
                while (child_ >= count) child_ -= count;
                if (selected_ < kWorkflowEntryCount) {
                    child_by_workflow_[selected_] = child_;
                }
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

        const UI::ThemePalette p = UI::themePalette();
        const int w = gfx.width();
        const int h = gfx.height();
        const int headerH = 14;
        const int footerH = 12;
        const int leftX = 4;
        const int leftW = 88;
        const int rightX = 98;
        const int rightW = w - rightX - 4;
        const int rowH = 14;
        const int rowsY = headerH + 4;

        gfx.fillRect(0, 0, w, h, p.background);
        gfx.fillRect(0, 0, w, headerH, p.panel);
        gfx.drawLine(0, headerH - 1, w - 1, headerH - 1, p.dim);
        gfx.setTextColor(p.accent);
        gfx.drawText(4, 2, "GROOVEPUTER / NAV R3");

        char themeBuf[20];
        std::snprintf(themeBuf, sizeof(themeBuf), "[%s]", UI::themeName(UI::currentStyle));
        gfx.setTextColor(p.secondary);
        gfx.drawText(w - gfx.textWidth(themeBuf) - 4, 2, themeBuf);

        for (int i = 0; i < kEntryCount; ++i) {
            const int y = rowsY + i * rowH;
            const bool selected = i == selected_;
            if (selected) {
                gfx.fillRect(leftX, y - 1, leftW, rowH - 1, p.focus);
            }
            gfx.setTextColor(selected ? p.invert : p.text);
            gfx.drawText(leftX + 4, y + 1, entryLabel(i));
        }

        const int panelY = rowsY;
        const int panelH = h - panelY - footerH - 2;
        gfx.fillRect(rightX, panelY - 1, rightW, panelH, p.inset);
        gfx.drawRect(rightX, panelY - 1, rightW, panelH, p.dim);

        gfx.setTextColor(p.accent);
        gfx.drawText(rightX + 5, panelY + 4, entryLabel(selected_));

        const int count = childCount(selected_);
        const int safeChild = (child_ < count) ? child_ : 0;
        gfx.setTextColor(p.text);
        gfx.drawText(rightX + 5, panelY + 20, childLabel(selected_, safeChild));

        const char* line1 = nullptr;
        const char* line2 = nullptr;
        descriptionLines(selected_, safeChild, line1, line2);
        gfx.setTextColor(p.secondary);
        if (line1) gfx.drawText(rightX + 5, panelY + 37, line1);
        if (line2) gfx.drawText(rightX + 5, panelY + 49, line2);

        if (selected_ < kWorkflowEntryCount) {
            char pos[24];
            std::snprintf(pos, sizeof(pos), "PAGE %d/%d", safeChild + 1, count);
            gfx.setTextColor(p.accent2);
            gfx.drawText(rightX + 5, panelY + 67, pos);

            gfx.setTextColor(p.dim);
            gfx.drawText(rightX + 5, panelY + 80,
                         count > 1 ? "[ ] PAGE  FN+[ ] FLOW" : "FN+[ ] WORKFLOW");
        }

        char memory[32];
        std::snprintf(memory, sizeof(memory), "MEM %d %d %d %d %d",
                      child_by_workflow_[0] + 1,
                      child_by_workflow_[1] + 1,
                      child_by_workflow_[2] + 1,
                      child_by_workflow_[3] + 1,
                      child_by_workflow_[4] + 1);
        gfx.setTextColor(p.dim);
        gfx.drawText(rightX + 5, panelY + 92, memory);

        gfx.fillRect(0, h - footerH, w, footerH, p.panel);
        gfx.drawLine(0, h - footerH, w - 1, h - footerH, p.dim);
        gfx.setTextColor(p.secondary);
        gfx.drawText(4, h - footerH + 2, "UP/DN ENT  L/R PAGE");
        gfx.setTextColor(p.text);
        gfx.drawText(w - gfx.textWidth("ALT+\\ THEME") - 4,
                     h - footerH + 2, "ALT+\\ THEME");
    }

private:
    static constexpr int kWorkflowEntryCount = 5;
    static constexpr int kEntryCount = 6;

    void loadRememberedPages_(const int8_t* pages, int count) {
        if (!pages || count <= 0) return;
        const int safeCount = count < kWorkflowEntryCount
            ? count
            : kWorkflowEntryCount;
        for (int entry = 0; entry < safeCount; ++entry) {
            const int page = static_cast<int>(pages[entry]);
            if (WorkflowPages::modeForPage(page) != entryMode(entry)) continue;
            child_by_workflow_[entry] =
                WorkflowPages::pageIndexInMode(page);
        }
    }

    int rememberedChild_(int entry) const {
        if (entry < 0 || entry >= kWorkflowEntryCount) return 0;
        const int count = childCount(entry);
        const int child = child_by_workflow_[entry];
        return child >= 0 && child < count ? child : 0;
    }

    static int entryForWorkspace(Workspace workspace) {
        switch (WorkflowPages::modeForWorkspace(workspace)) {
            case WorkflowMode::Perform: return 0;
            case WorkflowMode::Generate: return 1;
            case WorkflowMode::Hub: return 2;
            case WorkflowMode::Song: return 3;
            case WorkflowMode::Settings: return 4;
        }
        return 1;
    }

    static WorkflowMode entryMode(int entry) {
        switch (entry) {
            case 0: return WorkflowMode::Perform;
            case 1: return WorkflowMode::Generate;
            case 2: return WorkflowMode::Hub;
            case 3: return WorkflowMode::Song;
            case 4: return WorkflowMode::Settings;
            default: return WorkflowMode::Generate;
        }
    }

    static const char* entryLabel(int entry) {
        switch (entry) {
            case 0: return "PERFORM";
            case 1: return "GENERATE";
            case 2: return "HUB";
            case 3: return "SONG";
            case 4: return "SETTINGS";
            case 5: return "HELP";
            default: return "?";
        }
    }

    static int childCount(int entry) {
        if (entry >= kWorkflowEntryCount) return 1;
        return WorkflowPages::pageCountForMode(entryMode(entry));
    }

    static int childPage(int entry, int child) {
        if (entry >= kWorkflowEntryCount) return -1;
        return WorkflowPages::pageAt(entryMode(entry), child);
    }

    static const char* childLabel(int entry, int child) {
        if (entry == 5) return "CONTROLS";
        return WorkflowPages::pageName(childPage(entry, child));
    }

    static void descriptionForPage(int page,
                                   const char*& line1,
                                   const char*& line2) {
        switch (page) {
            case WorkflowPages::kPerform:
                line1 = "A / B / DX / DRUMS";
                line2 = "live MIDI keyboard";
                return;
            case WorkflowPages::kPlayer:
                line1 = "SD MIDI playback";
                line2 = "tempo / route / progress";
                return;
            case WorkflowPages::kGenre:
                line1 = "genre / texture / recipe";
                line2 = "musical direction";
                return;
            case WorkflowPages::kMode:
                line1 = "groove family / flavor";
                line2 = "macro generation style";
                return;
            case WorkflowPages::kFeelTexture:
                line1 = "grid / timebase / length";
                line2 = "timing character";
                return;
            case WorkflowPages::kGenerator:
                line1 = "swing / notes / scale";
                line2 = "TAB local groups";
                return;
            case WorkflowPages::kPattern:
                line1 = "all lanes / 16 steps";
                line2 = "open concrete editor";
                return;
            case WorkflowPages::kSynthA:
            case WorkflowPages::kSynthB:
                line1 = "pattern editor";
                line2 = "TAB local section";
                return;
            case WorkflowPages::kDrums:
                line1 = "pattern / sound / auto";
                line2 = "TAB local section";
                return;
            case WorkflowPages::kSynthAParameters:
            case WorkflowPages::kSynthBParameters:
                line1 = "synth sound controls";
                line2 = "live-note capable";
                return;
            case WorkflowPages::kArrange:
                line1 = "arrange / loop / marks";
                line2 = "song-level edit";
                return;
            case WorkflowPages::kProject:
                line1 = "load / save / MIDI";
                line2 = "UI / routing / LED";
                return;
            default:
                line1 = "";
                line2 = "";
                return;
        }
    }

    static void descriptionLines(int entry,
                                 int child,
                                 const char*& line1,
                                 const char*& line2) {
        if (entry == 5) {
            line1 = "page-aware help";
            line2 = "shortcuts / controls";
            return;
        }
        descriptionForPage(childPage(entry, child), line1, line2);
    }

    void activateSelection() {
        if (selected_ == 5) {
            help_request_ = true;
            visible_ = false;
            return;
        }
        if (selected_ < kWorkflowEntryCount) {
            child_by_workflow_[selected_] = child_;
        }
        page_request_ = childPage(selected_, child_);
        visible_ = false;
    }

    bool visible_ = false;
    int selected_ = 0;
    int child_ = 0;
    int child_by_workflow_[kWorkflowEntryCount]{0, 0, 0, 0, 0};
    int page_request_ = -1;
    bool help_request_ = false;
};