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
static inline uint8_t normalizeSynthVelocity(uint8_t midiVelocity) {
    int v = clampInt(static_cast<int>(midiVelocity), 1, 127);
    return static_cast<uint8_t>(58 + ((v * 42) / 127)); // ~58..100
}

static inline uint8_t normalizeDrumVelocity(uint8_t midiVelocity) {
    int v = clampInt(static_cast<int>(midiVelocity), 1, 127);
    return static_cast<uint8_t>(55 + ((v * 45) / 127)); // ~55..100
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
    static constexpr int kPatternsPerPage = kBankCount * Bank<SynthPattern>::kPatterns; // 16
    static constexpr int kMaxPages = 8;
    static constexpr int kMaxPatterns = kPatternsPerPage * kMaxPages; // 128 song patterns.
    bool clearedPattern[kMaxPatterns] = {};
    bool importRegionCleared = false;
    int firstRoutedStep = -1;

    for (int t = 0; t < header.numTracks; ++t) {
        if (file.readBytes(magic, 4) != 4 || memcmp(magic, "MTrk", 4) != 0) {
            return Error::InvalidFormat;
        }

        uint32_t trackSize = 0;
        if (!readBE32(file, trackSize)) return Error::ReadError;
        uint32_t trackEnd = file.position() + trackSize;
        if (trackEnd > fileSize) return Error::InvalidFormat;

        uint32_t absoluteTicks = 0;
        uint8_t lastStatus = 0;
        uint8_t timeSigNum = 4;
        uint8_t timeSigDenom = 4;

        auto ticksPerStep = [&]() -> double {
            double denom = static_cast<double>(timeSigDenom);
            if (denom <= 0.0) denom = 4.0;
            double barQuarters = static_cast<double>(timeSigNum) * (4.0 / denom);
            double t = static_cast<double>(header.division) * barQuarters / 16.0;
            if (t < 1.0) t = 1.0;
            return t;
        };

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
                if (lastStatus == 0) return Error::InvalidFormat;
                file.seek(file.position() - 1);
                status = lastStatus;
            } else {
                lastStatus = status;
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
                    // Quantize to 1/16 steps
                    // 1 step = 1/16 of a bar based on current time signature
                    double tps = ticksPerStep();
                    int stepIdx = static_cast<int>((static_cast<double>(absoluteTicks) + (tps * 0.5)) / tps);
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

                    if (settings.overwrite && !importRegionCleared) {
                        int clearFrom = settings.targetPatternIndex;
                        if (clearFrom < 0) clearFrom = 0;
                        if (clearFrom > kMaxPatterns) clearFrom = kMaxPatterns;
                        for (int p = clearFrom; p < kMaxPatterns; ++p) {
                            SynthPattern& synthA = getSynthPattern(0, p);
                            SynthPattern& synthB = getSynthPattern(1, p);
                            DrumPatternSet& drums = getDrumPatternSet(p);
                            for (int i = 0; i < SynthPattern::kSteps; ++i) {
                                synthA.steps[i] = SynthStep{};
                                synthB.steps[i] = SynthStep{};
                            }
                            for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
                                for (int i = 0; i < DrumPattern::kSteps; ++i) {
                                    drums.voices[v].steps[i] = DrumStep{};
                                }
                            }
                            clearedPattern[p] = true;
                        }
                        importRegionCleared = true;
                    }

                    int adjustedStep = stepIdx - firstRoutedStep - settings.startStepOffset;
                    if (adjustedStep < 0) {
                        continue;
                    }
                    int patternIdx = adjustedStep / 16 + settings.targetPatternIndex;
                    int stepInPattern = adjustedStep % 16;
                    if (patternIdx < 0 || patternIdx >= kMaxPatterns) {
                        continue;
                    }

                        if (routeToA) {
                            SynthPattern& pat = getSynthPattern(0, patternIdx);
                            if (!settings.overwrite && pat.steps[stepInPattern].note != 0) {
                                // Keep existing note
                            } else {
                                int safeNote = clampInt(static_cast<int>(note), 24, 96);
                                uint8_t safeVel = normalizeSynthVelocity(velocity);
                                pat.steps[stepInPattern].note = safeNote;
                                pat.steps[stepInPattern].accent = (velocity >= 118);
                                pat.steps[stepInPattern].velocity = safeVel;
                                notesImported++;
                            }
                        } else if (routeToB) {
                            SynthPattern& pat = getSynthPattern(1, patternIdx);
                            if (!settings.overwrite && pat.steps[stepInPattern].note != 0) {
                                // Keep existing note
                            } else {
                                int safeNote = clampInt(static_cast<int>(note), 24, 96);
                                uint8_t safeVel = normalizeSynthVelocity(velocity);
                                pat.steps[stepInPattern].note = safeNote;
                                pat.steps[stepInPattern].accent = (velocity >= 118);
                                pat.steps[stepInPattern].velocity = safeVel;
                                notesImported++;
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
                                     uint8_t safeVel = normalizeDrumVelocity(velocity);
                                     step.hit = true;
                                     step.accent = (velocity >= 124);
                                     step.velocity = safeVel;
                                     notesImported++;
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
                    if (metaLen != 0) return Error::InvalidFormat;
                    file.seek(trackEnd);
                    break;
                }
                if (metaType == 0x58) { // Time Signature
                    if (metaLen < 4) return Error::InvalidFormat;
                    uint8_t nn = 0, ddPow = 0, cc = 0, bb = 0;
                    if (!readU8(file, nn)) return Error::ReadError;
                    if (!readU8(file, ddPow)) return Error::ReadError;
                    if (!readU8(file, cc)) return Error::ReadError;
                    if (!readU8(file, bb)) return Error::ReadError;
                    if (ddPow < 8) {
                        timeSigNum = (nn == 0) ? 4 : nn;
                        timeSigDenom = static_cast<uint8_t>(1u << ddPow);
                        if (timeSigDenom == 0) timeSigDenom = 4;
                    }
                    uint32_t remaining = metaLen - 4;
                    uint32_t newPos = file.position() + remaining;
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
            } else {
                return Error::InvalidFormat;
            }
        }
    }

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
