#include "midi_file_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "midi_file_name_policy.h"
#include "ui_colors.h"
#include "ui_common.h"
#include "ui_utils.h"
#include "ui_widgets.h"
#include "components/music_visuals.h"
#include "src/midi/smf_player_service.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include "src/platform/cardputer_sd.h"
#endif

namespace GroovePuterUi {
namespace {

const char* basenameOf(const char* path) {
    if (!path) return "";
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int compareIgnoreCase(const char* lhs, const char* rhs) {
    if (!lhs) lhs = "";
    if (!rhs) rhs = "";
    while (*lhs && *rhs) {
        const int a = std::tolower(static_cast<unsigned char>(*lhs));
        const int b = std::tolower(static_cast<unsigned char>(*rhs));
        if (a != b) return a < b ? -1 : 1;
        ++lhs;
        ++rhs;
    }
    if (*lhs == *rhs) return 0;
    return *lhs ? 1 : -1;
}

bool equalIgnoreCase(const char* lhs, const char* rhs) {
    return compareIgnoreCase(lhs, rhs) == 0;
}

void copyText(char* dst, std::size_t size, const char* src) {
    if (!dst || size == 0) return;
    std::snprintf(dst, size, "%s", src ? src : "");
}

const char* kindPrefix(MidiFileManager::EntryKind kind) {
    switch (kind) {
        case MidiFileManager::EntryKind::Parent: return "< ";
        case MidiFileManager::EntryKind::Directory: return "/ ";
        case MidiFileManager::EntryKind::MidiFile: return "  ";
    }
    return "  ";
}

}  // namespace

MidiFileManager::MidiFileManager() = default;

MidiFileManager& midiFileManager() {
    static MidiFileManager manager;
    return manager;
}

void MidiFileManager::open() {
    if (std::strncmp(currentPath_, "/midi", 5) != 0 ||
        std::strstr(currentPath_, "..") != nullptr) {
        copyText(currentPath_, sizeof(currentPath_), "/midi");
    }
    mode_ = Mode::Browse;
    deleteConfirmed_ = false;
    refresh();
}

bool MidiFileManager::refresh() {
    char previousName[kNameBytes]{};
    EntryKind previousKind = EntryKind::MidiFile;
    if (const Entry* previous = selectedEntry()) {
        copyText(previousName, sizeof(previousName), previous->name);
        previousKind = previous->kind;
    }

    entryCount_ = 0;
    directoryCount_ = 0;
    fileCount_ = 0;
    storageReady_ = false;
    truncated_ = false;
    for (Entry& entry : entries_) entry = Entry{};

#ifdef ARDUINO
    bool exists = SD.exists(currentPath_);
    if (!exists) {
        GroovePuterPlatform::ensureCardputerSdMounted();
        exists = SD.exists(currentPath_);
    }
    if (!exists && std::strcmp(currentPath_, "/midi") == 0) {
        exists = SD.mkdir(currentPath_);
    }
    if (!exists) {
        copyText(currentPath_, sizeof(currentPath_), "/midi");
        if (!SD.exists(currentPath_)) SD.mkdir(currentPath_);
    }

    File root = SD.open(currentPath_);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        selection_ = 0;
        scroll_ = 0;
        return false;
    }
    storageReady_ = true;

    if (std::strcmp(currentPath_, "/midi") != 0) {
        Entry& parent = entries_[entryCount_++];
        parent.kind = EntryKind::Parent;
        copyText(parent.name, sizeof(parent.name), "..");
    }

