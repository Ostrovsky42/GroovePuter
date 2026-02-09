#pragma once

#include <stdint.h>
#include <string>
#include "src/dsp/miniacid_engine.h"

#include <FS.h>
#include <SD.h>

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
        bool overwrite = true;        // Overwrite existing patterns
        int drumChannel = 10;         // MIDI channel for drums (1-indexed, usually 10)
        int synthAChannel = 1;        // MIDI channel for Synth A
        int synthBChannel = 2;        // MIDI channel for Synth B
        bool quantize = true;         // Always true for now as engine is step-based
        bool omni = true;             // Route any non-drum notes to Synth A
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
     * @brief Get a descriptive error message.
     */
    std::string getErrorString(Error error) const;

private:
    MiniAcid& engine_;
    int cachedPageIndex_ = -1;

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
