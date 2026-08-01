#include "smf_player_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../components/music_visuals.h"
#include "src/dsp/miniacid_engine.h"
#include "src/midi/smf_track_mute.h"

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

bool smfStateIsActive(GroovePuterMidi::SmfPlayerState state) {
    return state == GroovePuterMidi::SmfPlayerState::Playing ||
           state == GroovePuterMidi::SmfPlayerState::Armed;
}
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
    if (currentPath_.size() < 5) currentPath_ = "/midi";
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
        if (index == 0) return true;
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

    const SmfPlayerSnapshot playerState = player_->snapshot();
    if (playerState.tempoMode == SmfTempoMode::Original) {
        if (miniAcid_.isPlaying()) miniAcid_.stop();
    } else if (!miniAcid_.isPlaying()) {
        miniAcid_.start();
    }

    std::string path = currentPath_ + "/" + files_[fileIdx];
    if (!player_->requestLoadAndPlay(path.c_str())) {
        UI::showToast("Player queue busy", 1000);
        return true;
    }
    browserVisible_ = false;
    UI::showToast(playerState.tempoMode == SmfTempoMode::Project
                      ? "Loading / arm next bar..."
                      : "Loading MIDI...",
                  900);
    return true;
}

bool SmfPlayerPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.alt || event.ctrl || event.meta) {
        return false;
    }

    player_ = smfPlayerService();

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
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                const bool queued = player_->toggleRouting();
                UI::showToast(queued
                                  ? (state.rawRouting ? "ROUTE: SEQTRAK SAFE" : "ROUTE: RAW")
                                  : "MIDI PLAYER BUSY",
                              850);
            }
            return true;
        }
        if (event.key == 't' || event.key == 'T') {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                const bool toProject = state.tempoMode == SmfTempoMode::Original;
                const bool queued = player_->toggleTempoMode();
                UI::showToast(queued
                                  ? (toProject ? "TEMPO SOURCE: PROJECT" : "TEMPO SOURCE: ORIGINAL")
                                  : "MIDI PLAYER BUSY",
                              900);
            }
            return true;
        }
        return false;
    }

    if (!player_) return false;
    const SmfPlayerSnapshot state = player_->snapshot();

    if (event.key == ' ') {
        if (state.tempoMode == SmfTempoMode::Project &&
            state.state != SmfPlayerState::Playing &&
            state.state != SmfPlayerState::Armed &&
            !miniAcid_.isPlaying()) {
            miniAcid_.start();
        }
        const bool queued = player_->togglePlayPause();
        UI::showToast(queued
                          ? (state.tempoMode == SmfTempoMode::Project
                              ? "MIDI: PROJECT PLAY/PAUSE"
                              : "MIDI: PLAY/PAUSE")
                          : "MIDI PLAYER BUSY",
                      800);
        return true;
    }
    if (event.scancode == GROOVEPUTER_LEFT) {
        player_->seekBars(event.shift ? -4 : -1);
        return true;
    }
    if (event.scancode == GROOVEPUTER_RIGHT) {
        player_->seekBars(event.shift ? 4 : 1);
        return true;
    }
    if (event.scancode == GROOVEPUTER_UP ||
        event.scancode == GROOVEPUTER_DOWN) {
        const int deltaBpm = event.scancode == GROOVEPUTER_UP ? 1 : -1;
        if (state.tempoMode == SmfTempoMode::Project) {
            miniAcid_.setBpm(miniAcid_.bpm() + static_cast<float>(deltaBpm));
            UI::showToast("PROJECT BPM / CLOCK", 700);
        } else {
            const bool queued = player_->adjustTempoBpm(deltaBpm);
            UI::showToast(queued ? "MIDI: BPM SET / PAUSE" : "MIDI PLAYER BUSY", 800);
        }
        return true;
    }
    if (event.key == 'o' || event.key == 'O') {
        if (state.tempoMode == SmfTempoMode::Project) {
            UI::showToast("PROJECT uses GroovePuter BPM", 900);
        } else {
            const bool queued = player_->resetTempo();
            UI::showToast(queued ? "MIDI: TEMPO ORIGINAL" : "MIDI PLAYER BUSY", 800);
        }
        return true;
    }
    if (event.key == 't' || event.key == 'T') {
        const bool wasActive = smfStateIsActive(state.state);
        const bool toProject = state.tempoMode == SmfTempoMode::Original;
        if (wasActive) {
            if (toProject && !miniAcid_.isPlaying()) {
                miniAcid_.start();
            } else if (!toProject && miniAcid_.isPlaying()) {
                miniAcid_.stop();
            }
        }

        const bool modeQueued = player_->toggleTempoMode();
        const bool resumeQueued = !wasActive ||
            (modeQueued && player_->togglePlayPause());
        if (!modeQueued || !resumeQueued) {
            UI::showToast("MIDI PLAYER BUSY", 900);
        } else if (toProject) {
            UI::showToast(wasActive ? "PROJECT: ARM NEXT BAR" : "PROJECT: SPACE TO ARM", 1000);
        } else {
            UI::showToast(wasActive ? "ORIGINAL: PLAYING" : "ORIGINAL: SPACE TO PLAY", 1000);
        }
        return true;
    }
    if (event.key == 'j' || event.key == 'J' ||
        event.key == 'l' || event.key == 'L') {
        smfTrackMuteState().selectRelative(
            (event.key == 'j' || event.key == 'J') ? -1 : 1);
        const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();
        char toast[40];
        if (tracks.trackCount == 0) {
            std::snprintf(toast, sizeof(toast), "NO MIDI TRACKS");
        } else {
            std::snprintf(toast, sizeof(toast), "TRACK %u/%u %s",
                          static_cast<unsigned>(tracks.selectedTrack + 1u),
                          static_cast<unsigned>(tracks.trackCount),
                          tracks.selectedMuted() ? "MUTED" : "ON");
        }
        UI::showToast(toast, 800);
        return true;
    }
    if (event.key == 'k' || event.key == 'K') {
        if (event.shift) {
            smfTrackMuteState().clear();
            UI::showToast("ALL MIDI TRACKS ON", 900);
        } else if (smfTrackMuteState().toggleSelected()) {
            const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();
            UI::showToast(tracks.selectedMuted()
                              ? "TRACK MUTE: NEXT NOTES"
                              : "TRACK UNMUTED",
                          900);
        } else {
            UI::showToast("NO MIDI TRACKS", 900);
        }
        return true;
    }
    if (event.key == 'v' || event.key == 'V') {
        const bool queued = player_->cycleVelocityBoost();
        UI::showToast(queued ? "MIDI: VELOCITY BOOST" : "MIDI PLAYER BUSY", 800);
        return true;
    }
    if (event.key == 'r' || event.key == 'R') {
        if (state.tempoMode == SmfTempoMode::Project && !miniAcid_.isPlaying()) {
            miniAcid_.start();
        }
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
        const bool queued = player_->toggleRouting();
        UI::showToast(queued
                          ? (state.rawRouting ? "ROUTE: SEQTRAK SAFE" : "ROUTE: RAW")
                          : "MIDI PLAYER BUSY",
                      850);
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
    char header[48];
    std::snprintf(header, sizeof(header), "MIDI LIBRARY  %.24s", currentPath_.c_str());
    gfx.setTextColor(MusicVisuals::accentForStyle());
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), header);

    const int total = entryCount();
    if (total == 0) {
        gfx.setTextColor(COLOR_LABEL);
#ifdef ARDUINO
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "NO MIDI FILES");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "COPY .MID TO /MIDI");
#else
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "SD BROWSER: CARDPUTER ONLY");
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
                         MusicVisuals::accentForStyle());
        }
        gfx.setTextColor(selected ? COLOR_BG : COLOR_TEXT);
        char line[42];
        const bool isDir = isDirEntry(index);
        std::snprintf(line, sizeof(line), "%c%s%.32s",
                      selected ? '>' : ' ', isDir ? "/" : " ",
                      displayName(index).c_str());
        gfx.drawText(Layout::COL_1 + 2, y, line);
    }
}