    while (true) {
        File file = root.openNextFile();
        if (!file) break;
        const bool directory = file.isDirectory();
        const char* rawName = basenameOf(file.name());
        const bool visible = rawName && rawName[0] != '\0' && rawName[0] != '.';
        const bool supported = directory || midiFilenameIsVisibleAndSupported(rawName);
        if (!visible || !supported) {
            file.close();
            continue;
        }

        if (directory) ++directoryCount_;
        else ++fileCount_;

        if (entryCount_ >= kMaxEntries || std::strlen(rawName) >= kNameBytes) {
            truncated_ = true;
            file.close();
            continue;
        }

        Entry& entry = entries_[entryCount_++];
        copyText(entry.name, sizeof(entry.name), rawName);
        entry.kind = directory ? EntryKind::Directory : EntryKind::MidiFile;
        entry.sizeBytes = directory ? 0u : static_cast<uint32_t>(file.size());
        file.close();
    }
    root.close();

    const int firstSortable =
        entryCount_ > 0 && entries_[0].kind == EntryKind::Parent ? 1 : 0;
    std::sort(entries_.begin() + firstSortable,
              entries_.begin() + entryCount_,
              [](const Entry& lhs, const Entry& rhs) {
                  if (lhs.kind != rhs.kind) {
                      return lhs.kind == EntryKind::Directory;
                  }
                  return compareIgnoreCase(lhs.name, rhs.name) < 0;
              });
#else
    (void)previousKind;
#endif

