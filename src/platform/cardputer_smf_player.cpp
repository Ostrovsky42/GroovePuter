#include "cardputer_smf_player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#include <esp_heap_caps.h>
#include "src/audio/audio_config.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/platform/cardputer_usb_midi_service.h"

using namespace GroovePuterMidi;

namespace {
constexpr uint32_t kPlayerTaskStack = 6144;
constexpr UBaseType_t kPlayerTaskPriority = 1;
constexpr BaseType_t kPlayerTaskCore = 0;
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(2);
constexpr double kProjectBoundaryEpsilon = 1.0e-3;
constexpr double kExternalRelaunchPrefillBlocks = 3.0;

bool followsSeqtrakClock() {
    return transportClockRuntime().source() ==
        TransportClockSource::SeqtrakExternal;
}
}

CardputerSmfPlayerService::CardputerSmfPlayerService() {
    snapshot_.state = SmfPlayerState::Unloaded;
    snapshot_.rawRouting = false;
    snapshot_.tempoMode = tempoMode_;
    snapshot_.launchMode = launchMode_;
    copyText(snapshot_.message, sizeof(snapshot_.message), "Select a MIDI file");
}

bool CardputerSmfPlayerService::begin() {
    if (taskHandle_ != nullptr) return true;

    const uint32_t freeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const uint32_t largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    Serial.printf("[SMF-INIT] begin freeInt=%u largest=%u\n",
                  static_cast<unsigned>(freeBefore),
                  static_cast<unsigned>(largestBefore));

    try {
        timingDocument_.events.reserve(kMaxTimingEvents);
    } catch (const std::bad_alloc&) {
        Serial.println("[SMF-INIT] timing document reserve failed");
        publishSnapshot(SmfPlayerState::Error, "SMF memory unavailable");
        return false;
    }
    if (!timing_.reserveForEvents(kMaxTimingEvents)) {
        Serial.println("[SMF-INIT] timing map reserve failed");
        publishSnapshot(SmfPlayerState::Error, "SMF memory unavailable");
        return false;
    }

    commandQueue_ = xQueueCreateStatic(
        kCommandDepth,
        sizeof(Command),
        commandQueueStorage_,
        &commandQueueStruct_);
    if (commandQueue_ == nullptr) {
        Serial.println("[SMF-INIT] command queue creation failed");
        publishSnapshot(SmfPlayerState::Error, "SMF queue unavailable");
        return false;
    }

    const BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "SmfPlayerTask",
        kPlayerTaskStack,
        this,
        kPlayerTaskPriority,
        &taskHandle_,
        kPlayerTaskCore);
    if (result != pdPASS) {
        taskHandle_ = nullptr;
        Serial.printf("[SMF-INIT] task creation failed: %d freeInt=%u largest=%u\n",
                      static_cast<int>(result),
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                      static_cast<unsigned>(
                          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        publishSnapshot(SmfPlayerState::Error, "SMF task unavailable");
        return false;
    }
    Serial.printf("[SMF-INIT] ready freeInt=%u largest=%u\n",
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                  static_cast<unsigned>(
                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    return true;
}

bool CardputerSmfPlayerService::requestLoad(const char* path) {
    if (!path || !path[0]) return false;
    Command command{};
    command.type = CommandType::Load;
    copyText(command.path, sizeof(command.path), path);
    return enqueue(command);
}

bool CardputerSmfPlayerService::togglePlayPause() {
    Command command{};
    command.type = CommandType::TogglePlayPause;
    return enqueue(command);
}

bool CardputerSmfPlayerService::pause() {
    Command command{};
    command.type = CommandType::Pause;
    return enqueue(command);
}

bool CardputerSmfPlayerService::restart(SmfPlayerRestartOrigin origin) {
    Command command{};
    command.type = origin == SmfPlayerRestartOrigin::FileStart
        ? CommandType::RestartFile
        : CommandType::RestartMusic;
    return enqueue(command);
}

bool CardputerSmfPlayerService::stop() {
    Command command{};
    command.type = CommandType::Stop;
    return enqueue(command);
}

bool CardputerSmfPlayerService::panic() {
    Command command{};
    command.type = CommandType::Panic;
    return enqueue(command);
}

bool CardputerSmfPlayerService::seekBars(int deltaBars) {
    if (deltaBars == 0) return true;
    Command command{};
    command.type = CommandType::SeekBars;
    command.value = deltaBars;
    return enqueue(command);
}

bool CardputerSmfPlayerService::toggleRouting() {
    Command command{};
    command.type = CommandType::ToggleRouting;
    return enqueue(command);
}

bool CardputerSmfPlayerService::toggleTempoMode() {
    Command command{};
    command.type = CommandType::ToggleTempoMode;
    return enqueue(command);
}

bool CardputerSmfPlayerService::adjustTempoBpm(int deltaBpm) {
    if (deltaBpm == 0) return true;
    Command command{};
    command.type = CommandType::AdjustTempoBpm;
    command.value = deltaBpm;
    return enqueue(command);
}

bool CardputerSmfPlayerService::resetTempo() {
    Command command{};
    command.type = CommandType::ResetTempo;
    return enqueue(command);
}

bool CardputerSmfPlayerService::cycleVelocityBoost() {
    Command command{};
    command.type = CommandType::CycleVelocityBoost;
    return enqueue(command);
}

SmfPlayerSnapshot CardputerSmfPlayerService::snapshot() const {
    portENTER_CRITICAL(&snapshotMux_);
    const SmfPlayerSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&snapshotMux_);
    return copy;
}

SmfChannelInspectorSnapshot CardputerSmfPlayerService::channelInspector() const {
    portENTER_CRITICAL(&snapshotMux_);
    const SmfChannelInspectorSnapshot copy = channelInspector_;
    portEXIT_CRITICAL(&snapshotMux_);
    return copy;
}

bool CardputerSmfPlayerService::SdByteSource::open(const char* path) {
    close();
    file_ = SD.open(path, FILE_READ);
    position_ = kUnknownPosition;
    stats_ = Stats{};
    return static_cast<bool>(file_);
}

void CardputerSmfPlayerService::SdByteSource::close() {
    if (file_) file_.close();
    position_ = kUnknownPosition;
}

uint32_t CardputerSmfPlayerService::SdByteSource::size() const {
    return file_ ? static_cast<uint32_t>(file_.size()) : 0;
}

bool CardputerSmfPlayerService::SdByteSource::readAt(
        uint32_t offset,
        uint8_t* dst,
        std::size_t length) {
    if (!file_ || !dst) return false;
    const uint32_t fileSize = static_cast<uint32_t>(file_.size());
    if (offset > fileSize || length > fileSize - offset) return false;

    const uint32_t started = micros();
    if (position_ != offset) {
        if (!file_.seek(offset)) {
            position_ = kUnknownPosition;
            return false;
        }
        position_ = offset;
        ++stats_.seeks;
    }
    if (file_.read(dst, length) != static_cast<int>(length)) {
        position_ = kUnknownPosition;
        return false;
    }
    position_ = offset + static_cast<uint32_t>(length);

    const uint32_t elapsed = micros() - started;
    ++stats_.reads;
    stats_.bytes += static_cast<uint32_t>(length);
    if (elapsed > stats_.maxReadMicros) stats_.maxReadMicros = elapsed;
    return true;
}

void CardputerSmfPlayerService::taskEntry(void* context) {
    static_cast<CardputerSmfPlayerService*>(context)->taskLoop();
}

void CardputerSmfPlayerService::taskLoop() {
    while (true) {
        handleTransportFailure();

        Command command{};
        while (commandQueue_ != nullptr &&
               xQueueReceive(commandQueue_, &command, 0) == pdTRUE) {
            handleCommand(command);
        }

        handleProjectTransport();

        SmfPlayerState state;
        {
            portENTER_CRITICAL(&snapshotMux_);
            state = snapshot_.state;
            portEXIT_CRITICAL(&snapshotMux_);
        }
        if (state == SmfPlayerState::Playing ||
            (state == SmfPlayerState::Armed && projectLaunchPlanned_)) {
            scheduleAhead();
            updatePlaybackSnapshot();
            logPerformance();
        } else if (state == SmfPlayerState::Armed) {
            updatePlaybackSnapshot();
        }
        vTaskDelay(kIdleDelay);
    }
}

void CardputerSmfPlayerService::handleTransportFailure() {
    uint32_t generation = 0;
    if (eventQueue_.takePendingTransportFailure(generation)) {
        // A host that stopped reading the endpoint is a wait condition, not a
        // broken file or a broken player: hold the position and keep the state
        // resumable instead of ending playback with an error.
        if (loaded_) {
            pausedTick_ = currentTickFromAudioClock();
            hasPendingEvent_ = false;
            projectLaunchPlanned_ = false;
            projectRelaunchAfterExternalStop_ = false;
            publishSnapshot(SmfPlayerState::Paused,
                            "USB MIDI BLOCKED - KEYS WAIT");
            updatePlaybackSnapshot();
        }
        Serial.printf("[SMF-WAIT] transport stalled generation=%u tick=%u\n",
                      static_cast<unsigned>(generation),
                      static_cast<unsigned>(pausedTick_));
    }

    if (eventQueue_.takePendingTransportRecovery()) {
        // Restarting playback here is not safe: the endpoint accepting one write
        // says nothing about the audio anchor, and resuming automatically both
        // replayed a multi-second backlog and oscillated stall -> resume -> stall
        // until the dispatcher starved the CPU0 idle task. Recovery only makes
        // the transport available again; continuing is the user's call.
        if (loaded_) {
            publishSnapshot(SmfPlayerState::Paused,
                            "USB MIDI READY - PRESS PLAY");
            updatePlaybackSnapshot();
        }
        Serial.printf("[SMF-WAIT] transport available again tick=%u\n",
                      static_cast<unsigned>(pausedTick_));
    }
}

void CardputerSmfPlayerService::handleProjectTransport() {
    if (!loaded_ || tempoMode_ != SmfTempoMode::Project) return;

    ProjectTransportBlockSnapshot transport{};
    const ProjectTransportReadResult readResult = readProjectTransport(transport);
    if (readResult == ProjectTransportReadResult::Unavailable) return;
    if (readResult == ProjectTransportReadResult::Stale) {
        pauseForStaleProjectTimeline();
        return;
    }
    const SmfPlayerState state = snapshot().state;

    if (state == SmfPlayerState::Armed) {
        if (!transport.valid) {
            publishSnapshot(SmfPlayerState::Armed,
                            followsSeqtrakClock()
                                ? "WAIT SEQTRAK PLAY"
                                : "WAIT GP MASTER PLAY");
            return;
        }
        if (!transport.playing) {
            if (transportClockRuntime().source() ==
                TransportClockSource::SeqtrakExternal) {
                publishSnapshot(SmfPlayerState::Armed,
                                "WAIT SEQTRAK PLAY");
                return;
            }
            pausedTick_ = snapshot().currentTick;
            eventQueue_.invalidateAndRequestPanic();
            projectLaunchPlanned_ = false;
            projectRelaunchAfterExternalStop_ = false;
            prepareStreamAt(pausedTick_);
            publishSnapshot(SmfPlayerState::Paused, "GP STOP / MIDI PAUSED");
            updatePlaybackSnapshot();
            return;
        }
        if (!projectLaunchPlanned_ && projectRelaunchAfterExternalStop_ &&
            transport.restartedFromBeginning) {
            // SEQTRAK emits FA after FC and has no FB transmit path. Return the
            // active file to MUSIC START, then use bounded relaunch below so
            // the user does not need R or wait for NEXT BAR.
            pausedTick_ = musicStartTick_;
            if (!prepareStreamAt(pausedTick_)) {
                projectRelaunchAfterExternalStop_ = false;
                publishSnapshot(SmfPlayerState::Error,
                                "SMF START RESET FAILED");
                return;
            }
        }
        if (projectLaunchPlanned_ &&
            transport.transportEpoch != projectTransportEpoch_) {
            pauseForStaleProjectTimeline();
            return;
        }
        if (projectLaunchPlanned_ && transport.bpmQ16 != projectBpmQ16_) {
            eventQueue_.invalidateScheduledEvents();
            projectLaunchPlanned_ = false;
            prepareStreamAt(pausedTick_);
        }
        if (!projectLaunchPlanned_ && !planProjectLaunch(transport)) return;
        if (projectLaunchPlanned_ &&
            transport.absoluteSteps() + kProjectBoundaryEpsilon >= projectOriginStep_) {
            publishSnapshot(SmfPlayerState::Playing,
                            followsSeqtrakClock()
                                ? "SEQ MASTER / SYNC"
                                : "GP MASTER / SYNC");
        }
        return;
    }

    if (state != SmfPlayerState::Playing) return;

    if (!transport.valid || !transport.playing) {
        // A stopped engine publishes phase zero. Reconstructing from that phase
        // can jump backwards, so preserve the last playing snapshot instead.
        pausedTick_ = snapshot().currentTick;
        eventQueue_.invalidateAndRequestPanic();
        prepareStreamAt(pausedTick_);
        projectLaunchPlanned_ = false;
        const bool followExternal = followsSeqtrakClock();
        projectRelaunchAfterExternalStop_ = followExternal;
        publishSnapshot(followExternal
                            ? SmfPlayerState::Armed
                            : SmfPlayerState::Paused,
                        followExternal
                            ? "WAIT SEQTRAK PLAY"
                            : "GP STOP / MIDI PAUSED");
        updatePlaybackSnapshot();
        return;
    }

    if (transport.transportEpoch != projectTransportEpoch_) {
        pauseForStaleProjectTimeline();
        return;
    }

    if (transport.bpmQ16 != projectBpmQ16_) {
        reanchorProjectTempo(transport);
    }
}

CardputerSmfPlayerService::ProjectTransportReadResult
CardputerSmfPlayerService::readProjectTransport(
        ProjectTransportBlockSnapshot& transport) {
    ProjectTransportBlockSnapshot candidate{};
    if (!projectTransportTimeline().trySnapshot(candidate)) {
        ++perfTimelineReadMisses_;
        return ProjectTransportReadResult::Unavailable;
    }

    if (candidate.valid && candidate.playing && candidate.blockFrames > 0 &&
        candidate.sampleRate > 0) {
        uint32_t anchorBlock = 0;
        uint32_t anchorMicros = 0;
        if (!snapshotCardputerUsbMidiBlockAnchor(anchorBlock, anchorMicros)) {
            ++perfTimelineReadMisses_;
            return ProjectTransportReadResult::Unavailable;
        }
        const uint32_t blockDurationMicros = static_cast<uint32_t>(
            (static_cast<uint64_t>(candidate.blockFrames) * 1000000ull) /
            candidate.sampleRate);
        if (projectTimelineIsStale(candidate.blockSequence,
                                   anchorBlock,
                                   anchorMicros,
                                   micros(),
                                   blockDurationMicros,
                                   kProjectTimelineMaxAgeBlocks)) {
            transport = candidate;
            return ProjectTransportReadResult::Stale;
        }
    }

    transport = candidate;
    lastProjectTransport_ = candidate;
    haveLastProjectTransport_ = true;
    return ProjectTransportReadResult::Fresh;
}

void CardputerSmfPlayerService::pauseForStaleProjectTimeline() {
    const SmfPlayerState state = snapshot().state;
    if (state != SmfPlayerState::Playing && state != SmfPlayerState::Armed) return;
    ++perfTimelineStalePauses_;
    pausedTick_ = haveLastProjectTransport_ && lastProjectTransport_.playing
        ? currentProjectTick(lastProjectTransport_)
        : snapshot().currentTick;
    eventQueue_.invalidateAndRequestPanic();
    projectLaunchPlanned_ = false;
    projectRelaunchAfterExternalStop_ = false;
    prepareStreamAt(pausedTick_);
    publishSnapshot(SmfPlayerState::Paused,
                    followsSeqtrakClock()
                        ? "SEQ CLOCK STALE / MIDI PAUSED"
                        : "GP CLOCK STALE / MIDI PAUSED");
    updatePlaybackSnapshot();
}

void CardputerSmfPlayerService::reanchorProjectTempo(
        const ProjectTransportBlockSnapshot& transport) {
    if (!transport.valid || !transport.playing || transport.blockFrames == 0 ||
        transport.sampleRate == 0 || transport.bpmX10 == 0) {
        return;
    }

    double currentTick = currentProjectSmfTick(transport);
    uint32_t streamTick = static_cast<uint32_t>(std::floor(currentTick));

    // Stop the old-BPM generation before a potentially long SD rescan. Active
    // wire-owned notes remain alive; only their future deadlines are rebuilt.
    eventQueue_.invalidateScheduledEvents();

    auto failReanchor = [this, &streamTick](SmfPlayerState state,
                                            const char* message) {
        // Old queued NoteOffs were invalidated before the scan. If rebuilding
        // cannot finish, scoped cleanup is mandatory to avoid stuck notes.
        eventQueue_.invalidateAndRequestPanic();
        projectLaunchPlanned_ = false;
        projectRelaunchAfterExternalStop_ = false;
        pausedTick_ = streamTick;
        publishSnapshot(state, message);
        updatePlaybackSnapshot();
    };

    if (!prepareStreamAt(streamTick)) {
        failReanchor(SmfPlayerState::Error, "TEMPO REANCHOR FAILED");
        return;
    }

    // A long SD scan may span several audio blocks. Refresh the anchor and
    // advance the parser to the corresponding tick before replacing deadlines.
    ProjectTransportBlockSnapshot anchor{};
    ProjectTransportBlockSnapshot refreshed{};
    ProjectTransportReadResult readResult = ProjectTransportReadResult::Unavailable;
    for (int attempt = 0; attempt < 4; ++attempt) {
        readResult = readProjectTransport(refreshed);
        if (readResult != ProjectTransportReadResult::Unavailable) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (readResult == ProjectTransportReadResult::Unavailable) {
        failReanchor(SmfPlayerState::Error, "TEMPO CLOCK UNAVAILABLE");
        return;
    }
    if (readResult == ProjectTransportReadResult::Stale) {
        pauseForStaleProjectTimeline();
        return;
    }
    if (!refreshed.valid || !refreshed.playing ||
        refreshed.transportEpoch != transport.transportEpoch) {
        failReanchor(SmfPlayerState::Paused, "GP TRANSPORT CHANGED");
        return;
    }
    anchor = refreshed;
    currentTick = currentProjectSmfTick(anchor);
    const uint32_t refreshedStreamTick =
        static_cast<uint32_t>(std::floor(currentTick));
    if (refreshedStreamTick != streamTick &&
        !prepareStreamAt(refreshedStreamTick)) {
        failReanchor(SmfPlayerState::Error, "TEMPO REANCHOR FAILED");
        return;
    }
    streamTick = refreshedStreamTick;

    // Existing wire-owned notes remain active. Their future NoteOff events are
    // rebuilt from streamTick under the new generation and BPM.
    playbackOriginTick_ = streamTick;
    projectOriginSmfTick_ = currentTick;
    playbackOriginBlock_ = anchor.blockSequence;
    playbackOriginFrame_ = 0;
    projectOriginStep_ = anchor.absoluteSteps();
    projectBpmX10_ = anchor.bpmX10;
    projectBpmQ16_ = anchor.bpmQ16;
    projectTransportEpoch_ = anchor.transportEpoch;
    pausedTick_ = streamTick;
    lastScheduledBlock_ = playbackOriginBlock_;
    projectLaunchPlanned_ = true;
    ++perfTempoReanchors_;
    publishSnapshot(SmfPlayerState::Playing,
                    followsSeqtrakClock()
                        ? "SEQ MASTER TEMPO SYNC"
                        : "GP MASTER TEMPO SYNC");
}

bool CardputerSmfPlayerService::enqueue(const Command& command) {
    return commandQueue_ != nullptr &&
           xQueueSend(commandQueue_, &command, 0) == pdTRUE;
}

void CardputerSmfPlayerService::handleCommand(const Command& command) {
    switch (command.type) {
        case CommandType::Load:
            publishSnapshot(SmfPlayerState::Loading, "Scanning MIDI...");
            try {
                loadFile(command.path);
            } catch (const std::bad_alloc&) {
                stopAndCleanup(false);
                source_.close();
                loaded_ = false;
                publishSnapshot(SmfPlayerState::Error, "Not enough memory");
                Serial.printf("[SMF-LOAD] allocation failed freeInt=%u largest=%u\n",
                              static_cast<unsigned>(
                                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                              static_cast<unsigned>(
                                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
            }
            break;
        case CommandType::TogglePlayPause: {
            const SmfPlayerState state = snapshot().state;
            if (!loaded_) break;
            if (state == SmfPlayerState::Playing || state == SmfPlayerState::Armed) {
                pauseAtCurrentPosition();
            } else if (state == SmfPlayerState::Paused) {
                startFromTick(pausedTick_);
            } else {
                startFromTick(musicStartTick_);
            }
            break;
        }
        case CommandType::Pause: {
            const SmfPlayerState state = snapshot().state;
            if (loaded_ &&
                (state == SmfPlayerState::Playing || state == SmfPlayerState::Armed)) {
                pauseAtCurrentPosition();
            }
            break;
        }
        case CommandType::RestartMusic:
            if (loaded_) startFromTick(musicStartTick_);
            break;
        case CommandType::RestartFile:
            if (loaded_) startFromTick(0);
            break;
        case CommandType::Stop:
            stopAndCleanup(true);
            break;
        case CommandType::Panic:
            if (loaded_) {
                pausedTick_ = currentTickFromAudioClock();
                eventQueue_.invalidateAndRequestPanic();
                projectLaunchPlanned_ = false;
                projectRelaunchAfterExternalStop_ = false;
                publishSnapshot(SmfPlayerState::Paused, "PANIC / PAUSED");
            }
            break;
        case CommandType::SeekBars: {
            if (!loaded_ || !timing_.valid()) break;
            const SmfPlayerState previous = snapshot().state;
            const SmfBarBeat current = timing_.barBeatForTick(
                previous == SmfPlayerState::Playing
                    ? currentTickFromAudioClock()
                    : pausedTick_);
            int64_t targetBar = static_cast<int64_t>(current.bar) + command.value;
            if (targetBar < 1) targetBar = 1;
            const uint32_t totalBars = timing_.barBeatForTick(endTick_).bar;
            if (targetBar > static_cast<int64_t>(totalBars)) targetBar = totalBars;
            const uint32_t targetTick = timing_.tickForBar(
                static_cast<uint32_t>(targetBar));
            eventQueue_.invalidateAndRequestPanic();
            projectLaunchPlanned_ = false;
            projectRelaunchAfterExternalStop_ = false;
            if (previous == SmfPlayerState::Playing || previous == SmfPlayerState::Armed) {
                startFromTick(targetTick);
            } else {
                pausedTick_ = targetTick;
                prepareStreamAt(targetTick);
                publishSnapshot(SmfPlayerState::Paused, "SEEK");
                updatePlaybackSnapshot();
            }
            break;
        }
        case CommandType::ToggleRouting: {
            const SmfPlayerState previous = snapshot().state;
            const uint32_t resumeTick = previous == SmfPlayerState::Playing
                ? currentTickFromAudioClock()
                : pausedTick_;
            eventQueue_.invalidateAndRequestPanic();
            projectLaunchPlanned_ = false;
            projectRelaunchAfterExternalStop_ = false;
            routingMode_ = routingMode_ == SmfRoutingMode::Raw
                ? SmfRoutingMode::Seqtrak
                : SmfRoutingMode::Raw;
            portENTER_CRITICAL(&snapshotMux_);
            snapshot_.rawRouting = routingMode_ == SmfRoutingMode::Raw;
            portEXIT_CRITICAL(&snapshotMux_);
            if (loaded_ &&
                (previous == SmfPlayerState::Playing || previous == SmfPlayerState::Armed)) {
                startFromTick(resumeTick);
            } else if (loaded_) {
                prepareStreamAt(resumeTick);
                publishSnapshot(previous,
                    routingMode_ == SmfRoutingMode::Raw
                        ? "RAW ROUTING"
                        : "SEQTRAK / GM MAP");
            }
            break;
        }
        case CommandType::ToggleTempoMode: {
            const SmfPlayerState previous = snapshot().state;
            const bool wasActive = previous == SmfPlayerState::Playing ||
                                   previous == SmfPlayerState::Armed;
            const uint32_t resumeTick = loaded_ && previous == SmfPlayerState::Playing
                ? currentTickFromAudioClock()
                : pausedTick_;
            if (loaded_) {
                eventQueue_.invalidateAndRequestPanic();
                projectLaunchPlanned_ = false;
                projectRelaunchAfterExternalStop_ = false;
                pausedTick_ = resumeTick;
                prepareStreamAt(resumeTick);
            }
            tempoMode_ = tempoMode_ == SmfTempoMode::Original
                ? SmfTempoMode::Project
                : SmfTempoMode::Original;
            if (tempoMode_ == SmfTempoMode::Project) {
                applyTempoScale(kSmfOriginalTempoScalePermille);
            }
            portENTER_CRITICAL(&snapshotMux_);
            snapshot_.tempoMode = tempoMode_;
            snapshot_.launchMode = launchMode_;
            portEXIT_CRITICAL(&snapshotMux_);
            const ProjectTransportBlockSnapshot projectTransport =
                projectTransportTimeline().snapshot();
            if (loaded_ && wasActive &&
                (tempoMode_ != SmfTempoMode::Project ||
                 (projectTransport.valid && projectTransport.playing))) {
                // Switching the clock source is one player command. Keeping
                // resume here avoids a second UI command racing or being lost
                // when the bounded command queue is busy.
                startFromTick(resumeTick);
            } else if (loaded_) {
                publishSnapshot(
                    tempoMode_ == SmfTempoMode::Project && wasActive
                        ? SmfPlayerState::Paused
                        : previous,
                    tempoMode_ == SmfTempoMode::Project
                        ? (followsSeqtrakClock()
                               ? "SPACE ARM / SEQ PLAY"
                               : "G START / SPACE MIDI")
                        : "TEMPO ORIGINAL");
                updatePlaybackSnapshot();
            } else {
                publishSnapshot(previous,
                    tempoMode_ == SmfTempoMode::Project
                        ? (followsSeqtrakClock()
                               ? "SEQ MASTER TEMPO"
                               : "GP MASTER TEMPO")
                        : "ORIGINAL TEMPO");
            }
            break;
        }
        case CommandType::AdjustTempoBpm: {
            if (!loaded_ || !timing_.valid()) break;
            if (tempoMode_ == SmfTempoMode::Project) {
                publishSnapshot(snapshot().state,
                                followsSeqtrakClock()
                                    ? "SEQ MASTER BPM"
                                    : "GP MASTER BPM");
                updatePlaybackSnapshot();
                break;
            }
            const SmfPlayerState previous = snapshot().state;
            if (previous == SmfPlayerState::Playing) pauseAtCurrentPosition();

            const uint16_t originalBpmX10 = originalBpmX10At(pausedTick_);
            const int32_t currentBpmX10 = effectiveBpmX10At(pausedTick_);
            const int32_t targetBpmX10 = std::max<int32_t>(
                kMinBpmX10,
                std::min<int32_t>(kMaxBpmX10,
                                  currentBpmX10 + command.value * 10));
            const uint32_t scale =
                (static_cast<uint32_t>(targetBpmX10) *
                     kSmfOriginalTempoScalePermille + originalBpmX10 / 2u) /
                originalBpmX10;
            applyTempoScale(static_cast<uint16_t>(std::min<uint32_t>(
                scale, std::numeric_limits<uint16_t>::max())));
            publishSnapshot(previous == SmfPlayerState::Playing
                                ? SmfPlayerState::Paused
                                : previous,
                            previous == SmfPlayerState::Playing
                                ? "TEMPO SET - SPACE TO PLAY"
                                : "TEMPO SET");
            updatePlaybackSnapshot();
            break;
        }
        case CommandType::ResetTempo: {
            if (!loaded_ || !timing_.valid()) break;
            if (tempoMode_ == SmfTempoMode::Project) {
                publishSnapshot(snapshot().state,
                                followsSeqtrakClock()
                                    ? "SEQ MASTER BPM"
                                    : "GP MASTER BPM");
                updatePlaybackSnapshot();
                break;
            }
            const SmfPlayerState previous = snapshot().state;
            if (previous == SmfPlayerState::Playing) pauseAtCurrentPosition();
            applyTempoScale(kSmfOriginalTempoScalePermille);
            publishSnapshot(previous == SmfPlayerState::Playing
                                ? SmfPlayerState::Paused
                                : previous,
                            "TEMPO ORIGINAL");
            updatePlaybackSnapshot();
            break;
        }
        case CommandType::CycleVelocityBoost:
            velocityBoost_ = nextSmfVelocityBoost(velocityBoost_);
            portENTER_CRITICAL(&snapshotMux_);
            snapshot_.velocityBoost = velocityBoost_;
            portEXIT_CRITICAL(&snapshotMux_);
            break;
    }
}

bool CardputerSmfPlayerService::loadFile(const char* path) {
    stopAndCleanup(false);
    resetMidiVisual();
    source_.close();
    loaded_ = false;
    haveLastProjectTransport_ = false;
    lastProjectTransport_ = ProjectTransportBlockSnapshot{};
    timingDocument_.events.clear();
    portENTER_CRITICAL(&snapshotMux_);
    channelInspector_ = SmfChannelInspectorSnapshot{};
    portEXIT_CRITICAL(&snapshotMux_);

    if (!source_.open(path)) {
        publishSnapshot(SmfPlayerState::Error, "Cannot open MIDI");
        return false;
    }

    const SmfIndexResult indexed = SmfFileIndexer::build(source_);
    if (!indexed.ok()) {
        publishSnapshot(SmfPlayerState::Error, SmfParser::errorString(indexed.error));
        source_.close();
        return false;
    }
    fileIndex_ = indexed.index;
    SmfChannelInspectorBuilder inspectorBuilder;
    inspectorBuilder.reset(fileIndex_.format,
                           fileIndex_.division,
                           fileIndex_.trackCount);

    if (!stream_.open(source_, fileIndex_)) {
        publishSnapshot(SmfPlayerState::Error, "Stream init failed");
        source_.close();
        return false;
    }

    // Preserve the storage reserved during setup; assigning a temporary
    // document here would discard capacity and reintroduce runtime allocation.
    timingDocument_.events.clear();
    timingDocument_.tracks.clear();
    timingDocument_.format = fileIndex_.format;
    timingDocument_.division = fileIndex_.division;
    timingDocument_.musicStartTick = 0;
    timingDocument_.endTick = 0;
    musicStartTick_ = 0;
    endTick_ = 0;
    bool foundMusic = false;

    SmfStreamEvent event{};
    while (stream_.next(event)) {
        inspectorBuilder.observe(event.event);
        endTick_ = std::max(endTick_, event.event.tick);
        if (!foundMusic && event.event.kind == SmfEventKind::NoteOn) {
            musicStartTick_ = event.event.tick;
            foundMusic = true;
        }
        if (event.event.kind == SmfEventKind::Tempo ||
            event.event.kind == SmfEventKind::TimeSignature) {
            if (timingDocument_.events.size() >= kMaxTimingEvents) {
                publishSnapshot(SmfPlayerState::Error, "Too many tempo events");
                source_.close();
                return false;
            }
            timingDocument_.events.push_back(event.event);
        }
    }
    timingDocument_.musicStartTick = musicStartTick_;
    timingDocument_.endTick = endTick_;

    if (!foundMusic) {
        publishSnapshot(SmfPlayerState::Error, "MIDI has no notes");
        source_.close();
        return false;
    }
    if (!timing_.build(timingDocument_)) {
        publishSnapshot(SmfPlayerState::Error, "Timing map failed");
        source_.close();
        return false;
    }

    portENTER_CRITICAL(&snapshotMux_);
    channelInspector_ = inspectorBuilder.snapshot();
    portEXIT_CRITICAL(&snapshotMux_);

    stream_.reset();
    loaded_ = true;
    tempoScalePermille_ = kSmfOriginalTempoScalePermille;
    // Velocity is a player-session preference: keep the selected boost while
    // browsing and loading another file, but still default to +0 after reboot.
    pausedTick_ = musicStartTick_;
    hasPendingEvent_ = false;
    streamEnded_ = false;
    streamPreparedValid_ = false;
    projectLaunchPlanned_ = false;
    projectRelaunchAfterExternalStop_ = false;

    portENTER_CRITICAL(&snapshotMux_);
    copyText(snapshot_.filename, sizeof(snapshot_.filename), basename(path));
    snapshot_.endTick = endTick_;
    snapshot_.totalBars = timing_.barBeatForTick(endTick_).bar;
    snapshot_.rawRouting = routingMode_ == SmfRoutingMode::Raw;
    snapshot_.tempoScalePermille = tempoScalePermille_;
    snapshot_.velocityBoost = velocityBoost_;
    snapshot_.tempoMode = tempoMode_;
    snapshot_.launchMode = launchMode_;
    portEXIT_CRITICAL(&snapshotMux_);

    publishSnapshot(SmfPlayerState::Stopped,
        tempoMode_ == SmfTempoMode::Project
            ? (followsSeqtrakClock()
                   ? "LOADED / SPACE ARM / SEQ PLAY"
                   : "LOADED / G THEN SPACE")
            : (routingMode_ == SmfRoutingMode::Raw
                ? "LOADED / RAW"
                : "LOADED / SEQTRAK"));
    Serial.printf("[SMF-LOAD] ready events=%u freeInt=%u largest=%u stackFree=%u\n",
                  static_cast<unsigned>(timingDocument_.events.size()),
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                  static_cast<unsigned>(
                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    return true;
}

bool CardputerSmfPlayerService::scanMetadata() {
    return loaded_ && timing_.valid();
}

bool CardputerSmfPlayerService::prepareStreamAt(uint32_t tick) {
    if (!loaded_) return false;
    // The stream is already positioned exactly where this call would leave it,
    // which is the common pause -> resume and routing-toggle case.
    if (streamPreparedValid_ && streamPreparedTick_ == tick) return true;

    // A full rescan re-reads every track from its first tick, which costs
    // seconds on a long file. Only backward seeks need it: when the stream head
    // is still behind the target, keep scanning forward from where we are.
    bool forward = false;
    if (hasPendingEvent_) {
        forward = pendingEvent_.event.tick < tick;
    } else if (!streamEnded_) {
        SmfStreamEvent head{};
        forward = stream_.peek(head) && head.event.tick < tick;
    }
    if (!forward) stream_.reset();

    hasPendingEvent_ = false;
    streamEnded_ = false;
    streamPreparedTick_ = tick;
    streamPreparedValid_ = true;

    SmfStreamEvent event{};
    while (stream_.next(event)) {
        if (event.event.tick < tick) continue;
        pendingEvent_ = event;
        hasPendingEvent_ = true;
        return true;
    }
    streamEnded_ = true;
    return true;
}

bool CardputerSmfPlayerService::startFromTick(uint32_t tick) {
    // Only a successful mounted USB write may clear endpoint backpressure.
    // User commands can cancel automatic resume, but must not schedule events
    // against an endpoint that is still stalled: those deadlines would all be
    // stale by the time the receiver returns.
    if (eventQueue_.transportFailed()) {
        publishSnapshot(SmfPlayerState::Paused,
                        "USB MIDI BLOCKED - KEYS WAIT");
        return false;
    }
    return tempoMode_ == SmfTempoMode::Project
        ? armProjectFromTick(tick)
        : startOriginalFromTick(tick);
}

bool CardputerSmfPlayerService::startOriginalFromTick(uint32_t tick) {
    if (!loaded_ || !timing_.valid()) return false;
    if (tick > endTick_) tick = musicStartTick_;

    eventQueue_.invalidateAndRequestPanic();
    resetMidiVisual();
    projectLaunchPlanned_ = false;
    projectRelaunchAfterExternalStop_ = false;
    if (!prepareStreamAt(tick)) return false;

    uint32_t anchorBlock = 0;
    uint32_t anchorMicros = 0;
    bool haveAnchor = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (snapshotCardputerUsbMidiBlockAnchor(anchorBlock, anchorMicros)) {
            haveAnchor = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (!haveAnchor) {
        publishSnapshot(SmfPlayerState::Error, "Audio clock unavailable");
        return false;
    }

    constexpr uint32_t leadBlocks = 3;
    playbackOriginTick_ = tick;
    playbackOriginBlock_ = anchorBlock + leadBlocks;
    playbackOriginFrame_ = 0;
    const uint64_t leadMicros =
        (static_cast<uint64_t>(leadBlocks) * kBlockFrames * 1000000ull) /
        static_cast<uint64_t>(kSampleRate);
    playbackOriginMicros_ = anchorMicros + static_cast<uint32_t>(leadMicros);
    lastScheduledBlock_ = playbackOriginBlock_;
    pausedTick_ = tick;
    perfLastLogMs_ = 0;

    publishSnapshot(SmfPlayerState::Playing,
        routingMode_ == SmfRoutingMode::Raw
            ? "RAW / ORIGINAL"
            : "SEQTRAK / ORIGINAL");
    scheduleAhead();
    updatePlaybackSnapshot();
    return true;
}

bool CardputerSmfPlayerService::armProjectFromTick(uint32_t tick) {
    if (!loaded_ || !timing_.valid()) return false;
    if (tick > endTick_) tick = musicStartTick_;

    eventQueue_.invalidateAndRequestPanic();
    resetMidiVisual();
    if (!prepareStreamAt(tick)) return false;

    pausedTick_ = tick;
    projectLaunchPlanned_ = false;
    projectRelaunchAfterExternalStop_ = false;
    perfLastLogMs_ = 0;
    publishSnapshot(SmfPlayerState::Armed, "WAIT NEXT BAR");

    const ProjectTransportBlockSnapshot transport = projectTransportTimeline().snapshot();
    if (transport.valid && transport.playing) {
        planProjectLaunch(transport);
    }
    updatePlaybackSnapshot();
    return true;
}

bool CardputerSmfPlayerService::planProjectLaunch(
        const ProjectTransportBlockSnapshot& transport) {
    if (!loaded_ || !transport.valid || !transport.playing ||
        transport.blockFrames == 0 || transport.sampleRate == 0 ||
        transport.bpmX10 == 0) {
        return false;
    }

    const SmfExternalRelaunchMode externalRelaunch =
        smfExternalRelaunchMode(projectRelaunchAfterExternalStop_,
                                transport.restartedFromBeginning);
    const bool boundedExternalRelaunch =
        smfExternalRelaunchUsesBoundedPrefill(externalRelaunch);
    double targetStep = 0.0;
    if (boundedExternalRelaunch) {
        const double stepsPerBlock =
            static_cast<double>(transport.blockFrames) * transport.bpm() *
            kProjectStepsPerQuarter /
            (60.0 * static_cast<double>(transport.sampleRate));
        targetStep = transport.absoluteSteps() +
                     stepsPerBlock * kExternalRelaunchPrefillBlocks;
    } else {
        targetStep = launchMode_ == SmfLaunchMode::Immediate
            ? transport.absoluteSteps()
            : nextProjectBarStep(transport);
    }

    ProjectScheduledPosition launch{};
    if (!scheduleProjectStepAtBpm(transport.absoluteSteps(),
                                  transport.blockSequence,
                                  0,
                                  targetStep,
                                  transport.bpm(),
                                  transport.sampleRate,
                                  transport.blockFrames,
                                  launch)) {
        return false;
    }

    // Give the SD/parser task enough time to prefill before the boundary. If
    // the next bar is already too close, use the following bar rather than
    // knowingly delivering a late burst. External relaunch already uses a
    // bounded three-block prefill and must not wait for another full bar.
    if (!boundedExternalRelaunch && launchMode_ == SmfLaunchMode::NextBar &&
        static_cast<int32_t>(launch.blockSequence -
                             (transport.blockSequence + kProjectLaunchLeadBlocks)) <= 0) {
        targetStep += kProjectStepsPerBar;
        if (!scheduleProjectStepAtBpm(transport.absoluteSteps(),
                                      transport.blockSequence,
                                      0,
                                      targetStep,
                                      transport.bpm(),
                                      transport.sampleRate,
                                      transport.blockFrames,
                                      launch)) {
            return false;
        }
    }

    playbackOriginTick_ = pausedTick_;
    projectOriginSmfTick_ = static_cast<double>(pausedTick_);
    playbackOriginBlock_ = launch.blockSequence;
    playbackOriginFrame_ = launch.frameOffset;
    projectOriginStep_ = targetStep;
    projectBpmX10_ = transport.bpmX10;
    projectBpmQ16_ = transport.bpmQ16;
    projectTransportEpoch_ = transport.transportEpoch;
    lastScheduledBlock_ = playbackOriginBlock_;
    projectLaunchPlanned_ = true;
    projectRelaunchAfterExternalStop_ = false;

    publishSnapshot(SmfPlayerState::Armed,
                    externalRelaunch == SmfExternalRelaunchMode::Restart
                        ? "ARMED / RESTART"
                        : (externalRelaunch == SmfExternalRelaunchMode::Continue
                               ? "ARMED / CONTINUE"
                               : (launchMode_ == SmfLaunchMode::NextBar
                                      ? "ARMED / NEXT BAR"
                                      : "ARMED / NOW")));
    scheduleAhead();
    return true;
}

void CardputerSmfPlayerService::pauseAtCurrentPosition() {
    if (!loaded_) return;
    pausedTick_ = currentTickFromAudioClock();
    eventQueue_.invalidateAndRequestPanic();
    resetMidiVisual();
    projectLaunchPlanned_ = false;
    projectRelaunchAfterExternalStop_ = false;
    prepareStreamAt(pausedTick_);
    publishSnapshot(SmfPlayerState::Paused,
        tempoMode_ == SmfTempoMode::Project
            ? (followsSeqtrakClock()
                   ? "SEQ MASTER / PAUSED"
                   : "GP MASTER / PAUSED")
            : (routingMode_ == SmfRoutingMode::Raw
                ? "RAW / ORIGINAL"
                : "SEQTRAK / ORIGINAL"));
    updatePlaybackSnapshot();
}

void CardputerSmfPlayerService::stopAndCleanup(bool resetToMusicStart) {
    eventQueue_.invalidateAndRequestPanic();
    resetMidiVisual();
    projectLaunchPlanned_ = false;
    projectRelaunchAfterExternalStop_ = false;
    if (resetToMusicStart && loaded_) pausedTick_ = musicStartTick_;
    if (loaded_) {
        prepareStreamAt(pausedTick_);
        publishSnapshot(SmfPlayerState::Stopped,
            tempoMode_ == SmfTempoMode::Project
                ? (followsSeqtrakClock()
                       ? "SEQ MASTER / STOPPED"
                       : "GP MASTER / STOPPED")
                : (routingMode_ == SmfRoutingMode::Raw
                    ? "RAW / ORIGINAL"
                    : "SEQTRAK / ORIGINAL"));
        updatePlaybackSnapshot();
    }
}

bool CardputerSmfPlayerService::takeNextNote(SmfStreamEvent& event) {
    // Consuming events moves the stream past the prepared position.
    streamPreparedValid_ = false;
    while (true) {
        if (hasPendingEvent_) {
            event = pendingEvent_;
            hasPendingEvent_ = false;
        } else if (!stream_.next(event)) {
            streamEnded_ = true;
            return false;
        }

        if (event.event.kind == SmfEventKind::NoteOn ||
            event.event.kind == SmfEventKind::NoteOff) {
            return true;
        }
    }
}

void CardputerSmfPlayerService::scheduleAhead() {
    const SmfPlayerState playerState = snapshot().state;
    if (!loaded_ ||
        (playerState != SmfPlayerState::Playing &&
         playerState != SmfPlayerState::Armed) ||
        eventQueue_.transportFailed()) {
        return;
    }
    if (tempoMode_ == SmfTempoMode::Project && !projectLaunchPlanned_) return;

    ProjectTransportBlockSnapshot projectTransport{};
    if (tempoMode_ == SmfTempoMode::Project) {
        const ProjectTransportReadResult readResult =
            readProjectTransport(projectTransport);
        if (readResult == ProjectTransportReadResult::Unavailable) return;
        if (readResult == ProjectTransportReadResult::Stale ||
            !projectTransport.valid || !projectTransport.playing ||
            projectTransport.transportEpoch != projectTransportEpoch_) {
            pauseForStaleProjectTimeline();
            return;
        }
    }

    uint32_t anchorBlock = 0;
    uint32_t anchorMicros = 0;
    if (!snapshotCardputerUsbMidiBlockAnchor(anchorBlock, anchorMicros)) return;
    (void)anchorMicros;

    const uint32_t scheduleStarted = micros();
    ++perfScheduleCalls_;
    const uint32_t entryDepth = static_cast<uint32_t>(eventQueue_.approximateSize());
    if (entryDepth < perfMinQueueDepth_) perfMinQueueDepth_ = entryDepth;
    const uint32_t lookaheadBlocks = tempoMode_ == SmfTempoMode::Project
        ? kProjectScheduleLookaheadBlocks
        : kScheduleLookaheadBlocks;

    while (eventQueue_.approximateSize() < kQueueFillLimit) {
        SmfStreamEvent event{};
        if (hasPendingEvent_) {
            event = pendingEvent_;
        } else if (!takeNextNote(event)) {
            break;
        }

        pendingEvent_ = event;
        hasPendingEvent_ = true;

        SmfScheduledPosition position{};
        bool scheduled = false;
        if (tempoMode_ == SmfTempoMode::Project) {
            ProjectScheduledPosition projectPosition{};
            const ProjectSmfScheduleResult result = scheduleProjectSmfTick(
                projectTransport,
                event.event.kind == SmfEventKind::NoteOn
                    ? ProjectSmfLatePolicy::DropNoteOn
                    : ProjectSmfLatePolicy::DispatchNoteOffImmediately,
                fileIndex_.division,
                projectOriginSmfTick_,
                projectOriginStep_,
                playbackOriginBlock_,
                playbackOriginFrame_,
                event.event.tick,
                projectBpmX10_,
                kSampleRate,
                static_cast<uint16_t>(kBlockFrames),
                projectPosition);
            if (result == ProjectSmfScheduleResult::DroppedLateNoteOn) {
                hasPendingEvent_ = false;
                ++perfProjectLateNoteOnDrops_;
                continue;
            }
            scheduled = result == ProjectSmfScheduleResult::Scheduled;
            position.blockSequence = projectPosition.blockSequence;
            position.frameOffset = projectPosition.frameOffset;
        } else {
            scheduled = scheduleSmfTick(timing_,
                                        playbackOriginTick_,
                                        playbackOriginBlock_,
                                        event.event.tick,
                                        kSampleRate,
                                        static_cast<uint16_t>(kBlockFrames),
                                        position,
                                        tempoScalePermille_);
        }
        if (!scheduled) {
            publishSnapshot(SmfPlayerState::Error, "Schedule conversion failed");
            eventQueue_.invalidateAndRequestPanic();
            return;
        }

        if (static_cast<int32_t>(position.blockSequence -
                                 (anchorBlock + lookaheadBlocks)) > 0) {
            break;
        }

        bool pushed = false;
        const SmfRoutedNote routed = routeSmfNote(
            routingMode_, event.event.channel, event.event.data1);
        if (!routed.mapped) {
            hasPendingEvent_ = false;
            ++perfUnmappedEventsFiltered_;
            continue;
        }
        uint8_t visualVelocity = 0;
        if (event.event.kind == SmfEventKind::NoteOn) {
            visualVelocity = applySmfVelocityBoost(event.event.data2, velocityBoost_);
            pushed = eventQueue_.tryPushNoteOn(
                routed.channel,
                routed.note,
                visualVelocity,
                position.blockSequence,
                position.frameOffset,
                tempoMode_ == SmfTempoMode::Project
                    ? projectTransport.transportEpoch
                    : 0u);
        } else {
            pushed = eventQueue_.tryPushNoteOff(
                routed.channel,
                routed.note,
                event.event.data2,
                position.blockSequence,
                position.frameOffset,
                tempoMode_ == SmfTempoMode::Project
                    ? projectTransport.transportEpoch
                    : 0u);
        }

        if (!pushed) {
            if (eventQueue_.transportFailed()) return;
            if (event.event.kind == SmfEventKind::NoteOff) {
                publishSnapshot(SmfPlayerState::Error, "MIDI cleanup overflow");
            }
            break;
        }
        if (event.event.kind == SmfEventKind::NoteOn) {
            queueMidiVisualNote(event.event.tick, routed.note, visualVelocity, routed.channel);
        }
        lastScheduledBlock_ = position.blockSequence;
        hasPendingEvent_ = false;
        ++perfQueuedEvents_;
    }

    const uint32_t scheduleMicros = micros() - scheduleStarted;
    if (scheduleMicros > perfMaxScheduleMicros_) {
        perfMaxScheduleMicros_ = scheduleMicros;
    }

    if (streamEnded_ && !hasPendingEvent_ &&
        static_cast<int32_t>(anchorBlock - lastScheduledBlock_) > 1) {
        pausedTick_ = musicStartTick_;
        projectLaunchPlanned_ = false;
        projectRelaunchAfterExternalStop_ = false;
        publishSnapshot(SmfPlayerState::Stopped, "END - Space to replay");
    }
}

void CardputerSmfPlayerService::logPerformance() {
    const uint32_t nowMs = millis();
    if (perfLastLogMs_ != 0 && nowMs - perfLastLogMs_ < kPerfLogIntervalMs) return;
    const uint32_t windowMs = perfLastLogMs_ == 0 ? 0 : nowMs - perfLastLogMs_;
    perfLastLogMs_ = nowMs;
    if (windowMs == 0) {
        source_.resetStats();
        perfScheduleCalls_ = 0;
        perfQueuedEvents_ = 0;
        perfUnmappedEventsFiltered_ = 0;
        perfMaxScheduleMicros_ = 0;
        perfMinQueueDepth_ = kPerfUnsetDepth;
        perfProjectLateNoteOnDrops_ = 0;
        perfTimelineReadMisses_ = 0;
        perfTimelineStalePauses_ = 0;
        perfTempoReanchors_ = 0;
        return;
    }

    const SdByteSource::Stats sd = source_.stats();
    SmfPlayerPerformanceSnapshot performance{};
    performance.trackCount = fileIndex_.trackCount;
    performance.cacheBytesPerTrack = static_cast<uint16_t>(
        std::min<uint32_t>(stream_.trackCacheBytes(),
                           std::numeric_limits<uint16_t>::max()));
    performance.reads = sd.reads;
    performance.seeks = sd.seeks;
    performance.bytes = sd.bytes;
    performance.maxReadMicros = sd.maxReadMicros;
    performance.scheduleCalls = perfScheduleCalls_;
    performance.queuedEvents = perfQueuedEvents_;
    performance.maxScheduleMicros = perfMaxScheduleMicros_;
    performance.minQueueDepth = perfMinQueueDepth_ == kPerfUnsetDepth
        ? -1
        : static_cast<int16_t>(std::min<uint32_t>(
              perfMinQueueDepth_, std::numeric_limits<int16_t>::max()));
    performance.queueFillLimit = static_cast<uint16_t>(kQueueFillLimit);
    const uint32_t lookaheadBlocks = tempoMode_ == SmfTempoMode::Project
        ? kProjectScheduleLookaheadBlocks
        : kScheduleLookaheadBlocks;
    performance.lookaheadMs = static_cast<uint16_t>(
        (lookaheadBlocks * kBlockFrames * 1000u) / kSampleRate);
    performance.unmappedEventsFiltered = perfUnmappedEventsFiltered_;
    performance.projectLateNoteOnDrops = perfProjectLateNoteOnDrops_;
    performance.timelineReadMisses = perfTimelineReadMisses_;
    performance.timelineStalePauses = perfTimelineStalePauses_;
    performance.tempoReanchors = perfTempoReanchors_;

    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.performance = performance;
    portEXIT_CRITICAL(&snapshotMux_);

    Serial.printf(
        "[SMF-PERF] tracks=%u cache=%u reads=%u seeks=%u bytes=%u "
        "maxReadUs=%u sched=%u queued=%u maxSchedUs=%u minQueue=%d fill=%u "
        "lookahead=%ums filtered=%u projectLateDrop=%u timelineMiss=%u "
        "timelineStale=%u reanchor=%u mode=%s\n",
        static_cast<unsigned>(performance.trackCount),
        static_cast<unsigned>(performance.cacheBytesPerTrack),
        static_cast<unsigned>(performance.reads),
        static_cast<unsigned>(performance.seeks),
        static_cast<unsigned>(performance.bytes),
        static_cast<unsigned>(performance.maxReadMicros),
        static_cast<unsigned>(performance.scheduleCalls),
        static_cast<unsigned>(performance.queuedEvents),
        static_cast<unsigned>(performance.maxScheduleMicros),
        static_cast<int>(performance.minQueueDepth),
        static_cast<unsigned>(performance.queueFillLimit),
        static_cast<unsigned>(performance.lookaheadMs),
        static_cast<unsigned>(performance.unmappedEventsFiltered),
        static_cast<unsigned>(performance.projectLateNoteOnDrops),
        static_cast<unsigned>(performance.timelineReadMisses),
        static_cast<unsigned>(performance.timelineStalePauses),
        static_cast<unsigned>(performance.tempoReanchors),
        smfTempoModeName(tempoMode_));

    source_.resetStats();
    perfScheduleCalls_ = 0;
    perfQueuedEvents_ = 0;
    perfUnmappedEventsFiltered_ = 0;
    perfMaxScheduleMicros_ = 0;
    perfMinQueueDepth_ = kPerfUnsetDepth;
    perfProjectLateNoteOnDrops_ = 0;
    perfTimelineReadMisses_ = 0;
    perfTimelineStalePauses_ = 0;
    perfTempoReanchors_ = 0;
}

double CardputerSmfPlayerService::currentProjectSmfTick(
        const ProjectTransportBlockSnapshot& transport) const {
    if (!loaded_ || fileIndex_.division == 0 || !projectLaunchPlanned_ ||
        !transport.valid) {
        return static_cast<double>(pausedTick_);
    }
    return projectSmfTickAtStep(fileIndex_.division,
                                projectOriginSmfTick_,
                                projectOriginStep_,
                                transport.absoluteSteps(),
                                endTick_);
}

uint32_t CardputerSmfPlayerService::currentProjectTick(
        const ProjectTransportBlockSnapshot& transport) const {
    return static_cast<uint32_t>(std::floor(currentProjectSmfTick(transport)));
}

uint32_t CardputerSmfPlayerService::currentTickFromAudioClock() {
    if (!loaded_ || !timing_.valid()) return 0;
    if (tempoMode_ == SmfTempoMode::Project) {
        ProjectTransportBlockSnapshot transport{};
        if (!projectTransportTimeline().trySnapshot(transport) ||
            !transport.valid || !transport.playing) {
            if (haveLastProjectTransport_ && lastProjectTransport_.playing) {
                return currentProjectTick(lastProjectTransport_);
            }
            return snapshot().currentTick;
        }
        lastProjectTransport_ = transport;
        haveLastProjectTransport_ = true;
        return currentProjectTick(transport);
    }

    const uint32_t now = micros();
    const int32_t elapsed = static_cast<int32_t>(now - playbackOriginMicros_);
    if (elapsed <= 0) return playbackOriginTick_;
    const uint64_t fileMicros = timing_.tickToMicros(playbackOriginTick_) +
        scaleSmfFileMicros(static_cast<uint32_t>(elapsed), tempoScalePermille_);
    return std::min(timing_.microsToTick(fileMicros), endTick_);
}

void CardputerSmfPlayerService::applyTempoScale(uint16_t scalePermille) {
    tempoScalePermille_ = scalePermille == 0
        ? kSmfOriginalTempoScalePermille
        : scalePermille;
    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.tempoScalePermille = tempoScalePermille_;
    portEXIT_CRITICAL(&snapshotMux_);
}

uint16_t CardputerSmfPlayerService::originalBpmX10At(uint32_t tick) const {
    const uint32_t mpq = timing_.microsPerQuarterAtTick(tick);
    return mpq > 0
        ? static_cast<uint16_t>(std::min<uint32_t>(65535, 600000000u / mpq))
        : 1200;
}

uint16_t CardputerSmfPlayerService::effectiveBpmX10At(uint32_t tick) const {
    if (tempoMode_ == SmfTempoMode::Project) {
        const ProjectTransportBlockSnapshot transport = projectTransportTimeline().snapshot();
        if (transport.valid && transport.bpmX10 > 0) return transport.bpmX10;
        return projectBpmX10_;
    }
    const uint32_t scaled =
        (static_cast<uint32_t>(originalBpmX10At(tick)) * tempoScalePermille_ +
         kSmfOriginalTempoScalePermille / 2u) /
        kSmfOriginalTempoScalePermille;
    return static_cast<uint16_t>(std::min<uint32_t>(65535, scaled));
}


void CardputerSmfPlayerService::resetMidiVisual() {
    midiVisualTimeline_.reset();
    const SmfMidiVisualSnapshot visual = midiVisualTimeline_.snapshot();
    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.midiVisual = visual;
    portEXIT_CRITICAL(&snapshotMux_);
}

void CardputerSmfPlayerService::queueMidiVisualNote(
        uint32_t tick, uint8_t note, uint8_t velocity, uint8_t channel) {
    midiVisualTimeline_.queue(tick, note, velocity, channel);
}

void CardputerSmfPlayerService::updatePlaybackSnapshot() {
    if (!loaded_ || !timing_.valid()) return;
    const SmfPlayerState state = snapshot().state;
    uint32_t tick = pausedTick_;
    if (state == SmfPlayerState::Playing) {
        tick = currentTickFromAudioClock();
    }
    const SmfBarBeat pos = timing_.barBeatForTick(tick);
    const uint16_t originalBpmX10 = originalBpmX10At(tick);
    const uint16_t bpmX10 = effectiveBpmX10At(tick);
    const SmfMidiVisualSnapshot midiVisual = midiVisualTimeline_.advanceTo(tick);

    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.currentTick = tick;
    snapshot_.bar = pos.bar;
    snapshot_.beat = pos.beat;
    snapshot_.originalBpmX10 = originalBpmX10;
    snapshot_.bpmX10 = bpmX10;
    snapshot_.tempoScalePermille = tempoScalePermille_;
    snapshot_.velocityBoost = velocityBoost_;
    snapshot_.tempoMode = tempoMode_;
    snapshot_.launchMode = launchMode_;
    snapshot_.midiVisual = midiVisual;
    portEXIT_CRITICAL(&snapshotMux_);
}

void CardputerSmfPlayerService::publishSnapshot(SmfPlayerState state,
                                                 const char* message) {
    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.state = state;
    snapshot_.tempoMode = tempoMode_;
    snapshot_.launchMode = launchMode_;
    if (message) copyText(snapshot_.message, sizeof(snapshot_.message), message);
    portEXIT_CRITICAL(&snapshotMux_);
}

void CardputerSmfPlayerService::copyText(char* dst,
                                         std::size_t size,
                                         const char* src) {
    if (!dst || size == 0) return;
    if (!src) src = "";
    std::snprintf(dst, size, "%s", src);
}

const char* CardputerSmfPlayerService::basename(const char* path) {
    if (!path) return "";
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}
