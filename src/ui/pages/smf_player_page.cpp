#include "smf_player_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"

#ifdef ARDUINO
#include <SD.h>
#endif

using namespace GroovePuterMidi;

SmfPlayerPage::SmfPlayerPage(IGfx& gfx, MiniAcid& miniAcid)
    : miniAcid_(miniAcid), player_(smfPlayerService()) {
    (void)gfx;
}

void SmfPlayerPage::onEnter(int context) {
    (void)context;
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    browserVisible_ = state.state == SmfPlayerState::Unloaded ||
                      state.state == SmfPlayerState::Error;
    if (files_.empty()) refreshFiles();
}

void SmfPlayerPage::refreshFiles() {
    files_.clear();
#ifdef ARDUINO
    if (!SD.exists("/midi")) SD.mkdir("/midi");
    File root = SD.open("/midi");
    if (root) {
        while (files_.size() < 48) {
            File entry = root.openNextFile();
            if (!entry) break;
            if (!entry.isDirectory()) {
                const char* raw = entry.name();
                if (raw) {
                    std::string name(raw);
                    const std::size_t slash = name.rfind('/');
                    if (slash != std::string::npos) name.erase(0, slash + 1);
                    if (name.size() >= 4) {
                        std::string ext = name.substr(name.size() - 4);
                        std::transform(ext.begin(), ext.end(), ext.begin(),
                                       [](unsigned char c) {
                                           return static_cast<char>(std::tolower(c));
                                       });
                        if (ext == ".mid") files_.push_back(std::move(name));
                    }
                }
            }
            entry.close();
        }
        root.close();
    }
#endif
    std::sort(files_.begin(), files_.end());
    if (selection_ >= static_cast<int>(files_.size())) {
        selection_ = files_.empty() ? 0 : static_cast<int>(files_.size()) - 1;
    }
    ensureSelectionVisible(7);
}

bool SmfPlayerPage::playSelected() {
    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }
    if (selection_ < 0 || selection_ >= static_cast<int>(files_.size())) {
        UI::showToast("No MIDI selected", 900);
        return true;
    }

    // Cardputer UI events are already executed under AudioMutationScope. Close
    // the accepted GroovePuter transport lifecycle before RAW SMF playback so
    // its F8 clock cannot run at an unrelated BPM beside the file timeline.
    if (miniAcid_.isPlaying()) miniAcid_.stop();

    std::string path = "/midi/" + files_[selection_];
    if (!player_->requestLoadAndPlay(path.c_str())) {
        UI::showToast("Player queue busy", 1000);
        return true;
    }
    browserVisible_ = false;
    UI::showToast("Loading MIDI...", 800);
    return true;
}

bool SmfPlayerPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.alt || event.ctrl || event.meta) {
        return false;
    }

    if (browserVisible_) {
        if (event.scancode == GROOVEPUTER_UP) {
            if (selection_ > 0) --selection_;
            ensureSelectionVisible(7);
            return true;
        }
        if (event.scancode == GROOVEPUTER_DOWN) {
            if (selection_ + 1 < static_cast<int>(files_.size())) ++selection_;
            ensureSelectionVisible(7);
            return true;
        }
        if (event.key == '\n' || event.key == '\r') return playSelected();
        if (event.key == 'r' || event.key == 'R') {
            refreshFiles();
            return true;
        }
        return false;
    }

    player_ = smfPlayerService();
    if (!player_) return false;

    if (event.scancode == GROOVEPUTER_LEFT) {
        player_->seekBars(event.shift ? -4 : -1);
        return true;
    }
    if (event.scancode == GROOVEPUTER_RIGHT) {
        player_->seekBars(event.shift ? 4 : 1);
        return true;
    }
    if (event.key == 'x' || event.key == 'X') {
        player_->panic();
        UI::showToast("MIDI PANIC / PAUSE", 900);
        return true;
    }
    if (event.key == 'b' || event.key == 'B') {
        browserVisible_ = true;
        refreshFiles();
        return true;
    }
    if (event.key == '\n' || event.key == '\r') {
        const SmfPlayerSnapshot state = player_->snapshot();
        if (state.state == SmfPlayerState::Error) {
            browserVisible_ = true;
            return true;
        }
    }
    return false;
}

