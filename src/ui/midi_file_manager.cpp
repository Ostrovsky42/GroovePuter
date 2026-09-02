#include "midi_file_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>

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
#include <cerrno>
#include <dirent.h>
#include <esp_heap_caps.h>
#include <sys/stat.h>
#include "src/platform/cardputer_sd.h"
#endif

namespace GroovePuterUi {
namespace {

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

#ifdef ARDUINO
struct DirectoryEntryInfo {
    const char* name{nullptr};
    bool directory{false};
    uint32_t sizeBytes{0};
};

bool buildVfsPath(const char* logicalPath,
                  const char* childName,
                  char* output,
                  std::size_t outputSize) {
    const char* mountpoint = SD.mountpoint();
    if (!mountpoint || !logicalPath || !output || outputSize == 0) return false;
    const int written = childName
        ? std::snprintf(output, outputSize, "%s%s/%s",
                        mountpoint, logicalPath, childName)
        : std::snprintf(output, outputSize, "%s%s", mountpoint, logicalPath);
    return written > 0 && static_cast<std::size_t>(written) < outputSize;
}

bool inspectDirectoryEntry(const char* logicalPath,
                           const dirent& raw,
                           bool includeSize,
                           DirectoryEntryInfo& info) {
    info = DirectoryEntryInfo{};
    if (raw.d_name[0] == '\0' || raw.d_name[0] == '.') return false;

    bool directory = false;
    if (raw.d_type == DT_DIR) {
        directory = true;
    } else if (raw.d_type != DT_REG) {
        char path[MidiFileManager::kPathBytes + MidiFileManager::kNameBytes + 8]{};
        struct stat metadata {};
        if (!buildVfsPath(logicalPath, raw.d_name, path, sizeof(path)) ||
            stat(path, &metadata) != 0) {
            return false;
        }
        directory = S_ISDIR(metadata.st_mode);
        if (!directory && !S_ISREG(metadata.st_mode)) return false;
        if (includeSize && !directory) {
            info.sizeBytes = static_cast<uint32_t>(metadata.st_size);
        }
    }

    if (!directory && !midiFilenameIsVisibleAndSupported(raw.d_name)) return false;
    if (std::strlen(raw.d_name) >= MidiFileManager::kNameBytes) return false;

    info.name = raw.d_name;
    info.directory = directory;
    if (includeSize && !directory && info.sizeBytes == 0) {
        char path[MidiFileManager::kPathBytes + MidiFileManager::kNameBytes + 8]{};
        struct stat metadata {};
        if (buildVfsPath(logicalPath, raw.d_name, path, sizeof(path)) &&
            stat(path, &metadata) == 0) {
            info.sizeBytes = static_cast<uint32_t>(metadata.st_size);
        }
    }
    return true;
}

void logDirectoryState(const char* state,
                       const char* stage,
                       const char* path,
                       int directories,
                       int files,
                       int shown,
                       int errorNumber) {
    const size_t freeInternal =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestInternal =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    Serial.printf("[MIDI-FILES] %s stage=%s path=%s dirs=%d files=%d shown=%d "
                  "errno=%d freeInt=%u largest=%u\n",
                  state ? state : "unknown",
                  stage ? stage : "unknown",
                  path ? path : "",
                  directories,
                  files,
                  shown,
                  errorNumber,
                  static_cast<unsigned>(freeInternal),
                  static_cast<unsigned>(largestInternal));
}
#endif

}  // namespace

MidiFileManager::MidiFileManager() = default;

void MidiFileManager::activateBrowserWorkspace() {
    if (browserWorkspaceActive_) return;
    new (&workspace_.entries) EntryWindow{};
    browserWorkspaceActive_ = true;
    windowStart_ = 0;
    windowCount_ = 0;
}

MidiImporter::ScanResult& MidiFileManager::beginImportScan() {
    new (&workspace_.importScan) MidiImporter::ScanResult{};
    browserWorkspaceActive_ = false;
    windowStart_ = 0;
    windowCount_ = 0;
    return workspace_.importScan;
}

const MidiImporter::ScanResult& MidiFileManager::importScanResult() const {
    return workspace_.importScan;
}

const char* MidiFileManager::storageFailureMessage() const {
    if (storageFailure_ == StorageFailure::DirectoryOpenFailed) {
        return "MIDI folder open failed";
    }
    if (storageFailure_ == StorageFailure::DirectoryReadFailed) {
        return "MIDI folder read failed";
    }
    return "SD unavailable";
}

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
    activateBrowserWorkspace();

