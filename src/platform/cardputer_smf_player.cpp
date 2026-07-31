#include "cardputer_smf_player.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "src/audio/audio_config.h"
#include "src/platform/cardputer_usb_midi_service.h"

using namespace GroovePuterMidi;

namespace {
constexpr uint32_t kPlayerTaskStack = 6144;
constexpr UBaseType_t kPlayerTaskPriority = 1;
constexpr BaseType_t kPlayerTaskCore = 0;
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(2);
}

CardputerSmfPlayerService::CardputerSmfPlayerService() {
    snapshot_.state = SmfPlayerState::Unloaded;
    snapshot_.rawRouting = true;
    copyText(snapshot_.message, sizeof(snapshot_.message), "Select a MIDI file");
}

bool CardputerSmfPlayerService::begin() {
    if (taskHandle_ != nullptr) return true;
    commandQueue_ = xQueueCreateStatic(
        kCommandDepth,
        sizeof(Command),
        commandQueueStorage_,
        &commandQueueStruct_);
    if (commandQueue_ == nullptr) return false;

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
        return false;
    }
    return true;
}

bool CardputerSmfPlayerService::requestLoadAndPlay(const char* path) {
    if (!path || !path[0]) return false;
    Command command{};
    command.type = CommandType::LoadAndPlay;
    copyText(command.path, sizeof(command.path), path);
    return enqueue(command);
}

bool CardputerSmfPlayerService::togglePlayPause() {
    Command command{};
    command.type = CommandType::TogglePlayPause;
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

SmfPlayerSnapshot CardputerSmfPlayerService::snapshot() const {
    portENTER_CRITICAL(&snapshotMux_);
    const SmfPlayerSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&snapshotMux_);
    return copy;
}

bool CardputerSmfPlayerService::SdByteSource::open(const char* path) {
    close();
    file_ = SD.open(path, FILE_READ);
    return static_cast<bool>(file_);
}

void CardputerSmfPlayerService::SdByteSource::close() {
    if (file_) file_.close();
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
    if (!file_.seek(offset)) return false;
    return file_.read(dst, length) == static_cast<int>(length);
}

void CardputerSmfPlayerService::taskEntry(void* context) {
    static_cast<CardputerSmfPlayerService*>(context)->taskLoop();
}

void CardputerSmfPlayerService::taskLoop() {
    while (true) {
        Command command{};
        while (commandQueue_ != nullptr &&
               xQueueReceive(commandQueue_, &command, 0) == pdTRUE) {
            handleCommand(command);
        }

        SmfPlayerState state;
        {
            portENTER_CRITICAL(&snapshotMux_);
            state = snapshot_.state;
            portEXIT_CRITICAL(&snapshotMux_);
        }
        if (state == SmfPlayerState::Playing) {
            scheduleAhead();
            updatePlaybackSnapshot();
        }
        vTaskDelay(kIdleDelay);
    }
}

bool CardputerSmfPlayerService::enqueue(const Command& command) {
    return commandQueue_ != nullptr &&
           xQueueSend(commandQueue_, &command, 0) == pdTRUE;
}

