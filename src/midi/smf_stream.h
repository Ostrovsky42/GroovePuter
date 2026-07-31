#pragma once

#include <cstddef>
#include <cstdint>

#include "smf_document.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfMaxTracks = 64;
// SD cards transfer whole sectors, so a read smaller than this still costs a
// full sector fetch plus a FAT layer seek. Windows are aligned and sized in
// sector units whenever the per-track budget allows it.
constexpr std::size_t kSmfSectorBytes = 512;
// The merger owns one shared cache pool and hands each track a slice sized by
// the file's actual track count. A 64-track file still gets the historical
// 64 bytes per track, while the common 2-16 track file gets 256-2048 bytes and
// therefore 4-32x fewer SD reads. Total footprint is unchanged.
constexpr std::size_t kSmfStreamCacheBytes = 4096;
constexpr std::size_t kSmfTrackReadCacheBytes = 2048;

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
    // Without a cache slice the stream still parses correctly, one byte per
    // source read. The merger always assigns a slice before playback.
    void setCache(uint8_t* buffer, uint32_t capacity);
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
    uint32_t cacheLen_{0};
    uint32_t cacheCapacity_{0};
    uint8_t* cache_{nullptr};
};

class SmfEventStreamMerger {
public:
    bool open(ISmfByteSource& source, const SmfFileIndex& index);
    void reset();
    bool next(SmfStreamEvent& out);
    bool peek(SmfStreamEvent& out) const;
    bool ended() const;
    uint32_t trackCacheBytes() const { return trackCacheBytes_; }

private:
    bool prime(std::size_t track);
    int selectedTrack() const;

    ISmfByteSource* source_{nullptr};
    SmfFileIndex index_{};
    SmfTrackStream streams_[kSmfMaxTracks]{};
    SmfStreamEvent next_[kSmfMaxTracks]{};
    bool hasNext_[kSmfMaxTracks]{};
    uint32_t trackCacheBytes_{0};
    // Recomputing the winning track for every peek()/ended()/next() call scans
    // all tracks three times per event; cache it until the winner is consumed.
    mutable int selected_{-1};
    mutable bool selectedValid_{false};
    uint8_t cachePool_[kSmfStreamCacheBytes]{};
};

}  // namespace GroovePuterMidi
