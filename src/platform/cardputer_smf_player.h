#pragma once

#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "src/midi/project_transport_timeline.h"
#include "src/midi/scheduled_smf_midi_event_queue.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/smf_routing.h"
#include "src/midi/smf_scheduler.h"
#include "src/midi/smf_stream.h"
#include "src/midi/smf_timing.h"

class CardputerSmfPlayerService final : public GroovePuterMidi::ISmfPlayerService {
public:
    CardputerSmfPlayerService();

    bool begin();
    ScheduledSmfMidiEventQueue& eventQueue() { return eventQueue_; }

    bool requestLoad(const char* path) override;
    bool togglePlayPause() override;
    bool pause() override;
    bool restart(GroovePuterMidi::SmfPlayerRestartOrigin origin) override;
    bool stop() override;
    bool panic() override;
    bool seekBars(int deltaBars) override;
    bool toggleRouting() override;
    bool toggleTempoMode() override;
    bool adjustTempoBpm(int deltaBpm) override;
    bool resetTempo() override;
    bool cycleVelocityBoost() override;
    GroovePuterMidi::SmfPlayerSnapshot snapshot() const override;

private:
    static constexpr std::size_t kCommandDepth = 4;
    static constexpr std::size_t kPathBytes = 128;
    // Tempo and time-signature metadata is bounded independently from notes.
    // Keeping this small is essential on the DRAM-only Cardputer ADV.
    static constexpr std::size_t kMaxTimingEvents = 32;
    // 32 blocks at 512 frames / 22050 Hz is roughly 740 ms of lookahead. It
    // costs no RAM and absorbs SD latency spikes; dense files remain bounded by
    // kQueueFillLimit rather than by this window.
    static constexpr uint32_t kScheduleLookaheadBlocks = 32;
    static constexpr uint32_t kProjectScheduleLookaheadBlocks = 12;
    static constexpr uint32_t kProjectLaunchLeadBlocks = 2;
    static constexpr uint32_t kProjectTimelineMaxAgeBlocks = 2;
    static constexpr std::size_t kQueueFillLimit =
        ScheduledSmfMidiEventQueue::kCapacity - 24;

    enum class CommandType : uint8_t {
        Load,
        TogglePlayPause,
        Pause,
        RestartMusic,
        RestartFile,
        Stop,
        Panic,
        SeekBars,
        ToggleRouting,
        ToggleTempoMode,
        AdjustTempoBpm,
        ResetTempo,
        CycleVelocityBoost,
    };

    enum class ProjectTransportReadResult : uint8_t {
        Fresh = 0,
        Unavailable,
        Stale,
    };

    struct Command {
        CommandType type{CommandType::TogglePlayPause};
        int32_t value{0};
        char path[kPathBytes]{};
    };

    class SdByteSource final : public GroovePuterMidi::ISmfByteSource {
    public:
        struct Stats {
            uint32_t reads{0};
            uint32_t seeks{0};
            uint32_t bytes{0};
            uint32_t maxReadMicros{0};
        };

        bool open(const char* path);
        void close();
        uint32_t size() const override;
        bool readAt(uint32_t offset, uint8_t* dst, std::size_t length) override;
        bool valid() const { return static_cast<bool>(file_); }
        const Stats& stats() const { return stats_; }
        void resetStats() { stats_ = Stats{}; }

    private:
        static constexpr uint32_t kUnknownPosition = 0xFFFFFFFFu;

        File file_;
        // Tracks the file cursor so sequential reads skip the seek() syscall,
        // which is the dominant cost for single-track and Format-0 files.
        uint32_t position_{kUnknownPosition};
        Stats stats_{};
    };

    static void taskEntry(void* context);
    void taskLoop();
    void handleTransportFailure();
    void handleProjectTransport();
    ProjectTransportReadResult readProjectTransport(
        GroovePuterMidi::ProjectTransportBlockSnapshot& transport);
    void pauseForStaleProjectTimeline();
    void reanchorProjectTempo(
        const GroovePuterMidi::ProjectTransportBlockSnapshot& transport);
    bool enqueue(const Command& command);
    void handleCommand(const Command& command);