    int previousIndex = -1;
    if (!scanDirectorySummary(previousName, previousKind, &previousIndex)) {
        selection_ = 0;
        scroll_ = 0;
        windowStart_ = 0;
        windowCount_ = 0;
        for (Entry& entry : workspace_.entries) entry = Entry{};
        return false;
    }
    if (previousIndex >= 0) selection_ = previousIndex;
    if (selection_ >= entryCount_) selection_ = entryCount_ > 0 ? entryCount_ - 1 : 0;
    if (selection_ < 0) selection_ = 0;
    ensureSelectionVisible();
    mode_ = Mode::Browse;
    deleteConfirmed_ = false;
    return storageReady_;
}

bool MidiFileManager::scanDirectorySummary(const char* selectedName,
                                           EntryKind selectedKind,
                                           int* selectedIndex) {
    entryCount_ = 0;
    directoryCount_ = 0;
    fileCount_ = 0;
    storageReady_ = false;
    truncated_ = false;
    storageFailure_ = StorageFailure::None;
    if (selectedIndex) *selectedIndex = -1;

#ifdef ARDUINO
    if (!GroovePuterPlatform::ensureCardputerSdMounted()) {
        storageFailure_ = StorageFailure::SdUnavailable;
        logDirectoryState("unavailable", "mount", currentPath_, 0, 0, 0, 0);
        return false;
    }

    bool exists = SD.exists(currentPath_);
    if (!exists && std::strcmp(currentPath_, "/midi") != 0) {
        copyText(currentPath_, sizeof(currentPath_), "/midi");
        exists = SD.exists(currentPath_);
    }
    if (!exists && std::strcmp(currentPath_, "/midi") == 0) {
        exists = SD.mkdir(currentPath_);
    }
    if (!exists) {
        storageFailure_ = StorageFailure::DirectoryOpenFailed;
        logDirectoryState("open failed", "exists", currentPath_, 0, 0, 0, errno);
        return false;
    }

    char vfsPath[kPathBytes + 8]{};
    if (!buildVfsPath(currentPath_, nullptr, vfsPath, sizeof(vfsPath))) {
        storageFailure_ = StorageFailure::DirectoryOpenFailed;
        logDirectoryState("open failed", "path", currentPath_, 0, 0, 0, ENAMETOOLONG);
        return false;
    }

    errno = 0;
    DIR* root = opendir(vfsPath);
    if (!root) {
        storageFailure_ = StorageFailure::DirectoryOpenFailed;
        logDirectoryState("open failed", "summary", currentPath_, 0, 0, 0, errno);
        return false;
    }
    storageReady_ = true;

    const int parentCount = std::strcmp(currentPath_, "/midi") != 0 ? 1 : 0;
    int selectedFileOffset = -1;

    int summaryError = 0;
    while (true) {
        errno = 0;
        dirent* raw = readdir(root);
        if (!raw) {
            summaryError = errno;
            break;
        }
        DirectoryEntryInfo info;
        if (!inspectDirectoryEntry(currentPath_, *raw, false, info)) {
            if (raw->d_name[0] != '\0' && raw->d_name[0] != '.' &&
                std::strlen(raw->d_name) >= kNameBytes) {
                truncated_ = true;
            }
            continue;
        }
        if (std::strlen(info.name) >= kNameBytes) {
            truncated_ = true;
            continue;
        }
        if (info.directory) {
            if (selectedIndex && selectedKind == EntryKind::Directory &&
                equalIgnoreCase(info.name, selectedName)) {
                *selectedIndex = parentCount + directoryCount_;
            }
            ++directoryCount_;
        } else {
            if (selectedKind == EntryKind::MidiFile &&
                equalIgnoreCase(info.name, selectedName)) {
                selectedFileOffset = fileCount_;
            }
            ++fileCount_;
        }
    }
    entryCount_ = parentCount + directoryCount_ + fileCount_;
    if (selectedIndex && selectedKind == EntryKind::Parent && parentCount > 0 &&
        equalIgnoreCase(selectedName, "..")) {
        *selectedIndex = 0;
    } else if (selectedIndex && selectedFileOffset >= 0) {
        *selectedIndex = parentCount + directoryCount_ + selectedFileOffset;
    }

    if (summaryError != 0) {
        closedir(root);
        storageReady_ = false;
        storageFailure_ = StorageFailure::DirectoryReadFailed;
        logDirectoryState("read incomplete", "summary", currentPath_,
                          directoryCount_, fileCount_, 0, summaryError);
        return false;
    }

    const int targetSelection = selectedIndex && *selectedIndex >= 0
        ? *selectedIndex
        : std::max(0, std::min(selection_, entryCount_ > 0 ? entryCount_ - 1 : 0));
    const int maxScroll = std::max(0, entryCount_ - visibleRows_);
    scroll_ = std::max(0, std::min(targetSelection, maxScroll));
    const int maxWindowStart = std::max(0, entryCount_ - kWindowEntries);
    windowStart_ = std::max(0, std::min(scroll_, maxWindowStart));
    windowCount_ = std::min(kWindowEntries, entryCount_ - windowStart_);
    for (Entry& entry : workspace_.entries) entry = Entry{};
    if (parentCount > 0 && windowStart_ == 0 && windowCount_ > 0) {
        workspace_.entries[0].kind = EntryKind::Parent;
        copyText(workspace_.entries[0].name,
                 sizeof(workspace_.entries[0].name), "..");
    }

    rewinddir(root);
    int secondDirectoryCount = 0;
    int secondFileCount = 0;
    int windowError = 0;
    while (true) {
        errno = 0;
        dirent* raw = readdir(root);
        if (!raw) {
            windowError = errno;
            break;
        }
        DirectoryEntryInfo info;
        if (!inspectDirectoryEntry(currentPath_, *raw, true, info)) continue;
        const int index = info.directory
            ? parentCount + secondDirectoryCount++
            : parentCount + directoryCount_ + secondFileCount++;
        if (index >= windowStart_ && index < windowStart_ + windowCount_) {
            Entry& entry = workspace_.entries[index - windowStart_];
            copyText(entry.name, sizeof(entry.name), info.name);
            entry.kind = info.directory ? EntryKind::Directory : EntryKind::MidiFile;
            entry.sizeBytes = info.directory ? 0u : info.sizeBytes;
        }
    }
    closedir(root);
    if (windowError != 0 || secondDirectoryCount != directoryCount_ ||
        secondFileCount != fileCount_) {
        storageReady_ = false;
        storageFailure_ = StorageFailure::DirectoryReadFailed;
        windowCount_ = 0;
        logDirectoryState("read incomplete", "initial-window", currentPath_,
                          secondDirectoryCount, secondFileCount, 0, windowError);
        return false;
    }
    logDirectoryState(entryCount_ == 0 ? "empty" : "ready", "initial-window",
                      currentPath_, directoryCount_, fileCount_, windowCount_, 0);
#else
    (void)selectedName;
    (void)selectedKind;
#endif
    return storageReady_;
}

