#include "smf_player_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "../components/music_visuals.h"
#include "src/dsp/miniacid_engine.h"
#include "src/midi/transport_clock_runtime.h"

#ifdef ARDUINO
#include <SD.h>
#include "../../platform/cardputer_sd.h"
#endif
#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace {
bool smfStateIsActive(GroovePuterMidi::SmfPlayerState state) {
    return state == GroovePuterMidi::SmfPlayerState::Playing ||
           state == GroovePuterMidi::SmfPlayerState::Armed;
}

const char* browserBasename(const char* path) {
    if (path == nullptr) return "";
    const char* slash = std::strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

bool browserNameIsVisible(const char* name) {
    return name != nullptr && name[0] != '\0' && name[0] != '.';
}

bool browserNameIsMidi(const char* name) {
    if (!browserNameIsVisible(name)) return false;
    const std::size_t length = std::strlen(name);
    if (length < 4) return false;
    const char* ext = name + length - 4;
    return ext[0] == '.' &&
           std::tolower(static_cast<unsigned char>(ext[1])) == 'm' &&
           std::tolower(static_cast<unsigned char>(ext[2])) == 'i' &&
           std::tolower(static_cast<unsigned char>(ext[3])) == 'd';
}
}  // namespace

using namespace GroovePuterMidi;

SmfPlayerPage::SmfPlayerPage(IGfx& gfx,
                             MiniAcid& miniAcid,
                             AudioGuard audioGuard)
    : miniAcid_(miniAcid),
      audioGuard_(audioGuard),
      player_(smfPlayerService()) {
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
    directoryCount_ = 0;
    fileCount_ = 0;
    totalEntries_ = 0;
    visibleWindowStart_ = -1;
    browserStorageReady_ = false;
    for (BrowserRow& row : browserRows_) row = {};
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
#if defined(ESP32) || defined(ESP_PLATFORM)
    const size_t freeBeforeOpen =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
    File root = SD.open(currentPath_.c_str());
    Serial.printf(
        "[SMF-BROWSE] root.open ok=%d isDir=%d"
#if defined(ESP32) || defined(ESP_PLATFORM)
        " freeBefore=%u freeOpen=%u"
#endif
        "\n",
        (int)(bool)root,
        root ? (int)root.isDirectory() : -1
#if defined(ESP32) || defined(ESP_PLATFORM)
        , static_cast<unsigned>(freeBeforeOpen),
        static_cast<unsigned>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))
#endif
    );
    if (root && root.isDirectory()) {
        browserStorageReady_ = true;
        int seen = 0;
        while (true) {
            File entry = root.openNextFile();
            if (!entry) break;
            ++seen;

            const bool isDir = entry.isDirectory();
            const char* name = browserBasename(entry.name());
            if (browserNameIsVisible(name)) {
                if (isDir) {
                    ++directoryCount_;
                } else if (browserNameIsMidi(name)) {
                    ++fileCount_;
                }
            }
            entry.close();
        }
        root.close();
        totalEntries_ = (hasParentEntry() ? 1 : 0) +
                        directoryCount_ + fileCount_;
        Serial.printf(
            "[SMF-BROWSE] scanned=%d dirs=%d files=%d complete=1"
#if defined(ESP32) || defined(ESP_PLATFORM)
            " freeAfter=%u"
#endif
            "\n",
            seen, directoryCount_, fileCount_
#if defined(ESP32) || defined(ESP_PLATFORM)
            , static_cast<unsigned>(
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))
#endif
        );
    } else {
        if (root) root.close();
        Serial.println("[SMF-BROWSE] complete=0 reason=root-unavailable");
    }
#endif
    const int total = entryCount();
    if (selection_ >= total) selection_ = total > 0 ? total - 1 : 0;
    if (selection_ < 0) selection_ = 0;
    ensureSelectionVisible(kBrowserVisibleRows);
}