    if (previousName[0] != '\0') selectEntryByName(previousName, previousKind);
    if (selection_ >= entryCount_) selection_ = entryCount_ > 0 ? entryCount_ - 1 : 0;
    if (selection_ < 0) selection_ = 0;
    ensureSelectionVisible();
    mode_ = Mode::Browse;
    deleteConfirmed_ = false;
    return storageReady_;
}

const MidiFileManager::Entry* MidiFileManager::selectedEntry() const {
    if (selection_ < 0 || selection_ >= entryCount_) return nullptr;
    return &entries_[selection_];
}

MidiFileManager::Entry* MidiFileManager::selectedEntry() {
    if (selection_ < 0 || selection_ >= entryCount_) return nullptr;
    return &entries_[selection_];
}

bool MidiFileManager::buildPathForEntry(const Entry& entry,
                                        char* output,
                                        std::size_t outputSize) const {
    if (!output || outputSize == 0 || entry.kind == EntryKind::Parent) return false;
    const int written = std::snprintf(output, outputSize, "%s/%s", currentPath_, entry.name);
    return written > 0 && static_cast<std::size_t>(written) < outputSize;
}

bool MidiFileManager::selectedFilePath(char* output, std::size_t outputSize) const {
    const Entry* entry = selectedEntry();
    return entry && entry->kind == EntryKind::MidiFile &&
           buildPathForEntry(*entry, output, outputSize);
}

bool MidiFileManager::navigateInto(const Entry& entry) {
    if (entry.kind != EntryKind::Directory) return false;
#ifdef ARDUINO
    char path[kPathBytes]{};
    if (!buildPathForEntry(entry, path, sizeof(path)) || !SD.exists(path)) return false;
    File directory = SD.open(path);
    const bool valid = directory && directory.isDirectory();
    if (directory) directory.close();
    if (!valid) return false;
    copyText(currentPath_, sizeof(currentPath_), path);
    selection_ = 0;
    scroll_ = 0;
    refresh();
    return true;
#else
    (void)entry;
    return false;
#endif
}

bool MidiFileManager::navigateUp() {
    if (std::strcmp(currentPath_, "/midi") == 0) return false;
    char* slash = std::strrchr(currentPath_, '/');
    if (!slash || slash <= currentPath_ + 4) {
        copyText(currentPath_, sizeof(currentPath_), "/midi");
    } else {
        *slash = '\0';
    }
    selection_ = 0;
    scroll_ = 0;
    refresh();
    return true;
}

void MidiFileManager::moveSelection(int delta) {
    if (entryCount_ <= 0 || delta == 0) return;
    selection_ += delta;
    if (selection_ < 0) selection_ = 0;
    if (selection_ >= entryCount_) selection_ = entryCount_ - 1;
    ensureSelectionVisible();
}

void MidiFileManager::ensureSelectionVisible() {
    if (visibleRows_ < 1) visibleRows_ = 1;
    if (selection_ < scroll_) scroll_ = selection_;
    if (selection_ >= scroll_ + visibleRows_) {
        scroll_ = selection_ - visibleRows_ + 1;
    }
    const int maxScroll = std::max(0, entryCount_ - visibleRows_);
    if (scroll_ > maxScroll) scroll_ = maxScroll;
    if (scroll_ < 0) scroll_ = 0;
}

void MidiFileManager::selectEntryByName(const char* name, EntryKind kind) {
    if (!name || name[0] == '\0') return;
    for (int index = 0; index < entryCount_; ++index) {
        if (entries_[index].kind == kind &&
            equalIgnoreCase(entries_[index].name, name)) {
            selection_ = index;
            ensureSelectionVisible();
            return;
        }
    }
}

bool MidiFileManager::selectedFileIsInUse() const {
    const Entry* entry = selectedEntry();
    if (!entry || entry->kind != EntryKind::MidiFile) return false;
    GroovePuterMidi::ISmfPlayerService* player = GroovePuterMidi::smfPlayerService();
    if (!player) return false;
    const GroovePuterMidi::SmfPlayerSnapshot snapshot = player->snapshot();
    if (snapshot.state == GroovePuterMidi::SmfPlayerState::Unloaded ||
        snapshot.state == GroovePuterMidi::SmfPlayerState::Error ||
        snapshot.filename[0] == '\0') {
        return false;
    }
    return equalIgnoreCase(entry->name, basenameOf(snapshot.filename));
}

void MidiFileManager::beginRename() {
    const Entry* entry = selectedEntry();
    if (!entry || entry->kind != EntryKind::MidiFile) {
        UI::showToast("Select a MIDI file", 800);
        return;
    }
    if (selectedFileIsInUse()) {
        UI::showToast("MIDI file is in use", 1000);
        return;
    }
    copyText(renameBuffer_, sizeof(renameBuffer_), entry->name);
    const std::size_t length = std::strlen(renameBuffer_);
    if (length >= 4 && midiEndsWithExtension(renameBuffer_)) {
        renameBuffer_[length - 4] = '\0';
    }
    mode_ = Mode::Rename;
}

void MidiFileManager::commitRename() {
    const Entry* entry = selectedEntry();
    if (!entry || entry->kind != EntryKind::MidiFile) {
        cancelOperation();
        return;
    }
#ifdef ARDUINO
    char newName[kNameBytes]{};
    if (!buildMidiFilenameFromStem(renameBuffer_, newName, sizeof(newName))) {
        UI::showToast("Invalid MIDI name", 900);
        return;
    }
    if (equalIgnoreCase(newName, entry->name)) {
        cancelOperation();
        return;
    }

    char oldPath[kPathBytes]{};
    char newPath[kPathBytes]{};
    const int newPathLength =
        std::snprintf(newPath, sizeof(newPath), "%s/%s", currentPath_, newName);
    if (!buildPathForEntry(*entry, oldPath, sizeof(oldPath)) ||
        newPathLength <= 0 ||
        static_cast<std::size_t>(newPathLength) >= sizeof(newPath)) {
        UI::showToast("MIDI path too long", 900);
        return;
    }
    if (SD.exists(newPath)) {
        UI::showToast("Name already exists", 1000);
        return;
    }
    if (!SD.rename(oldPath, newPath)) {
        UI::showToast("Rename failed", 900);
        return;
    }
    mode_ = Mode::Browse;
    refresh();
    selectEntryByName(newName, EntryKind::MidiFile);
    UI::showToast("MIDI renamed", 800);
#else
    UI::showToast("SD unavailable", 900);
#endif
}

void MidiFileManager::beginDelete() {
    const Entry* entry = selectedEntry();
    if (!entry || entry->kind != EntryKind::MidiFile) {
        UI::showToast("Select a MIDI file", 800);
        return;
    }
    if (selectedFileIsInUse()) {
        UI::showToast("MIDI file is in use", 1000);
        return;
    }
    deleteConfirmed_ = false;
    mode_ = Mode::ConfirmDelete;
}

void MidiFileManager::commitDelete() {
    const Entry* entry = selectedEntry();
    if (!entry || entry->kind != EntryKind::MidiFile) {
        cancelOperation();
        return;
    }
#ifdef ARDUINO
    char path[kPathBytes]{};
    if (!buildPathForEntry(*entry, path, sizeof(path))) {
        UI::showToast("MIDI path too long", 900);
        return;
    }
    if (!SD.remove(path)) {
        UI::showToast("Delete failed", 900);
        return;
    }
    mode_ = Mode::Browse;
    refresh();
    UI::showToast("MIDI deleted", 800);
#else
    UI::showToast("SD unavailable", 900);
#endif
}

void MidiFileManager::cancelOperation() {
    mode_ = Mode::Browse;
    deleteConfirmed_ = false;
}

MidiFileManager::EventResult MidiFileManager::handleEvent(
        UIEvent& event,
        char* activatedPath,
        std::size_t activatedPathSize) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return EventResult::NotHandled;
    if (activatedPath && activatedPathSize > 0) activatedPath[0] = '\0';

