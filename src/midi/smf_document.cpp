#include "smf_document.h"

#include <algorithm>
#include <limits>

namespace GroovePuterMidi {
namespace {

class Reader {
public:
    Reader(const uint8_t* data, std::size_t size)
        : data_(data), size_(size) {}

    std::size_t position() const { return pos_; }
    std::size_t remaining() const { return pos_ <= size_ ? size_ - pos_ : 0; }

    bool seek(std::size_t position) {
        if (position > size_) return false;
        pos_ = position;
        return true;
    }

    bool skip(std::size_t count) {
        if (count > remaining()) return false;
        pos_ += count;
        return true;
    }

    bool readU8(uint8_t& value) {
        if (remaining() < 1) return false;
        value = data_[pos_++];
        return true;
    }

    bool readBE16(uint16_t& value) {
        if (remaining() < 2) return false;
        value = static_cast<uint16_t>(data_[pos_]) << 8 |
                static_cast<uint16_t>(data_[pos_ + 1]);
        pos_ += 2;
        return true;
    }

    bool readBE32(uint32_t& value) {
        if (remaining() < 4) return false;
        value = static_cast<uint32_t>(data_[pos_]) << 24 |
                static_cast<uint32_t>(data_[pos_ + 1]) << 16 |
                static_cast<uint32_t>(data_[pos_ + 2]) << 8 |
                static_cast<uint32_t>(data_[pos_ + 3]);
        pos_ += 4;
        return true;
    }

    bool readVarLen(uint32_t& value) {
        value = 0;
        for (int i = 0; i < 4; ++i) {
            uint8_t byte = 0;
            if (!readU8(byte)) return false;
            value = (value << 7) | static_cast<uint32_t>(byte & 0x7Fu);
            if ((byte & 0x80u) == 0) return true;
        }
        return false;
    }

    const uint8_t* current() const {
        return pos_ < size_ ? data_ + pos_ : nullptr;
    }

private:
    const uint8_t* data_{nullptr};
    std::size_t size_{0};
    std::size_t pos_{0};
};

bool readTag(Reader& reader, const char expected[4]) {
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = 0;
        if (!reader.readU8(byte) || byte != static_cast<uint8_t>(expected[i])) {
            return false;
        }
    }
    return true;
}

bool addTick(uint32_t& tick, uint32_t delta) {
    if (delta > std::numeric_limits<uint32_t>::max() - tick) return false;
    tick += delta;
    return true;
}

void pushEvent(SmfDocument& document,
               uint32_t& sequence,
               uint32_t tick,
               SmfEventKind kind,
               uint8_t channel,
               uint8_t data1,
               uint8_t data2,
               uint32_t value = 0) {
    document.events.push_back(SmfEvent{
        tick,
        sequence++,
        kind,
        channel,
        data1,
        data2,
        value,
    });
}

}  // namespace