void SmfPlayerPage::fillVisibleEntries() {
    visibleWindowStart_ = scroll_;
    for (BrowserRow& row : browserRows_) row = {};

    if (hasParentEntry() && scroll_ == 0) {
        browserRows_[0].logicalIndex = 0;
        std::snprintf(browserRows_[0].displayName,
                      sizeof(browserRows_[0].displayName), "..");
    }

#ifdef ARDUINO
    File root = SD.open(currentPath_.c_str());
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        browserStorageReady_ = false;
        directoryCount_ = 0;
        fileCount_ = 0;
        totalEntries_ = 0;
        Serial.println("[SMF-BROWSE] window.open failed");
        return;
    }

    const int parentOffset = hasParentEntry() ? 1 : 0;
    const int requiredRows =
        std::min(kBrowserVisibleRows, std::max(0, totalEntries_ - scroll_));
    int filledRows = hasParentEntry() && scroll_ == 0 ? 1 : 0;
    int directoryIndex = 0;
    int fileIndex = 0;
    while (filledRows < requiredRows) {
        File entry = root.openNextFile();
        if (!entry) break;

        const bool isDir = entry.isDirectory();
        const char* name = browserBasename(entry.name());
        int logicalIndex = -1;
        if (browserNameIsVisible(name)) {
            if (isDir) {
                logicalIndex = parentOffset + directoryIndex++;
            } else if (browserNameIsMidi(name)) {
                logicalIndex = parentOffset + directoryCount_ + fileIndex++;
            }
        }

        const int slot = logicalIndex - scroll_;
        if (slot >= 0 && slot < kBrowserVisibleRows) {
            BrowserRow& row = browserRows_[slot];
            row.logicalIndex = logicalIndex;
            std::snprintf(row.displayName, sizeof(row.displayName), "%s", name);
            ++filledRows;
        }
        entry.close();
    }
    root.close();
#endif
}

bool SmfPlayerPage::resolveEntry(int logicalIndex,
                                 std::string& name,
                                 bool& isDirectory) const {
    name.clear();
    isDirectory = false;
    if (hasParentEntry() && logicalIndex == 0) {
        name = "..";
        isDirectory = true;
        return true;
    }

#ifdef ARDUINO
    const int parentOffset = hasParentEntry() ? 1 : 0;
    const bool targetIsDirectory =
        logicalIndex >= parentOffset &&
        logicalIndex < parentOffset + directoryCount_;
    const int targetOrdinal = targetIsDirectory
        ? logicalIndex - parentOffset
        : logicalIndex - parentOffset - directoryCount_;
    if (targetOrdinal < 0) return false;

    File root = SD.open(currentPath_.c_str());
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return false;
    }

    int ordinal = 0;
    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        const bool entryIsDirectory = entry.isDirectory();
        const char* entryName = browserBasename(entry.name());
        const bool matches = browserNameIsVisible(entryName) &&
            (targetIsDirectory
                 ? entryIsDirectory
                 : (!entryIsDirectory && browserNameIsMidi(entryName)));
        if (matches && ordinal++ == targetOrdinal) {
            name = entryName;
            isDirectory = entryIsDirectory;
            entry.close();
            root.close();
            return true;
        }
        entry.close();
    }
    root.close();
#else
    (void)logicalIndex;
#endif
    return false;
}

bool SmfPlayerPage::navigateIntoDir(const std::string& dirName) {
#ifdef ARDUINO
    std::string newPath = currentPath_ + "/" + dirName;
    if (!SD.exists(newPath.c_str())) return false;
    currentPath_ = newPath;
    selection_ = 0;
    scroll_ = 0;
    refreshFiles();
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
    selection_ = 0;
    scroll_ = 0;
    refreshFiles();
    return true;
}

bool SmfPlayerPage::hasParentEntry() const { return currentPath_ != "/midi"; }

int SmfPlayerPage::entryCount() const {
    return totalEntries_;
}

bool SmfPlayerPage::isDirEntry(int index) const {
    if (hasParentEntry()) {
        if (index == 0) return true;
        --index;
    }
    return index >= 0 && index < directoryCount_;
}

const char* SmfPlayerPage::displayName(int index) const {
    const int slot = index - visibleWindowStart_;
    if (slot < 0 || slot >= kBrowserVisibleRows) return "?";
    const BrowserRow& row = browserRows_[slot];
    return row.logicalIndex == index ? row.displayName : "?";
}