void SmfPlayerPage::drawNowPlaying(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();

    const bool playing = state.state == SmfPlayerState::Playing;
    const bool armed = state.state == SmfPlayerState::Armed;
    const bool error = state.state == SmfPlayerState::Error;
    const IGfxColor stateColor = error ? COLOR_DANGER
                                      : ((playing || armed)
                                          ? MusicVisuals::accentForStyle()
                                          : COLOR_WARN);

    int x = Layout::COL_1;
    const int chipY = LayoutManager::lineY(0);
    x += MusicVisuals::drawChip(gfx, x, chipY, smfPlayerStateName(state.state), true, stateColor) + 3;
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                state.rawRouting ? "RAW" : "SEQTRAK", true,
                                MusicVisuals::secondaryForStyle()) + 3;
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                smfTempoModeName(state.tempoMode), true,
                                state.tempoMode == SmfTempoMode::Project
                                    ? MusicVisuals::accentForStyle()
                                    : MusicVisuals::secondaryForStyle()) + 3;

    char chip[20];
    std::snprintf(chip, sizeof(chip), "+%uV",
                  static_cast<unsigned>(state.velocityBoost));
    MusicVisuals::drawChip(gfx, x, chipY, chip, state.velocityBoost > 0);

    gfx.setTextColor(COLOR_TEXT);
    char line[64];
    std::snprintf(line, sizeof(line), "%.38s", state.filename[0] ? state.filename : "--");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "BAR %lu.%u     %u.%u BPM",
                  static_cast<unsigned long>(state.bar),
                  static_cast<unsigned>(state.beat),
                  static_cast<unsigned>(state.bpmX10 / 10),
                  static_cast<unsigned>(state.bpmX10 % 10));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    const uint32_t total = state.endTick > 0 ? state.endTick : 1;
    const uint32_t current = std::min(state.currentTick, total);
    MusicVisuals::drawProgressBar(gfx,
                                  Layout::COL_1,
                                  LayoutManager::lineY(3) + 1,
                                  Layout::CONTENT.w - 12,
                                  9,
                                  current,
                                  total,
                                  stateColor);

    const unsigned percent = static_cast<unsigned>(
        (static_cast<uint64_t>(current) * 100u) / total);
    gfx.setTextColor(tracks.selectedMuted() ? COLOR_WARN : COLOR_LABEL);
    if (tracks.trackCount > 0) {
        std::snprintf(line, sizeof(line), "BAR %lu/%lu %u%%  TRK %u/%u %s",
                      static_cast<unsigned long>(state.bar),
                      static_cast<unsigned long>(state.totalBars),
                      percent,
                      static_cast<unsigned>(tracks.selectedTrack + 1u),
                      static_cast<unsigned>(tracks.trackCount),
                      tracks.selectedMuted() ? "MUTE" : "ON");
    } else {
        std::snprintf(line, sizeof(line), "%lu / %lu BARS    %u%%",
                      static_cast<unsigned long>(state.bar),
                      static_cast<unsigned long>(state.totalBars),
                      percent);
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    gfx.setTextColor(COLOR_LABEL);
    if (state.tempoMode == SmfTempoMode::Project) {
        std::snprintf(line, sizeof(line), "PROJECT CLOCK   LAUNCH %s",
                      smfLaunchModeName(state.launchMode));
    } else if (state.tempoScalePermille == 1000u) {
        std::snprintf(line, sizeof(line), "TEMPO SOURCE: FILE / ORIGINAL");
    } else {
        std::snprintf(line, sizeof(line), "ORIGINAL %u.%u BPM   O RESET",
                      static_cast<unsigned>(state.originalBpmX10 / 10),
                      static_cast<unsigned>(state.originalBpmX10 % 10));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);

    gfx.setTextColor(COLOR_TEXT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6),
                 "SPACE PLAY   J/L TRACK   K MUTE");

    gfx.setTextColor(error ? COLOR_DANGER : COLOR_LABEL);
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
    std::snprintf(line, sizeof(line), "LOOKAHEAD %u MS  %s",
                  static_cast<unsigned>(perf.lookaheadMs),
                  smfTempoModeName(state.tempoMode));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

void SmfPlayerPage::drawFooter(IGfx& gfx) {
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Select  Enter Play", "T TempoSrc  M Route  Bksp Up");
    } else if (performanceVisible_) {
        UI::drawStandardFooter(gfx, "D Player  B Files", "Space Play  T TempoSrc");
    } else {
        UI::drawStandardFooter(gfx, "J/L Track  K Mute  Sh+K All", "T TempoSrc  M Route  B Files");
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