    if (mode_ == Mode::Rename) {
        if (event.scancode == GROOVEPUTER_ESCAPE) {
            cancelOperation();
            return EventResult::Consumed;
        }
        if (event.key == '\b') {
            const std::size_t length = std::strlen(renameBuffer_);
            if (length > 0) renameBuffer_[length - 1] = '\0';
            return EventResult::Consumed;
        }
        if (event.key == '\n' || event.key == '\r') {
            commitRename();
            return EventResult::Consumed;
        }
        if (midiRenameCharacterAllowed(event.key)) {
            const std::size_t length = std::strlen(renameBuffer_);
            if (length + 5 < sizeof(renameBuffer_)) {
                renameBuffer_[length] = event.key;
                renameBuffer_[length + 1] = '\0';
            }
            return EventResult::Consumed;
        }
        return EventResult::Consumed;
    }

    if (mode_ == Mode::ConfirmDelete) {
        if (event.scancode == GROOVEPUTER_ESCAPE || event.key == '\b') {
            cancelOperation();
            return EventResult::Consumed;
        }
        if (event.scancode == GROOVEPUTER_LEFT || event.key == 'n' || event.key == 'N') {
            deleteConfirmed_ = false;
            return EventResult::Consumed;
        }
        if (event.scancode == GROOVEPUTER_RIGHT || event.key == 'y' || event.key == 'Y') {
            deleteConfirmed_ = true;
            return EventResult::Consumed;
        }
        if (event.key == '\n' || event.key == '\r') {
            if (deleteConfirmed_) commitDelete();
            else cancelOperation();
            return EventResult::Consumed;
        }
        return EventResult::Consumed;
    }

    if (event.scancode == GROOVEPUTER_UP) {
        moveSelection(-1);
        return EventResult::Consumed;
    }
    if (event.scancode == GROOVEPUTER_DOWN) {
        moveSelection(1);
        return EventResult::Consumed;
    }
    if (event.scancode == GROOVEPUTER_ESCAPE || event.key == '\b') {
        return navigateUp() ? EventResult::Consumed : EventResult::CloseRequested;
    }
    if (event.key == 'f' || event.key == 'F') {
        refresh();
        UI::showToast(storageReady_ ? "MIDI list refreshed" : "SD unavailable", 800);
        return EventResult::Consumed;
    }
    if (event.key == 'r' || event.key == 'R') {
        beginRename();
        return EventResult::Consumed;
    }
    if (event.key == 'x' || event.key == 'X') {
        beginDelete();
        return EventResult::Consumed;
    }
    if (event.key == '\n' || event.key == '\r') {
        const Entry* entry = selectedEntry();
        if (!entry) return EventResult::Consumed;
        if (entry->kind == EntryKind::Parent) {
            navigateUp();
            return EventResult::Consumed;
        }
        if (entry->kind == EntryKind::Directory) {
            if (!navigateInto(*entry)) UI::showToast("Folder unavailable", 900);
            return EventResult::Consumed;
        }
        if (activatedPath &&
            buildPathForEntry(*entry, activatedPath, activatedPathSize)) {
            return EventResult::FileActivated;
        }
        UI::showToast("MIDI path too long", 900);
        return EventResult::Consumed;
    }
    return EventResult::NotHandled;
}