SmfParseResult SmfParser::parse(const uint8_t* data, std::size_t size) {
    SmfParseResult result;
    if (!data || size < 14) {
        result.error = SmfParseError::InvalidHeader;
        return result;
    }

    Reader reader(data, size);
    if (!readTag(reader, "MThd")) {
        result.error = SmfParseError::InvalidHeader;
        return result;
    }

    uint32_t headerLength = 0;
    uint16_t trackCount = 0;
    if (!reader.readBE32(headerLength) || headerLength < 6 ||
        !reader.readBE16(result.document.format) ||
        !reader.readBE16(trackCount) ||
        !reader.readBE16(result.document.division)) {
        result.error = SmfParseError::Truncated;
        return result;
    }

    if (result.document.format > 1 || trackCount == 0) {
        result.error = SmfParseError::UnsupportedFormat;
        return result;
    }
    if ((result.document.division & 0x8000u) != 0 || result.document.division == 0) {
        result.error = SmfParseError::UnsupportedDivision;
        return result;
    }
    if (headerLength > 6 && !reader.skip(headerLength - 6)) {
        result.error = SmfParseError::Truncated;
        return result;
    }

    result.document.tracks.reserve(trackCount);
    uint32_t sequence = 0;
    bool foundMusicStart = false;

    for (uint16_t trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        if (!readTag(reader, "MTrk")) {
            result.error = SmfParseError::InvalidTrack;
            return result;
        }

        uint32_t trackLength = 0;
        if (!reader.readBE32(trackLength) || trackLength > reader.remaining()) {
            result.error = SmfParseError::Truncated;
            return result;
        }
        const std::size_t trackEnd = reader.position() + trackLength;

        SmfTrackInfo track;
        uint32_t tick = 0;
        uint8_t runningStatus = 0;
        bool endOfTrack = false;

        while (reader.position() < trackEnd && !endOfTrack) {
            uint32_t delta = 0;
            if (!reader.readVarLen(delta) || !addTick(tick, delta)) {
                result.error = SmfParseError::InvalidEvent;
                return result;
            }

            uint8_t first = 0;
            if (!reader.readU8(first)) {
                result.error = SmfParseError::Truncated;
                return result;
            }

            uint8_t status = first;
            bool hasFirstData = false;
            uint8_t firstData = 0;
            if ((first & 0x80u) == 0) {
                if (runningStatus < 0x80u || runningStatus >= 0xF0u) {
                    result.error = SmfParseError::InvalidEvent;
                    return result;
                }
                status = runningStatus;
                hasFirstData = true;
                firstData = first;
            } else if (status < 0xF0u) {
                runningStatus = status;
            } else {
                runningStatus = 0;
            }

            if (status >= 0x80u && status <= 0xEFu) {
                const uint8_t type = status & 0xF0u;
                const uint8_t channel = status & 0x0Fu;
                const bool oneDataByte = (type == 0xC0u || type == 0xD0u);
                uint8_t data1 = 0;
                uint8_t data2 = 0;

                if (hasFirstData) data1 = firstData;
                else if (!reader.readU8(data1)) {
                    result.error = SmfParseError::Truncated;
                    return result;
                }
                if (!oneDataByte && !reader.readU8(data2)) {
                    result.error = SmfParseError::Truncated;
                    return result;
                }

                if (type == 0x90u && data2 > 0) {
                    pushEvent(result.document, sequence, tick,
                              SmfEventKind::NoteOn, channel, data1, data2);
                    ++track.noteCount;
                    if (!foundMusicStart || tick < result.document.musicStartTick) {
                        result.document.musicStartTick = tick;
                        foundMusicStart = true;
                    }
                } else if (type == 0x80u || (type == 0x90u && data2 == 0)) {
                    pushEvent(result.document, sequence, tick,
                              SmfEventKind::NoteOff, channel, data1, data2);
                } else if (type == 0xC0u) {
                    pushEvent(result.document, sequence, tick,
                              SmfEventKind::ProgramChange, channel, data1, 0);
                }
            } else if (status == 0xFFu) {
                uint8_t metaType = 0;
                uint32_t length = 0;
                if (!reader.readU8(metaType) || !reader.readVarLen(length) ||
                    length > reader.remaining() ||
                    reader.position() + length > trackEnd) {
                    result.error = SmfParseError::Truncated;
                    return result;
                }

                if (metaType == 0x2Fu) {
                    if (!reader.skip(length)) {
                        result.error = SmfParseError::Truncated;
                        return result;
                    }
                    endOfTrack = true;
                } else if (metaType == 0x03u) {
                    const uint8_t* text = reader.current();
                    if (length > 0 && text) {
                        track.name.assign(reinterpret_cast<const char*>(text), length);
                    }
                    if (!reader.skip(length)) {
                        result.error = SmfParseError::Truncated;
                        return result;
                    }
                } else if (metaType == 0x51u && length == 3) {
                    uint8_t b0 = 0, b1 = 0, b2 = 0;
                    if (!reader.readU8(b0) || !reader.readU8(b1) || !reader.readU8(b2)) {
                        result.error = SmfParseError::Truncated;
                        return result;
                    }
                    const uint32_t tempo = static_cast<uint32_t>(b0) << 16 |
                                           static_cast<uint32_t>(b1) << 8 |
                                           static_cast<uint32_t>(b2);
                    pushEvent(result.document, sequence, tick,
                              SmfEventKind::Tempo, 0, 0, 0, tempo);
                } else if (metaType == 0x58u && length >= 2) {
                    uint8_t numerator = 0;
                    uint8_t denominatorPow2 = 0;
                    if (!reader.readU8(numerator) || !reader.readU8(denominatorPow2)) {
                        result.error = SmfParseError::Truncated;
                        return result;
                    }
                    pushEvent(result.document, sequence, tick,
                              SmfEventKind::TimeSignature, 0,
                              numerator, denominatorPow2);
                    if (!reader.skip(length - 2)) {
                        result.error = SmfParseError::Truncated;
                        return result;
                    }
                } else if (!reader.skip(length)) {
                    result.error = SmfParseError::Truncated;
                    return result;
                }
            } else if (status == 0xF0u || status == 0xF7u) {
                uint32_t length = 0;
                if (!reader.readVarLen(length) || length > reader.remaining() ||
                    reader.position() + length > trackEnd || !reader.skip(length)) {
                    result.error = SmfParseError::Truncated;
                    return result;
                }
            } else {
                result.error = SmfParseError::InvalidEvent;
                return result;
            }
        }

        if (reader.position() > trackEnd || !reader.seek(trackEnd)) {
            result.error = SmfParseError::Truncated;
            return result;
        }
        if (tick > result.document.endTick) result.document.endTick = tick;
        result.document.tracks.push_back(std::move(track));
    }

    std::stable_sort(result.document.events.begin(), result.document.events.end(),
                     [](const SmfEvent& a, const SmfEvent& b) {
                         if (a.tick != b.tick) return a.tick < b.tick;
                         return a.sequence < b.sequence;
                     });

    if (!foundMusicStart) result.document.musicStartTick = 0;
    result.error = SmfParseError::None;
    return result;
}

