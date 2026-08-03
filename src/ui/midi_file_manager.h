#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "ui_core.h"
#include "src/audio/midi_importer.h"

namespace GroovePuterUi {

class MidiFileManager {
public:
    enum class EntryKind : uint8_t {
        Parent = 0,
        Directory,
        MidiFile,
    };

    enum class Mode : uint8_t {
        Browse = 0,
        Rename,
        ConfirmDelete,
    };

    enum class EventResult : uint8_t {
        NotHandled = 0,
        Consumed,
        FileActivated,
        CloseRequested,
    };

    // Only the visible directory window stays resident. Moving beyond it
    // rescans the directory, so capacity never limits file reachability.
    static constexpr int kWindowEntries = 8;
    static constexpr std::size_t kNameBytes = 64;
    static constexpr std::size_t kPathBytes = 128;

    struct Entry {
        char name[kNameBytes]{};
        uint32_t sizeBytes{0};
        EntryKind kind{EntryKind::MidiFile};
    };

    MidiFileManager();

    void open();
    bool refresh();
    void draw(IGfx& gfx, const Rect& bounds, const char* purposeLabel);
    EventResult handleEvent(UIEvent& event,
                            char* activatedPath,
                            std::size_t activatedPathSize);

    bool selectedFilePath(char* output, std::size_t outputSize) const;
    const char* currentPath() const { return currentPath_; }
    int entryCount() const { return entryCount_; }
    int directoryCount() const { return directoryCount_; }
    int fileCount() const { return fileCount_; }
    int selection() const { return selection_; }
    Mode mode() const { return mode_; }
    bool storageReady() const { return storageReady_; }
    bool truncated() const { return truncated_; }

    // The browser window and import scan are never needed at the same time.
    // Reusing this static storage keeps the large scan result off the
    // fragmented Cardputer heap while Project -> Import MIDI is open.
    MidiImporter::ScanResult& beginImportScan();
    const MidiImporter::ScanResult& importScanResult() const;

private:
    enum class StorageFailure : uint8_t {
        None = 0,
        SdUnavailable,
        DirectoryOpenFailed,
        DirectoryReadFailed,
    };

    using EntryWindow = std::array<Entry, kWindowEntries>;
    union Workspace {
        EntryWindow entries;
        MidiImporter::ScanResult importScan;

        Workspace() : entries{} {}
    };
    static_assert(sizeof(MidiImporter::ScanResult) <= sizeof(EntryWindow),
                  "MIDI import scan must fit in the browser workspace");
    static_assert(std::is_trivially_destructible<EntryWindow>::value &&
                      std::is_trivially_destructible<
                          MidiImporter::ScanResult>::value,
                  "MIDI workspace members must not require destruction");

    void activateBrowserWorkspace();
    const char* storageFailureMessage() const;
    const Entry* entryAt(int index) const;
    Entry* entryAt(int index);
    const Entry* selectedEntry() const;
    Entry* selectedEntry();
    bool scanDirectorySummary(const char* selectedName,
                              EntryKind selectedKind,
                              int* selectedIndex);
    bool loadWindow(int firstIndex);
    bool buildPathForEntry(const Entry& entry,
                           char* output,
                           std::size_t outputSize) const;
    bool navigateInto(const Entry& entry);
    bool navigateUp();
    void moveSelection(int delta);
    void ensureSelectionVisible();
    void selectEntryByName(const char* name, EntryKind kind);
    void beginRename();
    void commitRename();
    void beginDelete();
    void commitDelete();
    void cancelOperation();
    bool selectedFileIsInUse() const;
    void drawRows(IGfx& gfx, const Rect& bounds, int listTop, int listBottom);
    void drawRenameOverlay(IGfx& gfx, const Rect& bounds);
    void drawDeleteOverlay(IGfx& gfx, const Rect& bounds);

    Workspace workspace_{};
    char currentPath_[kPathBytes]{"/midi"};
    char renameBuffer_[kNameBytes]{};
    int entryCount_{0};
    int windowStart_{0};
    int windowCount_{0};
    int directoryCount_{0};
    int fileCount_{0};
    int selection_{0};
    int scroll_{0};
    int visibleRows_{6};
    Mode mode_{Mode::Browse};
    bool storageReady_{false};
    bool truncated_{false};
    bool deleteConfirmed_{false};
    bool browserWorkspaceActive_{true};
    StorageFailure storageFailure_{StorageFailure::None};
};

static_assert(sizeof(MidiFileManager::Entry) <= 72,
              "MIDI browser entry must stay compact");
static_assert(sizeof(MidiFileManager) <= 1024,
              "MIDI file manager must stay below 1 KiB without PSRAM");

MidiFileManager& midiFileManager();

}  // namespace GroovePuterUi