void MidiFileManager::draw(IGfx& gfx,
                           const Rect& bounds,
                           const char* purposeLabel) {
    const int lineHeight = std::max(8, gfx.fontHeight());
    const int headerLines = 2;
    const int footerHeight = lineHeight + 4;
    const int listTop = bounds.y + headerLines * lineHeight + 4;
    const int listBottom = bounds.y + bounds.h - footerHeight - 2;
    visibleRows_ = std::max(1, (listBottom - listTop) / (lineHeight + 2));
    if (visibleRows_ > 7) visibleRows_ = 7;
    ensureSelectionVisible();

    gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, COLOR_BG);
    gfx.setTextColor(MusicVisuals::accentForStyle());
    char title[48];
    std::snprintf(title, sizeof(title), "MIDI FILES / %s",
                  purposeLabel ? purposeLabel : "BROWSE");
    Widgets::drawClippedText(gfx, bounds.x + 2, bounds.y,
                             bounds.w - 4, title);

    gfx.setTextColor(COLOR_LABEL);
    char pathLine[96];
    std::snprintf(pathLine, sizeof(pathLine), "%s  D%d F%d%s",
                  currentPath_, directoryCount_, fileCount_,
                  truncated_ ? " +" : "");
    Widgets::drawClippedText(gfx, bounds.x + 2, bounds.y + lineHeight,
                             bounds.w - 4, pathLine);

    drawRows(gfx, bounds, listTop, listBottom);

    gfx.setTextColor(COLOR_LABEL);
    Widgets::drawClippedText(gfx, bounds.x + 2,
                             bounds.y + bounds.h - footerHeight + 1,
                             bounds.w - 4,
                             "ENT OPEN  R RENAME  X DELETE  F REFRESH");

    if (mode_ == Mode::Rename) drawRenameOverlay(gfx, bounds);
    if (mode_ == Mode::ConfirmDelete) drawDeleteOverlay(gfx, bounds);
}

void MidiFileManager::drawRows(IGfx& gfx,
                               const Rect& bounds,
                               int listTop,
                               int listBottom) {
    const int rowHeight = std::max(10, gfx.fontHeight() + 2);
    if (!storageReady_) {
        gfx.setTextColor(COLOR_DANGER);
        gfx.drawText(bounds.x + 4, listTop + rowHeight, "SD UNAVAILABLE");
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(bounds.x + 4, listTop + rowHeight * 2, "F: RETRY");
        return;
    }
    if (entryCount_ == 0) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(bounds.x + 4, listTop + rowHeight, "NO MIDI FILES");
        gfx.drawText(bounds.x + 4, listTop + rowHeight * 2,
                     "COPY .MID TO /MIDI");
        return;
    }

    for (int row = 0; row < visibleRows_; ++row) {
        const int index = scroll_ + row;
        if (index >= entryCount_) break;
        const int y = listTop + row * rowHeight;
        if (y + rowHeight > listBottom) break;
        const Entry& entry = entries_[index];
        const bool selected = index == selection_;
        if (selected) {
            gfx.fillRect(bounds.x + 2, y, bounds.w - 4, rowHeight,
                         MusicVisuals::accentForStyle());
        }
        gfx.setTextColor(selected ? COLOR_BG : COLOR_TEXT);

        char sizeText[12]{};
        int sizeWidth = 0;
        if (entry.kind == EntryKind::MidiFile) {
            if (entry.sizeBytes < 1024u) {
                std::snprintf(sizeText, sizeof(sizeText), "%luB",
                              static_cast<unsigned long>(entry.sizeBytes));
            } else {
                std::snprintf(sizeText, sizeof(sizeText), "%luK",
                              static_cast<unsigned long>((entry.sizeBytes + 1023u) / 1024u));
            }
            sizeWidth = textWidth(gfx, sizeText) + 4;
        }

        char label[kNameBytes + 4]{};
        std::snprintf(label, sizeof(label), "%s%s", kindPrefix(entry.kind), entry.name);
        Widgets::drawClippedText(gfx, bounds.x + 4, y + 1,
                                 bounds.w - 8 - sizeWidth, label);
        if (sizeWidth > 0) {
            gfx.drawText(bounds.x + bounds.w - sizeWidth, y + 1, sizeText);
        }
    }
}

