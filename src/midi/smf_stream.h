#pragma once

#include <cstddef>
#include <cstdint>

#include "smf_document.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfMaxTracks = 64;
constexpr std::size_t kSmfTrackReadCacheBytes = 64;

class ISmfByteSource {
public:
    virtual ~ISmfByteSource() = default;
    virtual uint32_t size() const = 0;
    virtual bool readAt(uint32_t offset, uint8_t* dst, std::size_t length) = 0;
};

struct SmfTrackSpan {
    uint32_t offset{0};
    uint32_t length{0};
};

struct SmfFileIndex {
    uint16_t format{0};
    uint16_t division{0};
    uint16_t trackCount{0};
    SmfTrackSpan tracks[kSmfMaxTracks]{};
};

struct SmfIndexResult {
    SmfFileIndex index{};
    SmfParseError error{SmfParseError::None};

    bool ok() const { return error == SmfParseError::None; }
};

class SmfFileIndexer {
public:
    static SmfIndexResult build(ISmfByteSource& source);
};

struct SmfStreamEvent {
    SmfEvent event{};
    uint16_t trackIndex{0};
    uint32_t ordinal{0};
};

class SmfTrackStream {
public:
    bool open(ISmfByteSource& source, SmfTrackSpan span, uint16_t trackIndex);
    void reset();
    bool next(SmfStreamEvent& out);
    bool ended() const { return ended_; }

private:
    bool readByte(uint8_t& value);
    bool readVarLen(uint32_t& value);
    bool skip(uint32_t count);
    bool emitChannelEvent(uint8_t status,
                          bool hasFirstData,
                          uint8_t firstData,
                          SmfStreamEvent& out);

    ISmfByteSource* source_{nullptr};
    SmfTrackSpan span_{};
    uint16_t trackIndex_{0};
    uint32_t pos_{0};
    uint32_t tick_{0};
    uint32_t ordinal_{0};
    uint8_t runningStatus_{0};
    bool ended_{true};

    uint32_t cacheStart_{0};
    uint8_t cacheLen_{0};
    uint8_t cache_[kSmfTrackReadCacheBytes]{};
};

class SmfEventStreamMerger {
public:
    bool open(ISmfByteSource& source, const SmfFileIndex& index);
    void reset();
    bool next(SmfStreamEvent& out);
    bool peek(SmfStreamEvent& out) const;
    bool ended() const;

private:
    bool prime(std::size_t track);
    int selectedTrack() const;

    ISmfByteSource* source_{nullptr};
    SmfFileIndex index_{};
    SmfTrackStream streams_[kSmfMaxTracks]{};
    SmfStreamEvent next_[kSmfMaxTracks]{};
    bool hasNext_[kSmfMaxTracks]{};
};

}  // namespace GroovePuterMidi