bool SmfPlayerPage::playSelected() {
    if (hasParentEntry() && selection_ == 0) {
        navigateUpDir();
        return true;
    }

    std::string selectedName;
    bool selectedIsDirectory = false;
    if (!resolveEntry(selection_, selectedName, selectedIsDirectory)) {
        UI::showToast("MIDI entry unavailable", 900);
        return true;
    }
    if (selectedIsDirectory) {
        if (!navigateIntoDir(selectedName)) {
            UI::showToast("MIDI folder unavailable", 900);
        }
        return true;
    }

    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }
    const SmfPlayerSnapshot playerState = player_->snapshot();

    std::string path = currentPath_ + "/" + selectedName;
    if (!player_->requestLoad(path.c_str())) {
        UI::showToast("Player queue busy", 1000);
        return true;
    }
    browserVisible_ = false;
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
    const bool followSeqtrak = clock.source == TransportClockSource::SeqtrakExternal;
    UI::showToast(playerState.tempoMode == SmfTempoMode::Project
                      ? (followSeqtrak
                             ? (clock.externalFollowEnabled
                                    ? "LOADING / SPACE ARM / SEQ PLAY"
                                    : "LOADING / FOLLOW OFF")
                             : "LOADING / G THEN SPACE")
                      : "LOADING / SPACE TO PLAY",
                  1000);
    return true;
}

bool SmfPlayerPage::togglePlayerTransport() {
    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }

    const SmfPlayerSnapshot state = player_->snapshot();
    if (state.state == SmfPlayerState::Unloaded ||
        state.state == SmfPlayerState::Error) {
        UI::showToast("ENTER: LOAD MIDI", 900);
        return true;
    }

    const bool wasActive = smfStateIsActive(state.state);
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
    if (!wasActive && state.tempoMode == SmfTempoMode::Project &&
        !miniAcid_.isPlaying() &&
        clock.source == TransportClockSource::GroovePuterInternal) {
        UI::showToast("G START FIRST / THEN SPACE", 1100);
        return true;
    }
    const bool queued = player_->togglePlayPause();
    if (!queued) {
        UI::showToast("MIDI PLAYER BUSY", 800);
    } else if (wasActive) {
        UI::showToast("MIDI: PAUSE", 700);
    } else if (state.tempoMode == SmfTempoMode::Project) {
        UI::showToast(clock.source == TransportClockSource::SeqtrakExternal
                          ? (clock.externalFollowEnabled
                                 ? "MIDI ARMED / PLAY SEQTRAK"
                                 : "MIDI ARMED / FOLLOW OFF")
                          : "MIDI: ARM NEXT BAR",
                      900);
    } else {
        UI::showToast("MIDI: PLAY", 700);
    }
    return true;
}

void SmfPlayerPage::toggleGrooveTransport() {
    TransportClockRuntime& clockRuntime = transportClockRuntime();
    if (clockRuntime.source() == TransportClockSource::SeqtrakExternal) {
        const bool enabled = clockRuntime.toggleExternalFollowEnabled();
        UI::showToast(enabled
                          ? "EXT FOLLOW ON / WAIT SEQ"
                          : "EXT FOLLOW OFF / STOP",
                      1000);
        return;
    }
    player_ = smfPlayerService();
    if (miniAcid_.isPlaying()) {
        bool playerPauseQueued = true;
        if (player_) {
            const SmfPlayerSnapshot state = player_->snapshot();
            if (state.tempoMode == SmfTempoMode::Project &&
                smfStateIsActive(state.state)) {
                playerPauseQueued = player_->pause();
            }
        }
        withAudioGuard([this]() { miniAcid_.stop(); });
        UI::showToast(playerPauseQueued
                          ? "GROOVE STOP / MIDI PAUSED"
                          : "GROOVE STOP / MIDI BUSY",
                      900);
    } else {
        withAudioGuard([this]() { miniAcid_.start(); });
        UI::showToast("GROOVE PLAY / SPACE MIDI", 900);
    }
}

