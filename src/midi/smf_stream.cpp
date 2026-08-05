#include "smf_stream.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "smf_session_generation.h"
#include "smf_structural_inspector.h"
#include "smf_track_inspector.h"
#include "smf_track_mute.h"

namespace GroovePuterMidi {
namespace {

uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) << 8 |
           static_cast<uint16_t>(p[1]);
}

uint32_t be32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 |
           static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 |
           static_cast<uint32_t>(p[3]);
}

bool isTag(const uint8_t* p, const char* tag) {
    return p[0] == static_cast<uint8_t>(tag[0]) &&
           p[1] == static_cast<uint8_t>(tag[1]) &&
           p[2] == static_cast<uint8_t>(tag[2]) &&
           p[3] == static_cast<uint8_t>(tag[3]);
}

}  // namespace

SmfIndexResult SmfFileIndexer::build(ISmfByteSource& source) {
    SmfIndexResult result;
    if (source.size() < 14) {
        result.error = SmfParseError::InvalidHeader;
        return result;
    }

    uint8_t header[14]{};
    if (!source.readAt(0, header, sizeof(header)) || !isTag(header, "MThd")) {
        result.error = SmfParseError::InvalidHeader;
        return result;
    }

    const uint32_t headerLength = be32(header + 4);
    if (headerLength < 6) {
        result.error = SmfParseError::InvalidHeader;
        return result;
    }

    result.index.format = be16(header + 8);
    const uint16_t declaredTracks = be16(header + 10);
    result.index.division = be16(header + 12);

    if (result.index.format > 1 || declaredTracks == 0) {
        result.error = SmfParseError::UnsupportedFormat;
        return result;
    }
    if ((result.index.division & 0x8000u) != 0 || result.index.division == 0) {
        result.error = SmfParseError::UnsupportedDivision;
        return result;
    }

    const uint64_t firstChunk = 8ull + headerLength;
    if (firstChunk > source.size()) {
        result.error = SmfParseError::Truncated;
        return result;
    }

    uint32_t offset = static_cast<uint32_t>(firstChunk);
    uint16_t foundTracks = 0;
    uint16_t retainedTracks = 0;
    while (foundTracks < declaredTracks) {
        if (offset > source.size() || source.size() - offset < 8) {
            result.error = SmfParseError::InvalidTrack;
            return result;
        }

        uint8_t chunkHeader[8]{};
        if (!source.readAt(offset, chunkHeader, sizeof(chunkHeader))) {
            result.error = SmfParseError::Truncated;
            return result;
        }
        const uint32_t chunkLength = be32(chunkHeader + 4);
        const uint64_t dataOffset = static_cast<uint64_t>(offset) + 8ull;
        const uint64_t chunkEnd = dataOffset + chunkLength;
        if (chunkEnd > source.size()) {
            result.error = SmfParseError::Truncated;
            return result;
        }

        if (isTag(chunkHeader, "MTrk")) {
            if (retainedTracks < kSmfMaxTracks) {
                result.index.tracks[retainedTracks++] = SmfTrackSpan{
                    static_cast<uint32_t>(dataOffset),
                    chunkLength,
                };
            }
            ++foundTracks;
        }
        offset = static_cast<uint32_t>(chunkEnd);
    }

    result.index.trackCount = retainedTracks;
    result.index.declaredTrackCount = foundTracks;
    result.error = SmfParseError::None;
    return result;
}

bool SmfTrackStream::open(ISmfByteSource& source,
                          SmfTrackSpan span,
                          uint16_t trackIndex) {
    const uint64_t end = static_cast<uint64_t>(span.offset) + span.length;
    if (end > source.size()) return false;
    source_ = &source;
    span_ = span;
    trackIndex_ = trackIndex;
    reset();
    return true;
}

void SmfTrackStream::setCache(uint8_t* buffer, uint32_t capacity) {
    cache_ = capacity > 0 ? buffer : nullptr;
    cacheCapacity_ = cache_ != nullptr ? capacity : 0;
    cacheStart_ = 0;
    cacheLen_ = 0;
}