void MidiFileManager::drawRenameOverlay(IGfx& gfx, const Rect& bounds) {
    const int width = std::min(210, bounds.w - 12);
    const int height = 66;
    const int x = bounds.x + (bounds.w - width) / 2;
    const int y = bounds.y + (bounds.h - height) / 2;
    gfx.fillRect(x, y, width, height, COLOR_DARKER);
    gfx.drawRect(x, y, width, height, MusicVisuals::accentForStyle());
    gfx.setTextColor(COLOR_TEXT);
    gfx.drawText(x + 6, y + 5, "RENAME MIDI");
    gfx.fillRect(x + 5, y + 22, width - 10, 18, COLOR_PANEL);
    gfx.setTextColor(COLOR_WHITE);
    char input[kNameBytes + 8]{};
    std::snprintf(input, sizeof(input), "%s_.mid", renameBuffer_);
    Widgets::drawClippedText(gfx, x + 8, y + 26, width - 16, input);
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x + 6, y + 47, "ENTER SAVE   ESC CANCEL");
}

void MidiFileManager::drawDeleteOverlay(IGfx& gfx, const Rect& bounds) {
    const Entry* entry = selectedEntry();
    const int width = std::min(210, bounds.w - 12);
    const int height = 72;
    const int x = bounds.x + (bounds.w - width) / 2;
    const int y = bounds.y + (bounds.h - height) / 2;
    gfx.fillRect(x, y, width, height, COLOR_DARKER);
    gfx.drawRect(x, y, width, height, COLOR_DANGER);
    gfx.setTextColor(COLOR_DANGER);
    gfx.drawText(x + 6, y + 5, "DELETE MIDI FILE?");
    gfx.setTextColor(COLOR_TEXT);
    Widgets::drawClippedText(gfx, x + 6, y + 22, width - 12,
                             entry ? entry->name : "?");

    const int buttonY = y + 45;
    const int buttonWidth = 58;
    const int noX = x + width / 2 - buttonWidth - 6;
    const int yesX = x + width / 2 + 6;
    gfx.fillRect(noX, buttonY, buttonWidth, 18,
                 deleteConfirmed_ ? COLOR_PANEL : MusicVisuals::accentForStyle());
    gfx.fillRect(yesX, buttonY, buttonWidth, 18,
                 deleteConfirmed_ ? COLOR_DANGER : COLOR_PANEL);
    gfx.setTextColor(deleteConfirmed_ ? COLOR_LABEL : COLOR_BG);
    gfx.drawText(noX + 20, buttonY + 4, "NO");
    gfx.setTextColor(deleteConfirmed_ ? COLOR_WHITE : COLOR_LABEL);
    gfx.drawText(yesX + 16, buttonY + 4, "YES");
}

}  // namespace GroovePuterUi
