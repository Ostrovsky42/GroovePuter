#pragma once

#include <cstdint>

namespace GroovePuterMidi {

enum class SmfPlayerState : uint8_t {
    Unloaded = 0,
    Loading,
    Stopped,
    Armed,
    Playing,
    Paused,
    Error,
};

enum class SmfPlayerRestartOrigin : uint8_t {
    MusicStart = 0,
    FileStart,
};

enum class SmfTempoMode : uint8_t {
    Original = 0,
    Project,
};

enum class SmfLaunchMode : uint8_t {
    Immediate = 0,
    NextBar,
};

struct SmfPlayerPerformanceSnapshot {
    uint16_t trackCount{0};
    uint16_t cacheBytesPerTrack{0};
    uint32_t reads{0};
    uint32_t seeks{0};
    uint32_t bytes{0};
    uint32_t maxReadMicros{0};
    uint32_t scheduleCalls{0};
    uint32_t queuedEvents{0};
    uint32_t maxScheduleMicros{0};
    int16_t minQueueDepth{-1};
    uint16_t queueFillLimit{0};
    uint16_t lookaheadMs{0};
};

struct SmfPlayerSnapshot {
    SmfPlayerState state{SmfPlayerState::Unloaded};
    char filename[48]{};
    char message[48]{};
    uint32_t currentTick{0};
    uint32_t endTick{0};
    uint32_t bar{1};
    uint16_t beat{1};
    uint32_t totalBars{1};
    uint16_t originalBpmX10{1200};
    uint16_t bpmX10{1200};
    uint16_t tempoScalePermille{1000};
    uint8_t velocityBoost{0};
    bool rawRouting{true};
    SmfTempoMode tempoMode{SmfTempoMode::Original};
    SmfLaunchMode launchMode{SmfLaunchMode::NextBar};
    SmfPlayerPerformanceSnapshot performance{};
};

class ISmfPlayerService {
public:
    virtual ~ISmfPlayerService() = default;

    // All commands are non-blocking from the UI perspective. File I/O and
    // scanning belong to the platform player task, never the display handler.
    virtual bool requestLoadAndPlay(const char* path) = 0;
    virtual bool togglePlayPause() = 0;
    virtual bool restart(SmfPlayerRestartOrigin origin =
                         SmfPlayerRestartOrigin::MusicStart) = 0;
    virtual bool stop() = 0;
    virtual bool panic() = 0;
    virtual bool seekBars(int deltaBars) = 0;
    virtual bool toggleRouting() = 0;
    virtual bool toggleTempoMode() = 0;
    virtual bool adjustTempoBpm(int deltaBpm) = 0;
    virtual bool resetTempo() = 0;
    virtual bool cycleVelocityBoost() = 0;
    virtual SmfPlayerSnapshot snapshot() const = 0;
};

// Tiny service registry keeps UI pages platform-neutral. Cardputer registers
// its implementation without touching hardware; SDL simply leaves this null.
inline ISmfPlayerService*& smfPlayerServiceSlot() {
    static ISmfPlayerService* service = nullptr;
    return service;
}

inline void registerSmfPlayerService(ISmfPlayerService* service) {
    smfPlayerServiceSlot() = service;
}

inline ISmfPlayerService* smfPlayerService() {
    return smfPlayerServiceSlot();
}

inline const char* smfPlayerStateName(SmfPlayerState state) {
    switch (state) {
        case SmfPlayerState::Unloaded: return "NO FILE";
        case SmfPlayerState::Loading: return "LOADING";
        case SmfPlayerState::Stopped: return "STOPPED";
        case SmfPlayerState::Armed: return "ARMED";
        case SmfPlayerState::Playing: return "PLAYING";
        case SmfPlayerState::Paused: return "PAUSED";
        case SmfPlayerState::Error: return "ERROR";
    }
    return "?";
}

inline const char* smfTempoModeName(SmfTempoMode mode) {
    switch (mode) {
        case SmfTempoMode::Original: return "ORIGINAL";
        case SmfTempoMode::Project: return "GP MASTER";
    }
    return "?";
}

inline const char* smfLaunchModeName(SmfLaunchMode mode) {
    switch (mode) {
        case SmfLaunchMode::Immediate: return "NOW";
        case SmfLaunchMode::NextBar: return "NEXT BAR";
    }
    return "?";
}

}  // namespace GroovePuterMidi