bool MidiFileManager::loadWindow(int firstIndex) {
    activateBrowserWorkspace();
    for (Entry& entry : workspace_.entries) entry = Entry{};
    windowCount_ = 0;
    if (!storageReady_ || entryCount_ <= 0) {
        windowStart_ = 0;
        return storageReady_;
    }
    const int maxStart = std::max(0, entryCount_ - kWindowEntries);
    windowStart_ = std::max(0, std::min(firstIndex, maxStart));
    windowCount_ = std::min(kWindowEntries, entryCount_ - windowStart_);

#ifdef ARDUINO
    char vfsPath[kPathBytes + 8]{};
    if (!buildVfsPath(currentPath_, nullptr, vfsPath, sizeof(vfsPath))) {
        storageReady_ = false;
        storageFailure_ = StorageFailure::DirectoryOpenFailed;
        logDirectoryState("open failed", "window-path", currentPath_,
                          directoryCount_, fileCount_, 0, ENAMETOOLONG);
        windowCount_ = 0;
        return false;
    }

    errno = 0;
    DIR* root = opendir(vfsPath);
    if (!root) {
        storageReady_ = false;
        storageFailure_ = GroovePuterPlatform::cardputerSdMounted()
            ? StorageFailure::DirectoryOpenFailed
            : StorageFailure::SdUnavailable;
        logDirectoryState("open failed", "window", currentPath_,
                          directoryCount_, fileCount_, 0, errno);
        windowCount_ = 0;
        return false;
    }

    const int parentCount = std::strcmp(currentPath_, "/midi") != 0 ? 1 : 0;
    if (parentCount > 0 && windowStart_ == 0) {
        workspace_.entries[0].kind = EntryKind::Parent;
        copyText(workspace_.entries[0].name,
                 sizeof(workspace_.entries[0].name), "..");
    }
    int directoryIndex = 0;
    int fileIndex = 0;
    int readError = 0;
    while (true) {
        errno = 0;
        dirent* raw = readdir(root);
        if (!raw) {
            readError = errno;
            break;
        }
        DirectoryEntryInfo info;
        if (!inspectDirectoryEntry(currentPath_, *raw, true, info)) continue;
        const int index = info.directory
            ? parentCount + directoryIndex++
            : parentCount + directoryCount_ + fileIndex++;
        if (index >= windowStart_ && index < windowStart_ + windowCount_) {
            Entry& entry = workspace_.entries[index - windowStart_];
            copyText(entry.name, sizeof(entry.name), info.name);
            entry.kind = info.directory ? EntryKind::Directory : EntryKind::MidiFile;
            entry.sizeBytes = info.directory ? 0u : info.sizeBytes;
        }
    }
    closedir(root);
    if (readError != 0 || directoryIndex != directoryCount_ ||
        fileIndex != fileCount_) {
        storageReady_ = false;
        storageFailure_ = StorageFailure::DirectoryReadFailed;
        windowCount_ = 0;
        logDirectoryState("read incomplete", "window", currentPath_,
                          directoryIndex, fileIndex, 0, readError);
        return false;
    }
    logDirectoryState("ready", "window", currentPath_, directoryCount_,
                      fileCount_, windowCount_, 0);
#endif
    return true;
}

