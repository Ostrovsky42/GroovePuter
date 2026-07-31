#include "smf_player_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"

#ifdef ARDUINO
#include <SD.h>
#include "../../platform/cardputer_sd.h"
#endif
#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace {
constexpr size_t kMaxSmfDirsInUi = 24;
constexpr size_t kMaxSmfFilesInUi = 48;
constexpr size_t kSmfListLowMemGuardBytes = 4096;
}  // namespace

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
    if (entryCount() == 0) refreshFiles();
}

void SmfPlayerPage::refreshFiles() {
    dirs_.clear();
    files_.clear();
#ifdef ARDUINO
    if (currentPath_.empty()) currentPath_ = "/midi";
    bool exists = SD.exists(currentPath_.c_str());
    Serial.printf("[SMF-BROWSE] path=%s exists=%d\n", currentPath_.c_str(), (int)exists);
    if (!exists) {
        const bool reinit = GroovePuterPlatform::ensureCardputerSdMounted();
        Serial.printf("[SMF-BROWSE] SD mount retry ok=%d\n", (int)reinit);
        exists = SD.exists(currentPath_.c_str());
        Serial.printf("[SMF-BROWSE] path=%s exists-after-retry=%d\n", currentPath_.c_str(),
                      (int)exists);
    }
    if (!exists) SD.mkdir(currentPath_.c_str());
    File root = SD.open(currentPath_.c_str());
    Serial.printf("[SMF-BROWSE] root.open ok=%d isDir=%d\n", (int)(bool)root,
                  root ? (int)root.isDirectory() : -1);
    if (root) {
        int seen = 0;
        while (true) {
#if defined(ESP32) || defined(ESP_PLATFORM)
            size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (freeHeap < kSmfListLowMemGuardBytes) {
                Serial.printf("[SMF-BROWSE] low-mem break freeHeap=%u\n", (unsigned)freeHeap);
                break;
            }
#endif
            File entry = root.openNextFile();
            if (!entry) break;
            ++seen;

            const bool isDir = entry.isDirectory();
            Serial.printf("[SMF-BROWSE] entry#%d name=%s isDir=%d\n", seen, entry.name(), (int)isDir);
            if ((isDir && dirs_.size() >= kMaxSmfDirsInUi) ||
                (!isDir && files_.size() >= kMaxSmfFilesInUi)) {
                entry.close();
                continue;
            }

            const char* raw = entry.name();
            if (raw) {
                std::string name(raw);
                const std::size_t slash = name.rfind('/');
                if (slash != std::string::npos) name.erase(0, slash + 1);

                if (!name.empty() && name[0] != '.') {
                    if (isDir) {
                        dirs_.push_back(std::move(name));
                    } else if (name.size() >= 4) {
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
        Serial.printf("[SMF-BROWSE] scanned=%d dirs=%d files=%d\n", seen,
                      (int)dirs_.size(), (int)files_.size());
    }
#endif
    std::sort(dirs_.begin(), dirs_.end());
    std::sort(files_.begin(), files_.end());
    const int total = entryCount();
    if (selection_ >= total) selection_ = total > 0 ? total - 1 : 0;
    if (selection_ < 0) selection_ = 0;
    ensureSelectionVisible(7);
}

bool SmfPlayerPage::navigateIntoDir(const std::string& dirName) {
#ifdef ARDUINO
    std::string newPath = currentPath_ + "/" + dirName;
    if (!SD.exists(newPath.c_str())) return false;
    currentPath_ = newPath;
    refreshFiles();
    selection_ = 0;
    scroll_ = 0;
    return true;
#else
    (void)dirName;
    return false;
#endif
}

bool SmfPlayerPage::navigateUpDir() {
    if (currentPath_ == "/midi" || currentPath_.empty()) return false;
    const std::size_t lastSlash = currentPath_.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        currentPath_ = "/midi";
    } else {
        currentPath_ = currentPath_.substr(0, lastSlash);
    }
    if (currentPath_.size() < 5) currentPath_ = "/midi";  // safety
    refreshFiles();
    selection_ = 0;
    scroll_ = 0;
    return true;
}

bool SmfPlayerPage::hasParentEntry() const { return currentPath_ != "/midi"; }

int SmfPlayerPage::entryCount() const {
    return (hasParentEntry() ? 1 : 0) + static_cast<int>(dirs_.size()) +
           static_cast<int>(files_.size());
}

bool SmfPlayerPage::isDirEntry(int index) const {
    if (hasParentEntry()) {
        if (index == 0) return true;  // ".." entry
        index--;
    }
    return index >= 0 && index < static_cast<int>(dirs_.size());
}

const std::string& SmfPlayerPage::displayName(int index) const {
    static const std::string kParent = "..";
    static const std::string kUnknown = "?";
    if (hasParentEntry()) {
        if (index == 0) return kParent;
        index--;
    }
    if (index >= 0 && index < static_cast<int>(dirs_.size())) return dirs_[index];
    const int fileIdx = index - static_cast<int>(dirs_.size());
    if (fileIdx >= 0 && fileIdx < static_cast<int>(files_.size())) return files_[fileIdx];
    return kUnknown;
}

bool SmfPlayerPage::playSelected() {
    if (isDirEntry(selection_)) {
        bool hasParent = hasParentEntry();
        if (hasParent && selection_ == 0) {
            navigateUpDir();
        } else {
            const int dirIndex = selection_ - (hasParent ? 1 : 0);
            if (dirIndex >= 0 && dirIndex < static_cast<int>(dirs_.size())) {
                navigateIntoDir(dirs_[dirIndex]);
            }
        }
        return true;
    }

    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }
    const int fileIdx = selection_ - (hasParentEntry() ? 1 : 0) - static_cast<int>(dirs_.size());
    if (fileIdx < 0 || fileIdx >= static_cast<int>(files_.size())) {
        UI::showToast("No MIDI selected", 900);
        return true;
    }

    // Cardputer UI events are already executed under AudioMutationScope. Close
    // the accepted GroovePuter transport lifecycle before RAW SMF playback so
    // its F8 clock cannot run at an unrelated BPM beside the file timeline.
    if (miniAcid_.isPlaying()) miniAcid_.stop();

    std::string path = currentPath_ + "/" + files_[fileIdx];
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
            if (selection_ + 1 < entryCount()) ++selection_;
            ensureSelectionVisible(7);
            return true;
        }
        if (event.key == '\n' || event.key == '\r') return playSelected();
        if (event.key == '\b') {
            if (currentPath_ != "/midi") {
                navigateUpDir();
                return true;
            }
            return false;
        }
        if (event.key == 'r' || event.key == 'R') {
            refreshFiles();
            return true;
        }
        if (event.key == 'm' || event.key == 'M') {
            if (player_) player_->toggleRouting();
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
    if (event.key == 'r' || event.key == 'R') {
        const bool queued = player_->restart(SmfPlayerRestartOrigin::MusicStart);
        UI::showToast(queued ? "MIDI: RESTART" : "MIDI PLAYER BUSY", 800);
        return true;
    }
    if (event.key == 'd' || event.key == 'D') {
        performanceVisible_ = !performanceVisible_;
        return true;
    }
    if (event.key == 'x' || event.key == 'X') {
        player_->panic();
        UI::showToast("MIDI PANIC / PAUSE", 900);
        return true;
    }
    if (event.key == 'b' || event.key == 'B' ||
        event.key == '\n' || event.key == '\r' || event.key == '\b') {
        browserVisible_ = true;
        refreshFiles();
        return true;
    }
    if (event.key == 'm' || event.key == 'M') {
        player_->toggleRouting();
        UI::showToast("MIDI route changed", 800);
        return true;
    }
    return false;
}

void SmfPlayerPage::drawHeader(IGfx& gfx) {
    UI::drawStandardHeader(
        gfx, miniAcid_, performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER");
}

void SmfPlayerPage::drawContent(IGfx& gfx) {
    LayoutManager::clearContent(gfx);
    if (browserVisible_) drawBrowser(gfx);
    else if (performanceVisible_) drawPerformance(gfx);
    else drawNowPlaying(gfx);
}

void SmfPlayerPage::drawBrowser(IGfx& gfx) {
    gfx.setTextColor(COLOR_ACCENT);
    char header[42];
    std::snprintf(header, sizeof(header), "PLAY FROM %.30s", currentPath_.c_str());
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), header);

    const int total = entryCount();
    if (total == 0) {
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
        if (index >= total) break;
        const int y = LayoutManager::lineY(row + 1);
        const bool selected = index == selection_;
        if (selected) {
            gfx.fillRect(Layout::CONTENT.x + 2, y - 1,
                         Layout::CONTENT.w - 4, gfx.fontHeight() + 2,
                         COLOR_PANEL);
        }
        gfx.setTextColor(selected ? COLOR_ACCENT : COLOR_TEXT);
        char line[42];
        const bool isDir = isDirEntry(index);
        std::snprintf(line, sizeof(line), "%c%s%.32s",
                      selected ? '>' : ' ', isDir ? "/" : " ",
                      displayName(index).c_str());
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
                 state.rawRouting ? "ROUTE RAW  CH1..16" : "ROUTE SEQTRAK GM MAP");

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

void SmfPlayerPage::drawPerformance(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    const SmfPlayerPerformanceSnapshot& perf = state.performance;

    char line[64];
    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), "LIVE 2 SEC WINDOW");

    gfx.setTextColor(COLOR_TEXT);
    std::snprintf(line, sizeof(line), "TRACKS %u  CACHE/TRK %u B",
                  static_cast<unsigned>(perf.trackCount),
                  static_cast<unsigned>(perf.cacheBytesPerTrack));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    std::snprintf(line, sizeof(line), "READ %lu  SEEK %lu",
                  static_cast<unsigned long>(perf.reads),
                  static_cast<unsigned long>(perf.seeks));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    std::snprintf(line, sizeof(line), "BYTES %lu  READ MAX %lu US",
                  static_cast<unsigned long>(perf.bytes),
                  static_cast<unsigned long>(perf.maxReadMicros));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    std::snprintf(line, sizeof(line), "SCHED %lu  QUEUED %lu",
                  static_cast<unsigned long>(perf.scheduleCalls),
                  static_cast<unsigned long>(perf.queuedEvents));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    std::snprintf(line, sizeof(line), "SCHED MAX %lu US",
                  static_cast<unsigned long>(perf.maxScheduleMicros));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);

    if (perf.minQueueDepth < 0) {
        std::snprintf(line, sizeof(line), "QUEUE MIN -- / %u",
                      static_cast<unsigned>(perf.queueFillLimit));
    } else {
        std::snprintf(line, sizeof(line), "QUEUE MIN %d / %u",
                      static_cast<int>(perf.minQueueDepth),
                      static_cast<unsigned>(perf.queueFillLimit));
    }
    gfx.setTextColor(perf.minQueueDepth >= 0 && perf.minQueueDepth <= 2
                         ? COLOR_DANGER
                         : COLOR_TEXT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6), line);

    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "LOOKAHEAD %u MS",
                  static_cast<unsigned>(perf.lookaheadMs));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

void SmfPlayerPage::drawFooter(IGfx& gfx) {
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "Up/Dn Select Enter Open", "Bksp Up  M Route  [ ] Pages");
    } else if (performanceVisible_) {
        UI::drawStandardFooter(gfx, "D Player B Files", "Space Play R Restart");
    } else {
        UI::drawStandardFooter(gfx, "Space Play R Restart", "< > Seek B Files M/X/D");
    }
}

void SmfPlayerPage::ensureSelectionVisible(int visibleRows) {
    if (visibleRows < 1) visibleRows = 1;
    if (selection_ < scroll_) scroll_ = selection_;
    if (selection_ >= scroll_ + visibleRows) {
        scroll_ = selection_ - visibleRows + 1;
    }
    const int maxScroll = std::max(0, entryCount() - visibleRows);
    if (scroll_ > maxScroll) scroll_ = maxScroll;
    if (scroll_ < 0) scroll_ = 0;
}
