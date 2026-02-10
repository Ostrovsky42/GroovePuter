#include "midi_importer.h"
#include <Arduino.h>
#include "../../scenes.h"
#include <SD.h>

namespace {
static inline int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Keep imported dynamics musical but avoid clipping-heavy accents on tiny hardware.
static inline uint8_t normalizeSynthVelocity(uint8_t midiVelocity, bool loudMode) {
    int v = clampInt(static_cast<int>(midiVelocity), 1, 127);
    if (loudMode) return static_cast<uint8_t>(68 + ((v * 26) / 127)); // ~68..94
    return static_cast<uint8_t>(60 + ((v * 20) / 127));               // ~60..80
}

static inline uint8_t normalizeDrumVelocity(uint8_t midiVelocity, bool loudMode) {
    int v = clampInt(static_cast<int>(midiVelocity), 1, 127);
    if (loudMode) return static_cast<uint8_t>(60 + ((v * 30) / 127)); // ~60..90
    return static_cast<uint8_t>(54 + ((v * 24) / 127));               // ~54..78
}
} // namespace

MidiImporter::MidiImporter(MiniAcid& engine) : engine_(engine) {}

MidiImporter::Error MidiImporter::importFile(const std::string& path, const MidiImporter::ImportSettings& settings) {
    Serial.printf("[MidiImporter] importFile: %s\n", path.c_str());
    if (!SD.exists(path.c_str())) return Error::FileNotFound;

    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) return Error::FileNotFound;

    auto& sceneManager = engine_.sceneManager();
    const int originalPageIndex = sceneManager.currentPageIndex();
    cachedPageIndex_ = -1;
    lastImportedPatternIdx_ = -1;
    Error err = parseFile(file, settings);
    saveCacheToPage(); // Flush final touched page.
    if (sceneManager.currentPageIndex() != originalPageIndex) {
        sceneManager.setPage(originalPageIndex);
    }
    file.close();
    return err;
}

std::string MidiImporter::getErrorString(Error error) const {
    switch (error) {
        case Error::None: return "Success";
        case Error::FileNotFound: return "File not found";
        case Error::InvalidFormat: return "Invalid MIDI format";
        case Error::UnsupportedType: return "Unsupported MIDI type (use 0 or 1)";
        case Error::ReadError: return "Read error";
        case Error::NoNotesFound: return "No compatible notes found";
        default: return "Unknown error";
    }
}

