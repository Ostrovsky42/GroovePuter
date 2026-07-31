#pragma once

#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "src/midi/scheduled_smf_midi_event_queue.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/smf_scheduler.h"
#include "src/midi/smf_stream.h"
#include "src/midi/smf_timing.h"

class CardputerSmfPlayerService final : public GroovePuterMidi::ISmfPlayerService {
public:
    CardputerSmfPlayerService();

    bool begin();
    ScheduledSmfMidiEventQueue& eventQueue() { return eventQueue_; }

    bool requestLoadAndPlay(const char* path) override;
    bool togglePlayPause() override;
    bool restart(GroovePuterMidi::SmfPlayerRestartOrigin origin) override;
    bool stop() override;
    bool panic() override;
    bool seekBars(int deltaBars) override;
    GroovePuterMidi::SmfPlayerSnapshot snapshot() const override;

private:
    static constexpr std::size_t kCommandDepth = 4;
    static constexpr std::size_t kPathBytes = 128;
    static constexpr std::size_t kMaxTimingEvents = 256;
    static constexpr uint32_t kScheduleLookaheadBlocks = 16;
    static constexpr std::size_t kQueueFillLimit =
        ScheduledSmfMidiEventQueue::kCapacity - 24;

    enum class CommandType : uint8_t {
        LoadAndPlay,
        TogglePlayPause,
        RestartMusic,
        RestartFile,
        Stop,
        Panic,
        SeekBars,
    };

    struct Command {
        CommandType type{CommandType::TogglePlayPause};
        int32_t value{0};
        char path[kPathBytes]{};
    };

    class SdByteSource final : public GroovePuterMidi::ISmfByteSource {
    public:
        bool open(const char* path);
        void close();
        uint32_t size() const override;
        bool readAt(uint32_t offset, uint8_t* dst, std::size_t length) override;
        bool valid() const { return static_cast<bool>(file_); }

    private:
        File file_;
    };

    static void taskEntry(void* context);
    void taskLoop();
    bool enqueue(const Command& command);
    void handleCommand(const Command& command);

    bool loadFile(const char* path);
    bool scanMetadata();
    bool prepareStreamAt(uint32_t tick);
    bool startFromTick(uint32_t tick);
    void pauseAtCurrentPosition();
    void stopAndCleanup(bool resetToMusicStart);
    void scheduleAhead();
    void updatePlaybackSnapshot();
    uint32_t currentTickFromAudioClock() const;
    bool takeNextNote(GroovePuterMidi::SmfStreamEvent& event);
    void publishSnapshot(GroovePuterMidi::SmfPlayerState state,
                         const char* message = nullptr);
    static void copyText(char* dst, std::size_t size, const char* src);
    static const char* basename(const char* path);

    ScheduledSmfMidiEventQueue eventQueue_;
    SdByteSource source_;
    GroovePuterMidi::SmfFileIndex fileIndex_{};
    GroovePuterMidi::SmfEventStreamMerger stream_;
    GroovePuterMidi::SmfTimingMap timing_;
    GroovePuterMidi::SmfDocument timingDocument_;

    GroovePuterMidi::SmfStreamEvent pendingEvent_{};
    bool hasPendingEvent_{false};
    bool streamEnded_{true};
    uint32_t musicStartTick_{0};
    uint32_t endTick_{0};
    uint32_t playbackOriginTick_{0};
    uint32_t playbackOriginBlock_{0};
    uint32_t playbackOriginMicros_{0};
    uint32_t lastScheduledBlock_{0};
    uint32_t pausedTick_{0};
    bool loaded_{false};

    mutable portMUX_TYPE snapshotMux_ = portMUX_INITIALIZER_UNLOCKED;
    GroovePuterMidi::SmfPlayerSnapshot snapshot_{};

    StaticQueue_t commandQueueStruct_{};
    alignas(4) uint8_t commandQueueStorage_[kCommandDepth * sizeof(Command)]{};
    QueueHandle_t commandQueue_{nullptr};
    TaskHandle_t taskHandle_{nullptr};
};
