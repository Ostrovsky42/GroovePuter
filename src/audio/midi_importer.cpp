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
    if (loudMode) return static_cast<uint8_t>(40 + ((v * 80) / 127)); // ~40..120
    return static_cast<uint8_t>(30 + ((v * 70) / 127));               // ~30..100
}

static inline uint8_t normalizeDrumVelocity(uint8_t midiVelocity, bool loudMode) {
    int v = clampInt(static_cast<int>(midiVelocity), 1, 127);
    if (loudMode) return static_cast<uint8_t>(40 + ((v * 85) / 127)); // ~40..125
    return static_cast<uint8_t>(30 + ((v * 70) / 127));               // ~30..100
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

// ---------------------------------------------------------------------------
// scanFile — lightweight per-channel statistics without importing
// ---------------------------------------------------------------------------
MidiImporter::ScanResult MidiImporter::scanFile(const std::string& path) {
    ScanResult result;
#ifdef ARDUINO
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) return result;

    char magic[4];
    if (file.readBytes(magic, 4) != 4 || memcmp(magic, "MThd", 4) != 0) {
        file.close(); return result;
    }
    uint32_t headerSize = 0;
    if (!readBE32(file, headerSize)) { file.close(); return result; }
    if (headerSize < 6) { file.close(); return result; }
    if (!readBE16(file, result.format))    { file.close(); return result; }
    if (!readBE16(file, result.numTracks)) { file.close(); return result; }
    if (!readBE16(file, result.division))  { file.close(); return result; }
    if (result.division == 0 || (result.division & 0x8000)) { file.close(); return result; }
    if (headerSize > 6) {
        file.seek(file.position() + (headerSize - 6));
    }

    uint32_t fileSize = file.size();
    uint32_t maxAbsoluteTicks = 0;
    int currentTrackChannel = -1; // for track name→channel mapping

    int tracksParsed = 0;
    while (file.position() + 8 <= fileSize) {
        if (file.readBytes(magic, 4) != 4) break;
        uint32_t chunkSize = 0;
        if (!readBE32(file, chunkSize)) break;
        uint32_t chunkEnd = file.position() + chunkSize;
        if (chunkEnd > fileSize) break;

        if (memcmp(magic, "MTrk", 4) != 0) {
            file.seek(chunkEnd);
            continue;
        }
        tracksParsed++;
        if (tracksParsed > 128) break;

        uint32_t absoluteTicks = 0;
        uint8_t lastStatus = 0;
        currentTrackChannel = -1;

        while (file.position() < chunkEnd) {
            uint32_t bytesRead = 0;
            uint32_t deltaTime = 0;
            if (!readVarLen(file, deltaTime, bytesRead)) break;
            absoluteTicks += deltaTime;

            int status = file.read();
            if (status < 0) break;

            if (!(status & 0x80)) {
                if (lastStatus < 0x80 || lastStatus >= 0xF0) break;
                file.seek(file.position() - 1);
                status = lastStatus;
            } else {
                if (status < 0xF0) lastStatus = (uint8_t)status;
            }

            uint8_t msgType = status & 0xF0;
            uint8_t channel = status & 0x0F; // 0-indexed

            if (msgType == 0x80 || msgType == 0x90) {
                uint8_t note = 0, velocity = 0;
                if (!readU8(file, note)) break;
                if (!readU8(file, velocity)) break;
                if (msgType == 0x90 && velocity > 0) {
                    auto& ci = result.channels[channel];
                    ci.noteCount++;
                    if (note < ci.minNote) ci.minNote = note;
                    if (note > ci.maxNote) ci.maxNote = note;
                    result.totalNotes++;
                    if (absoluteTicks > maxAbsoluteTicks) maxAbsoluteTicks = absoluteTicks;
                    if (currentTrackChannel < 0) currentTrackChannel = channel;
                }
            } else if (msgType == 0xA0 || msgType == 0xB0 || msgType == 0xE0) {
                uint8_t tmp; readU8(file, tmp); readU8(file, tmp);
            } else if (msgType == 0xC0 || msgType == 0xD0) {
                uint8_t tmp; readU8(file, tmp);
            } else if (status == 0xFF) {
                uint8_t metaType = 0;
                if (!readU8(file, metaType)) break;
                uint32_t metaLen = 0;
                if (!readVarLen(file, metaLen, bytesRead)) break;
                if (metaType == 0x03 && metaLen > 0) {
                    // Track Name — read up to 15 chars
                    int tgtCh = (currentTrackChannel >= 0) ? currentTrackChannel : (tracksParsed - 1);
                    if (tgtCh >= 0 && tgtCh < 16 && result.channels[tgtCh].trackName[0] == '\0') {
                        int readLen = (metaLen < 15) ? metaLen : 15;
                        file.readBytes(result.channels[tgtCh].trackName, readLen);
                        result.channels[tgtCh].trackName[readLen] = '\0';
                        if (metaLen > (uint32_t)readLen) file.seek(file.position() + (metaLen - readLen));
                    } else {
                        file.seek(file.position() + metaLen);
                    }
                } else if (metaType == 0x2F) { // End of Track
                    file.seek(file.position() + metaLen);
                    file.seek(chunkEnd);
                    break;
                } else {
                    file.seek(file.position() + metaLen);
                }
            } else if (status == 0xF0 || status == 0xF7) {
                uint32_t sysexLen = 0;
                if (!readVarLen(file, sysexLen, bytesRead)) break;
                file.seek(file.position() + sysexLen);
            } else if (status == 0xF1 || status == 0xF3) {
                uint8_t tmp; readU8(file, tmp);
            } else if (status == 0xF2) {
                uint8_t tmp; readU8(file, tmp); readU8(file, tmp);
            }
            // else: system realtime, no payload
        }
        if (file.position() != chunkEnd) file.seek(chunkEnd);
    }
    file.close();

    // Compute summary
    for (int i = 0; i < 16; i++) {
        if (result.channels[i].used()) result.usedChannels++;
    }
    if (result.division > 0 && maxAbsoluteTicks > 0) {
        uint32_t ticksPerBar = result.division * 4; // 4/4 assumed
        result.estimatedBars = (int)((maxAbsoluteTicks + ticksPerBar - 1) / ticksPerBar);
    }
    result.valid = (tracksParsed > 0 && result.totalNotes > 0);
#endif
    return result;
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
                    int targetDest = -1;
                    if (channel == settings.synthAChannel) targetDest = settings.destSynthA;
                    else if (channel == settings.synthBChannel) targetDest = settings.destSynthB;
                    else if (channel == settings.drumChannel) targetDest = settings.destDrums;
                    else if (settings.omni) targetDest = settings.destSynthA;

                    if (targetDest < 0) continue;
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
                            clearTo = clearFrom + ((sourceLengthSteps + 15) / 16);
                            if (clearTo < clearFrom) clearTo = clearFrom;
                            if (clearTo > kMaxPatterns) clearTo = kMaxPatterns;
                        }
                        for (int p = clearFrom; p < clearTo; ++p) {
                            if (settings.destSynthA == 0 || settings.destSynthB == 0 || settings.destDrums == 0) {
                                SynthPattern& synthA = getSynthPattern(0, p);
                                for (int i = 0; i < SynthPattern::kSteps; ++i) {
                                    synthA.steps[i] = SynthStep{};
                                    synthA.steps[i].note = -1;
                                }
                            }
                            if (settings.destSynthA == 1 || settings.destSynthB == 1 || settings.destDrums == 1) {
                                SynthPattern& synthB = getSynthPattern(1, p);
                                for (int i = 0; i < SynthPattern::kSteps; ++i) {
                                    synthB.steps[i] = SynthStep{};
                                    synthB.steps[i].note = -1;
                                }
                            }
                            if (settings.destSynthA == 2 || settings.destSynthB == 2 || settings.destDrums == 2) {
                                DrumPatternSet& drums = getDrumPatternSet(p);
                                for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
                                    for (int i = 0; i < DrumPattern::kSteps; ++i) {
                                        drums.voices[v].steps[i] = DrumStep{};
                                    }
                                }
                            }
                        }
                        importRegionCleared = true;
                    }

                        if (targetDest == 0) {
                            SynthPattern& pat = getSynthPattern(0, patternIdx);
                            if (!settings.overwrite && pat.steps[stepInPattern].note >= 0) {
                                // Keep existing note
                            } else {
                                int safeNote = static_cast<int>(note);
                                while (safeNote < MiniAcid::kMin303Note) safeNote += 12;
                                while (safeNote > MiniAcid::kMax303Note) safeNote -= 12;
                                if (safeNote < MiniAcid::kMin303Note) safeNote = MiniAcid::kMin303Note; // Safety fallback

                                uint8_t safeVel = normalizeSynthVelocity(velocity, settings.loudMode);
                                pat.steps[stepInPattern].note = static_cast<int8_t>(safeNote);
                                pat.steps[stepInPattern].accent = false;
                                pat.steps[stepInPattern].velocity = safeVel;
                                notesImported++;
                                if (patternIdx > lastImportedPatternIdx_) lastImportedPatternIdx_ = patternIdx;
                            }
                        } else if (targetDest == 1) {
                            SynthPattern& pat = getSynthPattern(1, patternIdx);
                            if (!settings.overwrite && pat.steps[stepInPattern].note >= 0) {
                                // Keep existing note
                            } else {
                                int safeNote = static_cast<int>(note);
                                while (safeNote < MiniAcid::kMin303Note) safeNote += 12;
                                while (safeNote > MiniAcid::kMax303Note) safeNote -= 12;
                                if (safeNote < MiniAcid::kMin303Note) safeNote = MiniAcid::kMin303Note; // Safety fallback

                                uint8_t safeVel = normalizeSynthVelocity(velocity, settings.loudMode);
                                pat.steps[stepInPattern].note = static_cast<int8_t>(safeNote);
                                pat.steps[stepInPattern].accent = false;
                                pat.steps[stepInPattern].velocity = safeVel;
                                notesImported++;
                                if (patternIdx > lastImportedPatternIdx_) lastImportedPatternIdx_ = patternIdx;
                            }
                        } else if (targetDest == 2) {
                         int drumVoice = -1;
                         switch(note) {
                             case 35: case 36: drumVoice = 0; break; // Kick
                             case 38: case 40: drumVoice = 1; break; // Snare
                             case 42: case 44: drumVoice = 2; break; // CHat
                             case 46: drumVoice = 3; break;           // OHat
                             case 49: case 57: drumVoice = 3; break;  // Crash (mapped to OHat)
                             case 51: case 53: case 59: drumVoice = 3; break; // Ride (mapped to OHat)
                             case 54:          drumVoice = 2; break;  // Tambourine (mapped to CHat)
                             case 56:          drumVoice = 6; break;  // Cowbell (mapped to Rim)
                             case 41: case 43: case 50: drumVoice = 4; break; // MTom
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