const char* SmfParser::errorString(SmfParseError error) {
    switch (error) {
        case SmfParseError::None: return "OK";
        case SmfParseError::InvalidHeader: return "Invalid MIDI header";
        case SmfParseError::UnsupportedFormat: return "Unsupported MIDI format";
        case SmfParseError::UnsupportedDivision: return "SMPTE division is unsupported";
        case SmfParseError::Truncated: return "Truncated MIDI data";
        case SmfParseError::InvalidTrack: return "Invalid MIDI track";
        case SmfParseError::InvalidEvent: return "Invalid MIDI event";
    }
    return "Unknown MIDI parse error";
}

void SmfTimelineCursor::load(const SmfDocument* document) {
    document_ = document;
    state_ = SmfPlaybackState::Stopped;
    positionAt(0);
}

void SmfTimelineCursor::unload() {
    document_ = nullptr;
    state_ = SmfPlaybackState::Stopped;
    tick_ = 0;
    eventIndex_ = 0;
}

void SmfTimelineCursor::play() {
    if (!document_) return;
    if (eventIndex_ >= document_->events.size()) {
        positionAt(document_->musicStartTick);
    }
    state_ = SmfPlaybackState::Playing;
}

void SmfTimelineCursor::pause() {
    if (state_ == SmfPlaybackState::Playing) {
        state_ = SmfPlaybackState::Paused;
    }
}

void SmfTimelineCursor::stop() {
    if (!document_) return;
    state_ = SmfPlaybackState::Stopped;
    positionAt(0);
}

void SmfTimelineCursor::restart(SmfRestartOrigin origin) {
    if (!document_) return;
    const uint32_t target = origin == SmfRestartOrigin::MusicStart
        ? document_->musicStartTick
        : 0;
    positionAt(target);
    state_ = SmfPlaybackState::Playing;
}

void SmfTimelineCursor::seekTick(uint32_t tick) {
    if (!document_) return;
    if (tick > document_->endTick) tick = document_->endTick;
    positionAt(tick);
}

const SmfEvent* SmfTimelineCursor::peek() const {
    if (!document_ || eventIndex_ >= document_->events.size()) return nullptr;
    return &document_->events[eventIndex_];
}

const SmfEvent* SmfTimelineCursor::pop() {
    if (state_ != SmfPlaybackState::Playing) return nullptr;
    const SmfEvent* event = peek();
    if (!event) return nullptr;
    tick_ = event->tick;
    ++eventIndex_;
    return event;
}

void SmfTimelineCursor::positionAt(uint32_t tick) {
    tick_ = tick;
    eventIndex_ = 0;
    if (!document_) return;

    const auto it = std::lower_bound(
        document_->events.begin(), document_->events.end(), tick,
        [](const SmfEvent& event, uint32_t targetTick) {
            return event.tick < targetTick;
        });
    eventIndex_ = static_cast<std::size_t>(it - document_->events.begin());
}

}  // namespace GroovePuterMidi