void SmfTrackStream::reset() {
    pos_ = span_.offset;
    tick_ = 0;
    ordinal_ = 0;
    runningStatus_ = 0;
    ended_ = source_ == nullptr || span_.length == 0;
    cacheStart_ = 0;
    cacheLen_ = 0;
}

bool SmfTrackStream::readByte(uint8_t& value) {
    if (!source_ || ended_) return false;
    const uint32_t trackEnd = span_.offset + span_.length;
    if (pos_ >= trackEnd) return false;

    if (cache_ == nullptr) {
        if (!source_->readAt(pos_, &value, 1)) return false;
        ++pos_;
        return true;
    }

    if (cacheLen_ == 0 || pos_ < cacheStart_ ||
        pos_ >= cacheStart_ + cacheLen_) {
        uint32_t start = pos_;
        if (cacheCapacity_ >= kSmfSectorBytes) {
            const uint32_t aligned =
                pos_ & ~static_cast<uint32_t>(kSmfSectorBytes - 1u);
            start = std::max(aligned, span_.offset);
        }
        const uint32_t want = std::min<uint32_t>(cacheCapacity_, trackEnd - start);
        if (want == 0 || !source_->readAt(start, cache_, want)) return false;
        cacheStart_ = start;
        cacheLen_ = want;
    }

    value = cache_[pos_ - cacheStart_];
    ++pos_;
    return true;
}

bool SmfTrackStream::readVarLen(uint32_t& value) {
    value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = 0;
        if (!readByte(byte)) return false;
        value = (value << 7) | static_cast<uint32_t>(byte & 0x7Fu);
        if ((byte & 0x80u) == 0) return true;
    }
    return false;
}

bool SmfTrackStream::skip(uint32_t count) {
    const uint32_t trackEnd = span_.offset + span_.length;
    if (count > trackEnd - pos_) return false;
    pos_ += count;
    return true;
}

bool SmfTrackStream::emitChannelEvent(uint8_t status,
                                      bool hasFirstData,
                                      uint8_t firstData,
                                      SmfStreamEvent& out) {
    const uint8_t type = status & 0xF0u;
    const uint8_t channel = status & 0x0Fu;
    const bool oneDataByte = type == 0xC0u || type == 0xD0u;
    uint8_t data1 = 0;
    uint8_t data2 = 0;

    if (hasFirstData) data1 = firstData;
    else if (!readByte(data1)) return false;
    if (!oneDataByte && !readByte(data2)) return false;

    SmfEventKind kind = SmfEventKind::NoteOn;
    bool emit = true;
    if (type == 0x90u && data2 > 0) {
        kind = SmfEventKind::NoteOn;
    } else if (type == 0x80u || (type == 0x90u && data2 == 0)) {
        kind = SmfEventKind::NoteOff;
    } else if (type == 0xC0u) {
        kind = SmfEventKind::ProgramChange;
    } else {
        emit = false;
    }

    if (!emit) return true;
    out.event = SmfEvent{tick_, ordinal_, kind, channel, data1, data2, 0};
    out.trackIndex = trackIndex_;
    out.ordinal = ordinal_++;
    return true;
}

