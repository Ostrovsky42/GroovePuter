#pragma once

#include <stdint.h>
#include <string>
#include "src/dsp/miniacid_engine.h"

#ifdef ARDUINO
#include <FS.h>
#include <SD.h>
#endif

/**
 * @brief Service to import Standard MIDI Files (.mid) into GroovePuter patterns.
 */
class MidiImporter {
public:
    enum class Error {
        None,
        FileNotFound,
        InvalidFormat,
        UnsupportedType, // Only Type 0 and 1 supported
        ReadError,
        NoNotesFound    // No compatible notes found
    };

    struct ImportSettings {
        int targetPatternIndex = 0;   // Starting pattern index to fill
        int startStepOffset = 0;      // Skip this many steps before importing (16 steps per pattern)
        int sourceStartBar = 0;       // Slice start in bars (relative to first routed note)
        int sourceLengthBars = 0;     // Slice length in bars; <=0 means unlimited
        bool overwrite = true;        // Overwrite existing patterns (ignored if target is occupied and we find free block)
        int drumChannel = 10;         // MIDI channel for drums (1-indexed, usually 10)
        int synthAChannel = 1;        // MIDI channel for Synth A
        int synthBChannel = 2;        // MIDI channel for Synth B
        bool quantize = true;         // Always true for now as engine is step-based
        bool omni = true;             // Route any non-drum notes to Synth A
        bool loudMode = true;         // true=LOUD profile, false=CLEAN profile
        
        // Track routing: 0=Synth A, 1=Synth B, 2=Drums, -1=Skip
        int destSynthA = 0;
        int destSynthB = 1;
        int destDrums = 2;
    };

    /// Per-channel statistics collected by scanFile()
    struct ChannelInfo {
        int noteCount = 0;       ///< Total note-on events
        uint8_t minNote = 127;   ///< Lowest note seen
        uint8_t maxNote = 0;     ///< Highest note seen
        char trackName[16] = {}; ///< First track name for this channel (truncated)
        bool used() const { return noteCount > 0; }
    };

    /// Result of scanFile()
    struct ScanResult {
        bool valid = false;
        uint16_t format = 0;
        uint16_t numTracks = 0;
        uint16_t division = 0;
        int totalNotes = 0;
        int usedChannels = 0;
        int estimatedBars = 0;   ///< Rough bar count based on tick range
        ChannelInfo channels[16]; ///< 1-indexed channels stored at [ch-1]
    };

    explicit MidiImporter(MiniAcid& engine);

    /**
     * @brief Import a MIDI file from the SD card.
     * @param path Full path to the .mid file.
     * @param settings Configuration for the import.
     * @return Error code.
     */
    Error importFile(const std::string& path, const ImportSettings& settings);

    /**
     * @brief Scan a MIDI file and return per-channel statistics without importing.
     * @param path Full path to the .mid file.
     * @return ScanResult with channel info.
     */
    ScanResult scanFile(const std::string& path);

    /**
     * @brief Get a descriptive error message.
     */
    std::string getErrorString(Error error) const;

    /**
     * @brief Get the last pattern index that was actually written to during import.
     * Useful for determining the song length after a "Full" import.
     */
    int getLastImportedPatternIdx() const { return lastImportedPatternIdx_; }

private:
    MiniAcid& engine_;
    int cachedPageIndex_ = -1;
    int lastImportedPatternIdx_ = -1;

    struct MidiHeader {
        uint16_t format;      // 0, 1, or 2
        uint16_t numTracks;
        uint16_t division;    // Ticks per quarter note
    };

    Error parseFile(File& file, const ImportSettings& settings);

    bool loadPageToCache(int pageIndex);
    bool saveCacheToPage();
    
    SynthPattern& getSynthPattern(int synthIdx, int patternIdx);
    DrumPatternSet& getDrumPatternSet(int patternIdx);

    bool readU8(File& file, uint8_t& out);
    bool readVarLen(File& file, uint32_t& value, uint32_t& bytesRead);
    bool readBE32(File& file, uint32_t& out);
    bool readBE16(File& file, uint16_t& out);
};