MidiImporter::Error MidiImporter::parseFile(File& file, const MidiImporter::ImportSettings& settings) {
    char magic[4];
    if (file.readBytes(magic, 4) != 4 || memcmp(magic, "MThd", 4) != 0) {
        return Error::InvalidFormat;
    }

    uint32_t headerSize = 0;
    if (!readBE32(file, headerSize)) return Error::ReadError;
    if (headerSize < 6) return Error::InvalidFormat;

    MidiHeader header;
    if (!readBE16(file, header.format)) return Error::ReadError;
    if (!readBE16(file, header.numTracks)) return Error::ReadError;
    if (!readBE16(file, header.division)) return Error::ReadError;

    if (header.format > 1) return Error::UnsupportedType;
    if (header.division & 0x8000) return Error::UnsupportedType; // SMPTE not supported
    if (header.division == 0) return Error::InvalidFormat;
    if (header.numTracks == 0 || header.numTracks > 64) return Error::InvalidFormat;

    // Skip remaining header if any
    uint32_t fileSize = file.size();
    if (headerSize > 6) {
        uint32_t skip = headerSize - 6;
        uint32_t newPos = file.position() + skip;
        if (newPos > fileSize) return Error::ReadError;
        file.seek(newPos);
    }

    int notesImported = 0;
    // Using global constants from scenes.h: kPatternsPerPage, kMaxPages, kMaxPatterns
    bool importRegionCleared = false;
    int firstRoutedStep = -1;
    int sourceStartSteps = settings.sourceStartBar * 16;
    if (sourceStartSteps < 0) sourceStartSteps = 0;
    int sourceLengthSteps = 0;
    if (settings.sourceLengthBars > 0) {
        sourceLengthSteps = settings.sourceLengthBars * 16;
        if (sourceLengthSteps < 1) sourceLengthSteps = 1;
    }

    double ticksPerStep = static_cast<double>(header.division) / 4.0; // fixed 1/16 grid
    if (ticksPerStep < 1.0) ticksPerStep = 1.0;

    int tracksParsed = 0;
    while (file.position() + 8 <= fileSize) {
        if (file.readBytes(magic, 4) != 4) return Error::ReadError;

        uint32_t chunkSize = 0;
        if (!readBE32(file, chunkSize)) return Error::ReadError;
        uint32_t chunkEnd = file.position() + chunkSize;
        if (chunkEnd > fileSize) return Error::InvalidFormat;

        if (memcmp(magic, "MTrk", 4) != 0) {
            // Tolerate non-track chunks between tracks.
            file.seek(chunkEnd);
            continue;
        }
        tracksParsed++;
        if (tracksParsed > 128) break;

        uint32_t trackEnd = chunkEnd;
        uint32_t absoluteTicks = 0;
        uint8_t lastStatus = 0;

        while (file.position() < trackEnd) {
            if (file.position() > trackEnd) return Error::InvalidFormat;
            uint32_t bytesRead = 0;
            uint32_t deltaTime = 0;
            if (!readVarLen(file, deltaTime, bytesRead)) return Error::ReadError;
            if (file.position() > trackEnd) return Error::InvalidFormat;
            absoluteTicks += deltaTime;

            int status = file.read();
            if (status < 0) return Error::ReadError;
            if (file.position() > trackEnd) return Error::InvalidFormat;

            if (!(status & 0x80)) {
                // Running status
                if (lastStatus < 0x80 || lastStatus >= 0xF0) return Error::InvalidFormat;
                file.seek(file.position() - 1);
                status = lastStatus;
            } else {
                // Running status is valid only for channel voice messages.
                if (status < 0xF0) lastStatus = static_cast<uint8_t>(status);
            }

            uint8_t msgType = status & 0xF0;
            uint8_t channel = (status & 0x0F) + 1;

            if (msgType == 0x80 || msgType == 0x90) {
                // Note Off or Note On
                if (file.position() + 2 > trackEnd) return Error::InvalidFormat;
                uint8_t note = 0;
                uint8_t velocity = 0;
                if (!readU8(file, note)) return Error::ReadError;
                if (!readU8(file, velocity)) return Error::ReadError;
                bool isNoteOn = (msgType == 0x90 && velocity > 0);

                if (isNoteOn) {
                    // Quantize to fixed 1/16 grid.
                    int stepIdx = static_cast<int>((static_cast<double>(absoluteTicks) + (ticksPerStep * 0.5)) / ticksPerStep);
                    bool routeToA = (channel == settings.synthAChannel);
                    bool routeToB = (channel == settings.synthBChannel);
                    bool routeToDrums = (channel == settings.drumChannel);
                    if (settings.omni && !routeToA && !routeToB && !routeToDrums) {
                        routeToA = true;
                    }
                    if (!routeToA && !routeToB && !routeToDrums) {
                        continue;
                    }
                    if (firstRoutedStep < 0) {
                        firstRoutedStep = stepIdx;
                    }

                    int adjustedStep = stepIdx - firstRoutedStep - settings.startStepOffset - sourceStartSteps;
                    if (adjustedStep < 0) {
                        continue;
                    }
                    if (sourceLengthSteps > 0 && adjustedStep >= sourceLengthSteps) {
                        continue;
                    }
                    int patternIdx = adjustedStep / 16 + settings.targetPatternIndex;
                    int stepInPattern = adjustedStep % 16;
                    if (patternIdx < 0 || patternIdx >= kMaxPatterns) {
                        continue;
                    }

                    if (settings.overwrite && !importRegionCleared) {
                        int clearFrom = settings.targetPatternIndex;
                        if (clearFrom < 0) clearFrom = 0;
                        if (clearFrom > kMaxPatterns) clearFrom = kMaxPatterns;
                        int clearTo = kMaxPatterns;
                        if (sourceLengthSteps > 0) {
                            // Overwrite only the selected import window, not the whole tail of the song.
                            clearTo = clearFrom + ((sourceLengthSteps + 15) / 16);
                            if (clearTo < clearFrom) clearTo = clearFrom;
                            if (clearTo > kMaxPatterns) clearTo = kMaxPatterns;
                        }
                        for (int p = clearFrom; p < clearTo; ++p) {
                            SynthPattern& synthA = getSynthPattern(0, p);
                            SynthPattern& synthB = getSynthPattern(1, p);
                            DrumPatternSet& drums = getDrumPatternSet(p);
                            for (int i = 0; i < SynthPattern::kSteps; ++i) {
                                synthA.steps[i] = SynthStep{};
                                synthB.steps[i] = SynthStep{};
                                synthA.steps[i].note = -1; // Rest (note=0 is a valid note and causes unwanted pulses)
                                synthB.steps[i].note = -1;
                            }
                            for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
                                for (int i = 0; i < DrumPattern::kSteps; ++i) {
                                    drums.voices[v].steps[i] = DrumStep{};
                                }
                            }
                        }
                        importRegionCleared = true;
                    }

                        if (routeToA) {
                            SynthPattern& pat = getSynthPattern(0, patternIdx);
                            if (!settings.overwrite && pat.steps[stepInPattern].note >= 0) {
                                // Keep existing note
                            } else {
                                int safeNote = clampInt(static_cast<int>(note), MiniAcid::kMin303Note, MiniAcid::kMax303Note);
                                uint8_t safeVel = normalizeSynthVelocity(velocity, settings.loudMode);
                                pat.steps[stepInPattern].note = safeNote;
                                pat.steps[stepInPattern].accent = false;
                                pat.steps[stepInPattern].velocity = safeVel;
                                notesImported++;
                                if (patternIdx > lastImportedPatternIdx_) lastImportedPatternIdx_ = patternIdx;
                            }
                        } else if (routeToB) {
                            SynthPattern& pat = getSynthPattern(1, patternIdx);
                            if (!settings.overwrite && pat.steps[stepInPattern].note >= 0) {
                                // Keep existing note
                            } else {
                                int safeNote = clampInt(static_cast<int>(note), MiniAcid::kMin303Note, MiniAcid::kMax303Note);
                                uint8_t safeVel = normalizeSynthVelocity(velocity, settings.loudMode);
                                pat.steps[stepInPattern].note = safeNote;
                                pat.steps[stepInPattern].accent = false;
                                pat.steps[stepInPattern].velocity = safeVel;
                                notesImported++;
                                if (patternIdx > lastImportedPatternIdx_) lastImportedPatternIdx_ = patternIdx;
                            }
                        } else if (routeToDrums) {
                         int drumVoice = -1;
                         switch(note) {
                             case 35: case 36: drumVoice = 0; break; // Kick
                             case 38: case 40: drumVoice = 1; break; // Snare
                             case 42: case 44: drumVoice = 2; break; // CHat
                             case 46: drumVoice = 3; break;           // OHat
                             case 41: case 43: drumVoice = 4; break; // MTom
                             case 45: case 47: case 48: drumVoice = 5; break; // HTom
                             case 37: drumVoice = 6; break;           // Rim
                             case 39: drumVoice = 7; break;           // Clap
                         }
                         if (drumVoice >= 0) {
                             DrumPatternSet& patSet = getDrumPatternSet(patternIdx);
                             auto& step = patSet.voices[drumVoice].steps[stepInPattern];
                                 if (!settings.overwrite && step.hit) {
                                     // Keep existing hit
                                 } else {
                                     uint8_t safeVel = normalizeDrumVelocity(velocity, settings.loudMode);
                                     step.hit = true;
                                     step.hit = true;
                                     step.velocity = safeVel;
                                     notesImported++;
                                     if (patternIdx > lastImportedPatternIdx_) lastImportedPatternIdx_ = patternIdx;
                                 }
                             }
                    }
                }
            } else if (msgType == 0xA0 || msgType == 0xB0 || msgType == 0xE0) {
                if (file.position() + 2 > trackEnd) return Error::InvalidFormat;
                uint8_t tmp = 0;
                if (!readU8(file, tmp)) return Error::ReadError;
                if (!readU8(file, tmp)) return Error::ReadError;
            } else if (msgType == 0xC0 || msgType == 0xD0) {
                if (file.position() + 1 > trackEnd) return Error::InvalidFormat;
                uint8_t tmp = 0;
                if (!readU8(file, tmp)) return Error::ReadError;
            } else if (status == 0xFF) {
                // Meta Event
                if (file.position() + 1 > trackEnd) return Error::InvalidFormat;
                uint8_t metaType = 0;
                if (!readU8(file, metaType)) return Error::ReadError;
                uint32_t metaLen = 0;
                if (!readVarLen(file, metaLen, bytesRead)) return Error::ReadError;
                if (metaType == 0x2F) { // End of Track
                    // Be tolerant: some files may carry non-zero payload here.
                    uint32_t newPos = file.position() + metaLen;
                    if (newPos > trackEnd) return Error::InvalidFormat;
                    file.seek(newPos);
                    file.seek(trackEnd);
                    break;
                }
                if (metaType == 0x58) { // Time Signature
                    // We quantize to fixed 1/16, so just skip this payload.
                    uint32_t newPos = file.position() + metaLen;
                    if (newPos > trackEnd) return Error::InvalidFormat;
                    file.seek(newPos);
                } else {
                    uint32_t newPos = file.position() + metaLen;
                    if (newPos > trackEnd) return Error::InvalidFormat;
                    file.seek(newPos);
                }
            } else if (status == 0xF0 || status == 0xF7) {
                // SysEx
                uint32_t sysexLen = 0;
                if (!readVarLen(file, sysexLen, bytesRead)) return Error::ReadError;
                uint32_t newPos = file.position() + sysexLen;
                if (newPos > trackEnd) return Error::InvalidFormat;
                file.seek(newPos);
            } else if (status == 0xF1 || status == 0xF3) {
                if (file.position() + 1 > trackEnd) return Error::InvalidFormat;
                uint8_t tmp = 0;
                if (!readU8(file, tmp)) return Error::ReadError;
            } else if (status == 0xF2) {
                if (file.position() + 2 > trackEnd) return Error::InvalidFormat;
                uint8_t tmp = 0;
                if (!readU8(file, tmp)) return Error::ReadError;
                if (!readU8(file, tmp)) return Error::ReadError;
            } else if ((status >= 0xF4 && status <= 0xF6) || (status >= 0xF8 && status <= 0xFE)) {
                // System common/realtime bytes with no payload in SMF stream.
            } else {
                return Error::InvalidFormat;
            }
        }

        // Ensure parser is aligned to the next chunk boundary.
        if (file.position() != trackEnd) {
            file.seek(trackEnd);
        }
    }
    if (tracksParsed == 0) return Error::InvalidFormat;

    if (notesImported == 0) return Error::NoNotesFound;

    return Error::None;
}