bool SmfTrackStream::next(SmfStreamEvent& out) {
    if (!source_ || ended_) return false;
    const uint32_t trackEnd = span_.offset + span_.length;

    while (pos_ < trackEnd) {
        uint32_t delta = 0;
        if (!readVarLen(delta) ||
            delta > std::numeric_limits<uint32_t>::max() - tick_) {
            ended_ = true;
            return false;
        }
        tick_ += delta;

        uint8_t first = 0;
        if (!readByte(first)) {
            ended_ = true;
            return false;
        }

        uint8_t status = first;
        bool hasFirstData = false;
        uint8_t firstData = 0;
        if ((first & 0x80u) == 0) {
            if (runningStatus_ < 0x80u || runningStatus_ >= 0xF0u) {
                ended_ = true;
                return false;
            }
            status = runningStatus_;
            hasFirstData = true;
            firstData = first;
        } else if (status < 0xF0u) {
            runningStatus_ = status;
        } else {
            runningStatus_ = 0;
        }

        if (status >= 0x80u && status <= 0xEFu) {
            const uint32_t ordinalBefore = ordinal_;
            if (!emitChannelEvent(status, hasFirstData, firstData, out)) {
                ended_ = true;
                return false;
            }
            if (ordinal_ != ordinalBefore) return true;
            continue;
        }

        if (status == 0xFFu) {
            uint8_t metaType = 0;
            uint32_t length = 0;
            if (!readByte(metaType) || !readVarLen(length) ||
                length > trackEnd - pos_) {
                ended_ = true;
                return false;
            }

            if (metaType == 0x2Fu) {
                if (!skip(length)) {
                    ended_ = true;
                    return false;
                }
                ended_ = true;
                return false;
            }
            if (metaType == 0x03u) {
                char name[kSmfTrackNameBytes]{};
                const uint32_t copyLength = std::min<uint32_t>(
                    length, static_cast<uint32_t>(kSmfTrackNameBytes - 1u));
                for (uint32_t i = 0; i < copyLength; ++i) {
                    uint8_t value = 0;
                    if (!readByte(value)) {
                        ended_ = true;
                        return false;
                    }
                    name[i] = static_cast<char>(value);
                }
                if (!skip(length - copyLength)) {
                    ended_ = true;
                    return false;
                }
                smfTrackInspectorState().setName(trackIndex_, name);
                continue;
            }
            if (metaType == 0x51u && length == 3) {
                uint8_t b0 = 0, b1 = 0, b2 = 0;
                if (!readByte(b0) || !readByte(b1) || !readByte(b2)) {
                    ended_ = true;
                    return false;
                }
                out.event = SmfEvent{
                    tick_, ordinal_, SmfEventKind::Tempo, 0, 0, 0,
                    static_cast<uint32_t>(b0) << 16 |
                    static_cast<uint32_t>(b1) << 8 |
                    static_cast<uint32_t>(b2),
                };
                out.trackIndex = trackIndex_;
                out.ordinal = ordinal_++;
                return true;
            }
            if (metaType == 0x58u && length >= 2) {
                uint8_t numerator = 0;
                uint8_t denominatorPow2 = 0;
                if (!readByte(numerator) || !readByte(denominatorPow2) ||
                    !skip(length - 2)) {
                    ended_ = true;
                    return false;
                }
                out.event = SmfEvent{
                    tick_, ordinal_, SmfEventKind::TimeSignature,
                    0, numerator, denominatorPow2, 0,
                };
                out.trackIndex = trackIndex_;
                out.ordinal = ordinal_++;
                return true;
            }
            if (!skip(length)) {
                ended_ = true;
                return false;
            }
            continue;
        }

        if (status == 0xF0u || status == 0xF7u) {
            uint32_t length = 0;
            if (!readVarLen(length) || length > trackEnd - pos_ || !skip(length)) {
                ended_ = true;
                return false;
            }
            continue;
        }

        ended_ = true;
        return false;
    }

    ended_ = true;
    return false;
}

