#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GroovePuterMidi {

enum class SmfEventKind : uint8_t {
    NoteOn = 0,
    NoteOff,
    Tempo,
    TimeSignature,
    ProgramChange,
};

struct SmfEvent {
    uint32_t tick{0};
    uint32_t sequence{0};
    SmfEventKind kind{SmfEventKind::NoteOn};
    uint8_t channel{0};
    uint8_t data1{0};
    uint8_t data2{0};
    uint32_t value{0};
};

struct SmfTrackInfo {
    std::string name;
    uint32_t noteCount{0};
};

struct SmfDocument {
    uint16_t format{0};
    uint16_t division{0};
    std::vector<SmfEvent> events;
    std::vector<SmfTrackInfo> tracks;
    uint32_t musicStartTick{0};
    uint32_t endTick{0};

    bool empty() const { return events.empty(); }
};

enum class SmfParseError : uint8_t {
    None = 0,
    InvalidHeader,
    UnsupportedFormat,
    UnsupportedDivision,
    Truncated,
    InvalidTrack,
    InvalidEvent,
};

struct SmfParseResult {
    SmfDocument document;
    SmfParseError error{SmfParseError::None};

    bool ok() const { return error == SmfParseError::None; }
};

class SmfParser {
public:
    static SmfParseResult parse(const uint8_t* data, std::size_t size);
    static const char* errorString(SmfParseError error);
};

enum class SmfRestartOrigin : uint8_t {
    MusicStart = 0,
    FileStart,
};

enum class SmfPlaybackState : uint8_t {
    Stopped = 0,
    Playing,
    Paused,
};

class SmfTimelineCursor {
public:
    void load(const SmfDocument* document);
    void unload();

    bool loaded() const { return document_ != nullptr; }
    SmfPlaybackState state() const { return state_; }
    uint32_t tick() const { return tick_; }
    std::size_t eventIndex() const { return eventIndex_; }

    void play();
    void pause();
    void stop();
    void restart(SmfRestartOrigin origin);
    void seekTick(uint32_t tick);

    const SmfEvent* peek() const;
    const SmfEvent* pop();

private:
    void positionAt(uint32_t tick);

    const SmfDocument* document_{nullptr};
    SmfPlaybackState state_{SmfPlaybackState::Stopped};
    uint32_t tick_{0};
    std::size_t eventIndex_{0};
};

}  // namespace GroovePuterMidi