bool MidiImporter::readU8(File& file, uint8_t& out) {
    int value = file.read();
    if (value < 0) return false;
    out = static_cast<uint8_t>(value);
    return true;
}

bool MidiImporter::readVarLen(File& file, uint32_t& value, uint32_t& bytesRead) {
    value = 0;
    bytesRead = 0;
    uint8_t byte = 0;
    do {
        if (!readU8(file, byte)) return false;
        bytesRead++;
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80 && bytesRead < 4);
    return true;
}

bool MidiImporter::readBE32(File& file, uint32_t& out) {
    uint32_t val = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = 0;
        if (!readU8(file, byte)) return false;
        val = (val << 8) | byte;
    }
    out = val;
    return true;
}

bool MidiImporter::readBE16(File& file, uint16_t& out) {
    uint16_t val = 0;
    for (int i = 0; i < 2; ++i) {
        uint8_t byte = 0;
        if (!readU8(file, byte)) return false;
        val = (val << 8) | byte;
    }
    out = val;
    return true;
}

SynthPattern& MidiImporter::getSynthPattern(int synthIdx, int patternIdx) {
    static constexpr int kPatternsPerPage = kBankCount * Bank<SynthPattern>::kPatterns; // 16
    int safePattern = (patternIdx < 0) ? 0 : patternIdx;
    int pageIdx = safePattern / kPatternsPerPage;
    int localIdx = safePattern % kPatternsPerPage;

    loadPageToCache(pageIdx);

    int bankIdx = localIdx / Bank<SynthPattern>::kPatterns;
    int slotIdx = localIdx % Bank<SynthPattern>::kPatterns;
    auto& scene = engine_.sceneManager().currentScene();
    return (synthIdx == 0)
        ? scene.synthABanks[bankIdx].patterns[slotIdx]
        : scene.synthBBanks[bankIdx].patterns[slotIdx];
}

DrumPatternSet& MidiImporter::getDrumPatternSet(int patternIdx) {
    static constexpr int kPatternsPerPage = kBankCount * Bank<SynthPattern>::kPatterns; // 16
    int safePattern = (patternIdx < 0) ? 0 : patternIdx;
    int pageIdx = safePattern / kPatternsPerPage;
    int localIdx = safePattern % kPatternsPerPage;

    loadPageToCache(pageIdx);

    int bankIdx = localIdx / Bank<SynthPattern>::kPatterns;
    int slotIdx = localIdx % Bank<SynthPattern>::kPatterns;
    auto& scene = engine_.sceneManager().currentScene();
    return scene.drumBanks[bankIdx].patterns[slotIdx];
}

bool MidiImporter::loadPageToCache(int pageIndex) {
    if (pageIndex == cachedPageIndex_) return true;
    auto& sceneManager = engine_.sceneManager();
    sceneManager.setPage(pageIndex); // Saves previous page and loads requested one.
    cachedPageIndex_ = sceneManager.currentPageIndex();
    return (cachedPageIndex_ == pageIndex);
}

bool MidiImporter::saveCacheToPage() {
    if (cachedPageIndex_ < 0) return true;
    return engine_.sceneManager().saveCurrentPage();
}