void CardputerSmfPlayerService::handleCommand(const Command& command) {
    switch (command.type) {
        case CommandType::LoadAndPlay:
            publishSnapshot(SmfPlayerState::Loading, "Scanning MIDI...");
            if (loadFile(command.path)) {
                startFromTick(musicStartTick_);
            }
            break;
        case CommandType::TogglePlayPause: {
            const SmfPlayerState state = snapshot().state;
            if (!loaded_) break;
            if (state == SmfPlayerState::Playing) {
                pauseAtCurrentPosition();
            } else if (state == SmfPlayerState::Paused) {
                startFromTick(pausedTick_);
            } else {
                startFromTick(musicStartTick_);
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
            if (previous == SmfPlayerState::Playing) {
                startFromTick(targetTick);
            } else {
                pausedTick_ = targetTick;
                prepareStreamAt(targetTick);
                publishSnapshot(SmfPlayerState::Paused, "SEEK");
                updatePlaybackSnapshot();
            }
            break;
        }
    }
}

bool CardputerSmfPlayerService::loadFile(const char* path) {
    stopAndCleanup(false);
    source_.close();
    loaded_ = false;
    timingDocument_.events.clear();

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

    if (!stream_.open(source_, fileIndex_)) {
        publishSnapshot(SmfPlayerState::Error, "Stream init failed");
        source_.close();
        return false;
    }

    timingDocument_ = SmfDocument{};
    timingDocument_.format = fileIndex_.format;
    timingDocument_.division = fileIndex_.division;
    timingDocument_.events.reserve(kMaxTimingEvents);
    musicStartTick_ = 0;
    endTick_ = 0;
    bool foundMusic = false;

    SmfStreamEvent event{};
    while (stream_.next(event)) {
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

    stream_.reset();
    loaded_ = true;
    pausedTick_ = musicStartTick_;
    hasPendingEvent_ = false;
    streamEnded_ = false;

    portENTER_CRITICAL(&snapshotMux_);
    copyText(snapshot_.filename, sizeof(snapshot_.filename), basename(path));
    snapshot_.endTick = endTick_;
    snapshot_.totalBars = timing_.barBeatForTick(endTick_).bar;
    snapshot_.rawRouting = true;
    portEXIT_CRITICAL(&snapshotMux_);

    publishSnapshot(SmfPlayerState::Stopped, "RAW / ORIGINAL");
    return true;
}

bool CardputerSmfPlayerService::scanMetadata() {
    return loaded_ && timing_.valid();
}

bool CardputerSmfPlayerService::prepareStreamAt(uint32_t tick) {
    if (!loaded_) return false;
    stream_.reset();
    hasPendingEvent_ = false;
    streamEnded_ = false;

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
    if (!loaded_ || !timing_.valid()) return false;
    if (tick > endTick_) tick = musicStartTick_;

    eventQueue_.invalidateAndRequestPanic();
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
    const uint64_t leadMicros =
        (static_cast<uint64_t>(leadBlocks) * kBlockFrames * 1000000ull) /
        static_cast<uint64_t>(kSampleRate);
    playbackOriginMicros_ = anchorMicros + static_cast<uint32_t>(leadMicros);
    lastScheduledBlock_ = playbackOriginBlock_;
    pausedTick_ = tick;

    publishSnapshot(SmfPlayerState::Playing, "RAW / ORIGINAL");
    scheduleAhead();
    updatePlaybackSnapshot();
    return true;
}

void CardputerSmfPlayerService::pauseAtCurrentPosition() {
    if (!loaded_) return;
    pausedTick_ = currentTickFromAudioClock();
    eventQueue_.invalidateAndRequestPanic();
    prepareStreamAt(pausedTick_);
    publishSnapshot(SmfPlayerState::Paused, "RAW / ORIGINAL");
    updatePlaybackSnapshot();
}

void CardputerSmfPlayerService::stopAndCleanup(bool resetToMusicStart) {
    eventQueue_.invalidateAndRequestPanic();
    if (resetToMusicStart && loaded_) pausedTick_ = musicStartTick_;
    if (loaded_) {
        prepareStreamAt(pausedTick_);
        publishSnapshot(SmfPlayerState::Stopped, "RAW / ORIGINAL");
        updatePlaybackSnapshot();
    }
}

bool CardputerSmfPlayerService::takeNextNote(SmfStreamEvent& event) {
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
    if (!loaded_ || snapshot().state != SmfPlayerState::Playing) return;

    uint32_t anchorBlock = 0;
    uint32_t anchorMicros = 0;
    if (!snapshotCardputerUsbMidiBlockAnchor(anchorBlock, anchorMicros)) return;
    (void)anchorMicros;

    while (eventQueue_.approximateSize() < kQueueFillLimit) {
        SmfStreamEvent event{};
        if (hasPendingEvent_) {
            event = pendingEvent_;
        } else if (!takeNextNote(event)) {
            break;
        }

        // takeNextNote consumes its returned event. Keep it pending until the
        // queue accepts it or until it enters the active lookahead window.
        pendingEvent_ = event;
        hasPendingEvent_ = true;

        SmfScheduledPosition position{};
        if (!scheduleSmfTick(timing_,
                             playbackOriginTick_,
                             playbackOriginBlock_,
                             event.event.tick,
                             kSampleRate,
                             static_cast<uint16_t>(kBlockFrames),
                             position)) {
            publishSnapshot(SmfPlayerState::Error, "Schedule conversion failed");
            eventQueue_.invalidateAndRequestPanic();
            return;
        }

        if (static_cast<int32_t>(position.blockSequence -
                                 (anchorBlock + kScheduleLookaheadBlocks)) > 0) {
            break;
        }

        bool pushed = false;
        if (event.event.kind == SmfEventKind::NoteOn) {
            pushed = eventQueue_.tryPushNoteOn(
                event.event.channel,
                event.event.data1,
                event.event.data2,
                position.blockSequence,
                position.frameOffset);
        } else {
            pushed = eventQueue_.tryPushNoteOff(
                event.event.channel,
                event.event.data1,
                event.event.data2,
                position.blockSequence,
                position.frameOffset);
        }

        if (!pushed) {
            if (event.event.kind == SmfEventKind::NoteOff) {
                publishSnapshot(SmfPlayerState::Error, "MIDI cleanup overflow");
            }
            break;
        }
        lastScheduledBlock_ = position.blockSequence;
        hasPendingEvent_ = false;
    }

    if (streamEnded_ && !hasPendingEvent_ &&
        static_cast<int32_t>(anchorBlock - lastScheduledBlock_) > 1) {
        pausedTick_ = musicStartTick_;
        publishSnapshot(SmfPlayerState::Stopped, "END - Space to replay");
    }
}

uint32_t CardputerSmfPlayerService::currentTickFromAudioClock() const {
    if (!loaded_ || !timing_.valid()) return 0;
    const uint32_t now = micros();
    const int32_t elapsed = static_cast<int32_t>(now - playbackOriginMicros_);
    if (elapsed <= 0) return playbackOriginTick_;
    const uint64_t fileMicros = timing_.tickToMicros(playbackOriginTick_) +
                                static_cast<uint32_t>(elapsed);
    return std::min(timing_.microsToTick(fileMicros), endTick_);
}

void CardputerSmfPlayerService::updatePlaybackSnapshot() {
    if (!loaded_ || !timing_.valid()) return;
    const SmfPlayerState state = snapshot().state;
    uint32_t tick = pausedTick_;
    if (state == SmfPlayerState::Playing) {
        tick = currentTickFromAudioClock();
    }
    const SmfBarBeat pos = timing_.barBeatForTick(tick);
    const uint32_t mpq = timing_.microsPerQuarterAtTick(tick);
    const uint16_t bpmX10 = mpq > 0
        ? static_cast<uint16_t>(std::min<uint32_t>(65535, 600000000u / mpq))
        : 1200;

    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.currentTick = tick;
    snapshot_.bar = pos.bar;
    snapshot_.beat = pos.beat;
    snapshot_.bpmX10 = bpmX10;
    portEXIT_CRITICAL(&snapshotMux_);
}

void CardputerSmfPlayerService::publishSnapshot(SmfPlayerState state,
                                                 const char* message) {
    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.state = state;
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