bool SmfEventStreamMerger::open(ISmfByteSource& source,
                                const SmfFileIndex& index) {
    const uint32_t generation = smfBeginSessionOpen();

    auto invalidate = [this]() {
        source_ = nullptr;
        index_ = SmfFileIndex{};
        smfTrackMuteState().reset(0);
        smfTrackInspectorState().reset(0, 0);
        smfStructuralInspectorState().reset(0, 0);
        captureStructuralAnalysis_ = false;
        trackCacheBytes_ = 0u;
        selected_ = -1;
        selectedValid_ = false;
        for (std::size_t i = 0; i < kSmfMaxTracks; ++i) hasNext_[i] = false;
    };

    invalidate();
    if (index.trackCount == 0 || index.trackCount > kSmfMaxTracks) return false;

    source_ = &source;
    index_ = index;
    smfTrackMuteState().reset(index.trackCount);
    smfTrackInspectorState().reset(index.trackCount, index.declaredTrackCount);
    smfStructuralInspectorState().reset(index.division, index.trackCount);
    captureStructuralAnalysis_ = true;

    // Split the shared pool across the tracks this file actually uses. Slices
    // are sector-aligned whenever they are at least one sector wide.
    uint32_t perTrack = static_cast<uint32_t>(
        std::min<std::size_t>(kSmfTrackReadCacheBytes,
                              kSmfStreamCacheBytes / index_.trackCount));
    if (perTrack >= kSmfSectorBytes) {
        perTrack = (perTrack / kSmfSectorBytes) * kSmfSectorBytes;
    }
    trackCacheBytes_ = perTrack;

    for (std::size_t i = 0; i < index_.trackCount; ++i) {
        if (!streams_[i].open(source, index_.tracks[i], static_cast<uint16_t>(i))) {
            invalidate();
            return false;
        }
        streams_[i].setCache(cachePool_ + i * perTrack, perTrack);
        prime(i);
    }

    if (!smfCompleteSessionOpen(generation)) {
        invalidate();
        return false;
    }
    return true;
}

void SmfEventStreamMerger::reset() {
    if (!source_) return;
    selectedValid_ = false;
    for (std::size_t i = 0; i < index_.trackCount; ++i) {
        streams_[i].reset();
        prime(i);
    }
}

bool SmfEventStreamMerger::prime(std::size_t track) {
    if (track >= index_.trackCount) return false;
    selectedValid_ = false;
    hasNext_[track] = streams_[track].next(next_[track]);
    return hasNext_[track];
}

int SmfEventStreamMerger::selectedTrack() const {
    if (selectedValid_) return selected_;

    int selected = -1;
    for (std::size_t i = 0; i < index_.trackCount; ++i) {
        if (!hasNext_[i]) continue;
        if (selected < 0) {
            selected = static_cast<int>(i);
            continue;
        }
        const SmfStreamEvent& candidate = next_[i];
        const SmfStreamEvent& current = next_[static_cast<std::size_t>(selected)];
        if (candidate.event.tick < current.event.tick ||
            (candidate.event.tick == current.event.tick &&
             (candidate.trackIndex < current.trackIndex ||
              (candidate.trackIndex == current.trackIndex &&
               candidate.ordinal < current.ordinal)))) {
            selected = static_cast<int>(i);
        }
    }
    selected_ = selected;
    selectedValid_ = true;
    return selected;
}

bool SmfEventStreamMerger::next(SmfStreamEvent& out) {
    while (true) {
        const int selected = selectedTrack();
        if (selected < 0) {
            if (captureStructuralAnalysis_) {
                smfStructuralInspectorState().finalize();
                captureStructuralAnalysis_ = false;
            }
            return false;
        }
        const std::size_t track = static_cast<std::size_t>(selected);
        out = next_[track];
        prime(track);

        smfTrackInspectorState().observe(out.trackIndex, out.event);
        if (captureStructuralAnalysis_) {
            smfStructuralInspectorState().observe(out.trackIndex, out.event);
        }
        const bool noteOn = out.event.kind == SmfEventKind::NoteOn;
        if (shouldEmitSmfTrackEvent(noteOn, out.trackIndex)) return true;
    }
}

bool SmfEventStreamMerger::peek(SmfStreamEvent& out) const {
    const int selected = selectedTrack();
    if (selected < 0) return false;
    out = next_[static_cast<std::size_t>(selected)];
    return true;
}

bool SmfEventStreamMerger::ended() const {
    return selectedTrack() < 0;
}

}  // namespace GroovePuterMidi