    bool loadFile(const char* path);
    bool scanMetadata();
    bool prepareStreamAt(uint32_t tick);
    bool startFromTick(uint32_t tick);
    bool startOriginalFromTick(uint32_t tick);
    bool armProjectFromTick(uint32_t tick);
    bool planProjectLaunch(
        const GroovePuterMidi::ProjectTransportBlockSnapshot& transport);
    void pauseAtCurrentPosition();
    void stopAndCleanup(bool resetToMusicStart);
    void scheduleAhead();
    void logPerformance();
    void updatePlaybackSnapshot();
    void applyTempoScale(uint16_t scalePermille);
    uint16_t originalBpmX10At(uint32_t tick) const;
    uint16_t effectiveBpmX10At(uint32_t tick) const;
    uint32_t currentTickFromAudioClock();
    uint32_t currentProjectTick(
        const GroovePuterMidi::ProjectTransportBlockSnapshot& transport) const;
    double currentProjectSmfTick(
        const GroovePuterMidi::ProjectTransportBlockSnapshot& transport) const;
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
    // Memo of the tick prepareStreamAt() last positioned the stream at, so a
    // repeat request skips the scan entirely.
    uint32_t streamPreparedTick_{0};
    bool streamPreparedValid_{false};
    uint32_t musicStartTick_{0};
    uint32_t endTick_{0};
    uint32_t playbackOriginTick_{0};
    uint32_t playbackOriginBlock_{0};
    uint16_t playbackOriginFrame_{0};
    uint32_t playbackOriginMicros_{0};
    uint32_t lastScheduledBlock_{0};
    uint32_t pausedTick_{0};
    bool loaded_{false};
    static constexpr uint16_t kMinBpmX10 = 400;
    static constexpr uint16_t kMaxBpmX10 = 2500;
    uint16_t tempoScalePermille_{
        GroovePuterMidi::kSmfOriginalTempoScalePermille};
    uint8_t velocityBoost_{0};

    GroovePuterMidi::SmfTempoMode tempoMode_{
        GroovePuterMidi::SmfTempoMode::Original};
    GroovePuterMidi::SmfLaunchMode launchMode_{
        GroovePuterMidi::SmfLaunchMode::NextBar};
    bool projectLaunchPlanned_{false};
    // Only an SMF that was active before external Stop gets bounded relaunch.
    // SEQTRAK's next FA restarts it; a real FB from another controller resumes.
    bool projectRelaunchAfterExternalStop_{false};
    double projectOriginStep_{0.0};
    double projectOriginSmfTick_{0.0};
    uint16_t projectBpmX10_{1200};
    uint32_t projectBpmQ16_{120u << 16};
    uint32_t projectTransportEpoch_{0};
    GroovePuterMidi::ProjectTransportBlockSnapshot lastProjectTransport_{};
    bool haveLastProjectTransport_{false};

    static constexpr uint32_t kPerfUnsetDepth = 0xFFFFFFFFu;
    static constexpr uint32_t kPerfLogIntervalMs = 2000;
    uint32_t perfLastLogMs_{0};
    uint32_t perfScheduleCalls_{0};
    uint32_t perfMaxScheduleMicros_{0};
    uint32_t perfMinQueueDepth_{kPerfUnsetDepth};
    uint32_t perfQueuedEvents_{0};
    uint32_t perfUnmappedEventsFiltered_{0};
    uint32_t perfProjectLateNoteOnDrops_{0};
    uint32_t perfTimelineReadMisses_{0};
    uint32_t perfTimelineStalePauses_{0};
    uint32_t perfTempoReanchors_{0};

    GroovePuterMidi::SmfRoutingMode routingMode_{
        GroovePuterMidi::SmfRoutingMode::Seqtrak};

    mutable portMUX_TYPE snapshotMux_ = portMUX_INITIALIZER_UNLOCKED;
    GroovePuterMidi::SmfPlayerSnapshot snapshot_{};

    StaticQueue_t commandQueueStruct_{};
    alignas(4) uint8_t commandQueueStorage_[kCommandDepth * sizeof(Command)]{};
    QueueHandle_t commandQueue_{nullptr};
    TaskHandle_t taskHandle_{nullptr};
};