const MidiFileManager::Entry* MidiFileManager::entryAt(int index) const {
    if (!browserWorkspaceActive_) return nullptr;
    if (index < windowStart_ || index >= windowStart_ + windowCount_) return nullptr;
    return &workspace_.entries[index - windowStart_];
}

MidiFileManager::Entry* MidiFileManager::entryAt(int index) {
    if (!browserWorkspaceActive_) return nullptr;
    if (index < windowStart_ || index >= windowStart_ + windowCount_) return nullptr;
    return &workspace_.entries[index - windowStart_];
}

const MidiFileManager::Entry* MidiFileManager::selectedEntry() const {
    if (selection_ < 0 || selection_ >= entryCount_) return nullptr;
    return entryAt(selection_);
}

MidiFileManager::Entry* MidiFileManager::selectedEntry() {
    if (selection_ < 0 || selection_ >= entryCount_) return nullptr;
    return entryAt(selection_);
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
    char previousPath[kPathBytes]{};
    copyText(previousPath, sizeof(previousPath), currentPath_);
    copyText(currentPath_, sizeof(currentPath_), path);
    selection_ = 0;
    scroll_ = 0;
    if (refresh()) return true;
    copyText(currentPath_, sizeof(currentPath_), previousPath);
    refresh();
    return false;
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
    const int requiredEnd = std::min(entryCount_, scroll_ + visibleRows_);
    if (scroll_ < windowStart_ || requiredEnd > windowStart_ + windowCount_) {
        loadWindow(scroll_);
    }
}

void MidiFileManager::selectEntryByName(const char* name, EntryKind kind) {
    if (!name || name[0] == '\0') return;
    int index = -1;
    if (scanDirectorySummary(name, kind, &index) && index >= 0) {
        windowCount_ = 0;
        selection_ = index;
        ensureSelectionVisible();
    }
}

bool MidiFileManager::selectedFileIsInUse() const {
    const Entry* entry = selectedEntry();
    if (!entry || entry->kind != EntryKind::MidiFile) return false;
    GroovePuterMidi::ISmfPlayerService* player = GroovePuterMidi::smfPlayerService();
    if (!player) return false;
    char selectedPath[kPathBytes]{};
    char loadedPath[kPathBytes]{};
    if (!buildPathForEntry(*entry, selectedPath, sizeof(selectedPath)) ||
        !player->currentFilePath(loadedPath, sizeof(loadedPath))) {
        return false;
    }
    return equalIgnoreCase(selectedPath, loadedPath);
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
        UI::showToast(storageReady_ ? "MIDI list refreshed"
                                    : storageFailureMessage(),
                      800);
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
    char pathLine[kPathBytes + 24]{};
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
        const char* failure = "SD UNAVAILABLE";
        if (storageFailure_ == StorageFailure::DirectoryOpenFailed) {
            failure = "MIDI FOLDER OPEN FAILED";
        } else if (storageFailure_ == StorageFailure::DirectoryReadFailed) {
            failure = "MIDI FOLDER READ FAILED";
        }
        gfx.drawText(bounds.x + 4, listTop + rowHeight, failure);
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
        const Entry* entryPtr = entryAt(index);
        if (!entryPtr) break;
        const Entry& entry = *entryPtr;
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