bool SmfPlayerPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.alt || event.ctrl || event.meta) {
        return false;
    }

    player_ = smfPlayerService();

    if (event.key == 'c' || event.key == 'C') {
        const TransportClockSource source = transportClockRuntime().toggleSource();
        const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
        UI::showToast(source == TransportClockSource::SeqtrakExternal &&
                              !clock.externalFollowEnabled
                          ? "SEQ MASTER / FOLLOW OFF"
                          : transportClockSourceName(source),
                      1000);
        return true;
    }

    if (event.key == ' ') return togglePlayerTransport();
    if (event.key == 'g' || event.key == 'G') {
        toggleGrooveTransport();
        return true;
    }

    if (browserVisible_) {
        if (event.scancode == GROOVEPUTER_UP) {
            if (selection_ > 0) --selection_;
            ensureSelectionVisible(kBrowserVisibleRows);
            return true;
        }
        if (event.scancode == GROOVEPUTER_DOWN) {
            if (selection_ + 1 < entryCount()) ++selection_;
            ensureSelectionVisible(kBrowserVisibleRows);
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
                const bool followSeqtrak = transportClockRuntime().source() ==
                    TransportClockSource::SeqtrakExternal;
                UI::showToast(queued
                                  ? (toProject
                                         ? (followSeqtrak
                                                ? "TEMPO: SEQ MASTER"
                                                : "TEMPO: GP MASTER > USB")
                                         : "TEMPO: FILE ORIGINAL")
                                  : "MIDI PLAYER BUSY",
                              900);
            }
            return true;
        }
        return false;
    }

    if (!player_) return false;
    const SmfPlayerSnapshot state = player_->snapshot();

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
            if (transportClockRuntime().source() == TransportClockSource::SeqtrakExternal) {
                UI::showToast("SEQ MASTER BPM", 700);
            } else {
                withAudioGuard([this, deltaBpm]() {
                    const float targetBpm = std::max(
                        10.0f,
                        std::min(250.0f,
                                 miniAcid_.bpm() + static_cast<float>(deltaBpm)));
                    miniAcid_.setBpm(targetBpm);
                });
                UI::showToast("GP MASTER BPM / USB CLOCK", 700);
            }
        } else {
            const bool queued = player_->adjustTempoBpm(deltaBpm);
            UI::showToast(queued ? "MIDI: BPM SET / PAUSE" : "MIDI PLAYER BUSY", 800);
        }
        return true;
    }
    if (event.key == 'o' || event.key == 'O') {
        if (state.tempoMode == SmfTempoMode::Project) {
            UI::showToast(transportClockRuntime().source() == TransportClockSource::SeqtrakExternal
                              ? "SEQ MASTER uses SEQTRAK BPM"
                              : "GP MASTER uses GroovePuter BPM",
                          900);
        } else {
            const bool queued = player_->resetTempo();
            UI::showToast(queued ? "MIDI: TEMPO ORIGINAL" : "MIDI PLAYER BUSY", 800);
        }
        return true;
    }
    if (event.key == 't' || event.key == 'T') {
        const bool toProject = state.tempoMode == SmfTempoMode::Original;

        const bool modeQueued = player_->toggleTempoMode();
        if (!modeQueued) {
            UI::showToast("MIDI PLAYER BUSY", 900);
        } else if (toProject) {
            if (transportClockRuntime().source() == TransportClockSource::SeqtrakExternal) {
                UI::showToast("SEQ MASTER: SPACE ARM / SEQ PLAY", 1000);
            } else {
                UI::showToast(miniAcid_.isPlaying()
                                  ? "GP MASTER: ARM NEXT BAR"
                                  : "GP MASTER: G START FIRST",
                              1000);
            }
        } else {
            UI::showToast("FILE TEMPO / ORIGINAL", 1000);
        }
        return true;
    }
    if (event.key == 'v' || event.key == 'V') {
        const bool queued = player_->cycleVelocityBoost();
        UI::showToast(queued ? "MIDI: VELOCITY BOOST" : "MIDI PLAYER BUSY", 800);
        return true;
    }
    if (event.key == 'r' || event.key == 'R') {
        const bool queued = player_->restart(SmfPlayerRestartOrigin::MusicStart);
        const bool followSeqtrak = transportClockRuntime().source() == TransportClockSource::SeqtrakExternal;
        UI::showToast(queued
                          ? (state.tempoMode == SmfTempoMode::Project && !miniAcid_.isPlaying()
                                 ? (followSeqtrak
                                        ? "MIDI RESTART ARMED - SEQ PLAY"
                                        : "MIDI RESTART ARMED - G START")
                                 : "MIDI: RESTART")
                          : "MIDI PLAYER BUSY",
                      900);
        return true;
    }
    if (event.key == 'd' || event.key == 'D') {
        performanceVisible_ = !performanceVisible_;
        return true;
    }
    if (event.key == 'x' || event.key == 'X') {
        const bool queued = player_->panic();
        UI::showToast(queued ? "MIDI PANIC / PAUSE" : "PANIC QUEUE BUSY", 900);
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
    UI::drawStandardHeader(gfx, miniAcid_, performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER");
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
        if (browserStorageReady_) {
            gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "NO MIDI FILES");
            gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "COPY .MID TO /MIDI");
        } else {
            gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "SD UNAVAILABLE");
            gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "R: RETRY BROWSER");
        }