void SmfPlayerPage::drawHeader(IGfx& gfx) {
    UI::drawStandardHeader(gfx, miniAcid_, "MIDI PLAYER");
}

void SmfPlayerPage::drawContent(IGfx& gfx) {
    LayoutManager::clearContent(gfx);
    if (browserVisible_) drawBrowser(gfx);
    else drawNowPlaying(gfx);
}

void SmfPlayerPage::drawBrowser(IGfx& gfx) {
    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), "PLAY FROM /midi");

    if (files_.empty()) {
        gfx.setTextColor(COLOR_LABEL);
#ifdef ARDUINO
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "No .mid files found");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "Copy files to /midi");
#else
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "SD browser: Cardputer only");
#endif
        return;
    }

    constexpr int visibleRows = 7;
    for (int row = 0; row < visibleRows; ++row) {
        const int index = scroll_ + row;
        if (index >= static_cast<int>(files_.size())) break;
        const int y = LayoutManager::lineY(row + 1);
        const bool selected = index == selection_;
        if (selected) {
            gfx.fillRect(Layout::CONTENT.x + 2, y - 1,
                         Layout::CONTENT.w - 4, gfx.fontHeight() + 2,
                         COLOR_PANEL);
        }
        gfx.setTextColor(selected ? COLOR_ACCENT : COLOR_TEXT);
        char line[42];
        std::snprintf(line, sizeof(line), "%c %.34s",
                      selected ? '>' : ' ', files_[index].c_str());
        gfx.drawText(Layout::COL_1, y, line);
    }
}

void SmfPlayerPage::drawNowPlaying(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};

    char line[64];
    gfx.setTextColor(state.state == SmfPlayerState::Error ? COLOR_DANGER : COLOR_ACCENT);
    std::snprintf(line, sizeof(line), "%s", smfPlayerStateName(state.state));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    gfx.setTextColor(COLOR_TEXT);
    std::snprintf(line, sizeof(line), "%.38s", state.filename[0] ? state.filename : "--");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    std::snprintf(line, sizeof(line), "BAR %lu : BEAT %u / %lu",
                  static_cast<unsigned long>(state.bar),
                  static_cast<unsigned>(state.beat),
                  static_cast<unsigned long>(state.totalBars));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    std::snprintf(line, sizeof(line), "TEMPO ORIGINAL  %u.%u BPM",
                  static_cast<unsigned>(state.bpmX10 / 10),
                  static_cast<unsigned>(state.bpmX10 % 10));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4),
                 state.rawRouting ? "ROUTE RAW  CH1..16" : "ROUTE --");

    const uint32_t total = state.endTick > 0 ? state.endTick : 1;
    const uint32_t current = std::min(state.currentTick, total);
    const int barW = Layout::CONTENT.w - 12;
    const int filled = static_cast<int>(
        (static_cast<uint64_t>(current) * barW) / total);
    const int y = LayoutManager::lineY(5) + 2;
    gfx.drawRect(Layout::COL_1, y, barW, 7, COLOR_LABEL);
    if (filled > 0) gfx.fillRect(Layout::COL_1 + 1, y + 1, filled - 1, 5, COLOR_ACCENT);

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), state.message);
}

void SmfPlayerPage::drawFooter(IGfx& gfx) {
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "Up/Dn Select Enter Play", "R Refresh  [ ] Pages");
    } else {
        UI::drawStandardFooter(gfx, "Space Play  Sh+Space Start", "< > Seek  X Panic  B Files");
    }
}

void SmfPlayerPage::ensureSelectionVisible(int visibleRows) {
    if (visibleRows < 1) visibleRows = 1;
    if (selection_ < scroll_) scroll_ = selection_;
    if (selection_ >= scroll_ + visibleRows) {
        scroll_ = selection_ - visibleRows + 1;
    }
    const int maxScroll = std::max(0, static_cast<int>(files_.size()) - visibleRows);
    if (scroll_ > maxScroll) scroll_ = maxScroll;
    if (scroll_ < 0) scroll_ = 0;
}