#else
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "SD BROWSER: CARDPUTER ONLY");
#endif
        return;
    }

    for (int row = 0; row < kBrowserVisibleRows; ++row) {
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
                      selected ? '>' : ' ', isDir ? "/" : " ", displayName(index));
        gfx.drawText(Layout::COL_1 + 2, y, line);
    }
}

void SmfPlayerPage::drawNowPlaying(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};

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
    std::snprintf(chip, sizeof(chip), "+%uV", static_cast<unsigned>(state.velocityBoost));
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

    const unsigned percent = static_cast<unsigned>((static_cast<uint64_t>(current) * 100u) / total);
    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "%lu / %lu BARS    %u%%",
                  static_cast<unsigned long>(state.bar),
                  static_cast<unsigned long>(state.totalBars),
                  percent);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    if (state.tempoMode == SmfTempoMode::Project) {
        const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
        if (clock.source == TransportClockSource::SeqtrakExternal) {
            if (!clock.externalFollowEnabled) {
                std::snprintf(line, sizeof(line), "SEQ MASTER: FOLLOW OFF");
            } else {
                std::snprintf(line, sizeof(line), "SEQ MASTER: %s %5.1f BPM",
                              externalClockLockStateName(clock.externalState),
                              clock.externalTempoValid ? clock.externalBpm() : 0.0);
            }
        } else {
            std::snprintf(line, sizeof(line), "GP MASTER: %s > USB CLOCK",
                          miniAcid_.isPlaying() ? "RUN" : "STOP");
        }
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
                 transportClockRuntime().source() == TransportClockSource::SeqtrakExternal
                     ? "G FOLLOW   SPACE MIDI   R RESTART"
                     : "G GROOVE   SPACE MIDI   R RESTART");

    const bool usbBlocked = std::strncmp(state.message, "USB MIDI BLOCKED", 16) == 0;
    gfx.setTextColor((error || usbBlocked) ? COLOR_DANGER : COLOR_LABEL);
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
    const bool seqMaster = transportClockRuntime().source() == TransportClockSource::SeqtrakExternal;
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Select  Enter Load",
                               seqMaster ? "C Master  G Follow  T Tempo"
                                         : "C Master  Space MIDI  T Tempo");
    } else if (performanceVisible_) {
        UI::drawStandardFooter(gfx, "D Player  B Files",
                               seqMaster ? "C Master  G Follow  T Tempo"
                                         : "C Master  Space MIDI  T Tempo");
    } else {
        UI::drawStandardFooter(gfx,
                               seqMaster ? "Space MIDI  G Follow  C Master"
                                         : "Space MIDI  C Master  R Restart",
                               "B Files  T Tempo  V Vel  X Panic");
    }
}

void SmfPlayerPage::ensureSelectionVisible(int visibleRows) {
    if (visibleRows < 1) visibleRows = 1;
    if (selection_ < scroll_) scroll_ = selection_;
    if (selection_ >= scroll_ + visibleRows) scroll_ = selection_ - visibleRows + 1;
    const int maxScroll = std::max(0, entryCount() - visibleRows);
    if (scroll_ > maxScroll) scroll_ = maxScroll;
    if (scroll_ < 0) scroll_ = 0;
    if (visibleWindowStart_ != scroll_) fillVisibleEntries();
}
