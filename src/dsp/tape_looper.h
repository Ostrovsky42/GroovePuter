#pragma once
#include <cstdint>
#include "../audio/audio_config.h"
#include "../../scenes.h"

// TapeLooper is a mono ring-buffer looper. Storage capacity is selected by
// init() on supported targets; the current Cardputer ADV product policy keeps
// the sample buffer unallocated so Tape cannot consume sampler/runtime DRAM.
//
// Modes:
// - STOP: No recording or playback
// - REC:  Record input to loop buffer (defines loop length)
// - DUB:  Overdub input onto existing loop
// - PLAY: Playback loop only
//
// Features:
// - Speed control (0.5x, 1.0x, 2.0x) with linear interpolation
// - Stutter effect (playhead freeze in small window)
// - Eject (full reset to clean state)

class TapeLooper {
public:
    // Implementation ceiling used by supported allocation profiles.
    // This is not the current Cardputer ADV allocation policy.
    static constexpr uint32_t kMaxSeconds = 2;
    static constexpr uint32_t kMaxSamples = kMaxSeconds * kSampleRate;
    static constexpr uint32_t kStutterFrames = 512; // ~23ms @ 22kHz
    static constexpr uint32_t kCrossfadeFrames = 256;

    TapeLooper();
    ~TapeLooper();

    // Initialize looper with dynamic memory
    // Attempts to allocate 'maxSeconds' long buffer on supported targets.
    // Priority: PSRAM, fallback: DRAM.
    // Cardputer ADV currently has an explicit no-storage product policy.
    // Returns true only when a usable sample buffer exists.
    bool init(float maxSeconds);
    bool storageReady() const { return buffer_ != nullptr && maxSamples_ > 0; }

    // Mode control (call with AudioGuard from UI thread!).
    // Invariant: without usable storage every requested mode resolves to STOP.
    void setMode(TapeMode mode);
    TapeMode mode() const { return mode_; }
    
    // Speed control: 0=0.5x, 1=1.0x, 2=2.0x
    void setSpeed(uint8_t speed);
    uint8_t speed() const { return speed_; }
    
    // Stutter: hold to freeze playhead in small loop
    void setStutter(bool active);
    bool stutterActive() const { return stutterActive_; }

    // Safety overdub: when enabled, DUB auto-returns to PLAY after one loop cycle.
    void setDubAutoExit(bool enabled) { dubAutoExit_ = storageReady() && enabled; }
    bool dubAutoExit() const { return dubAutoExit_; }
    
    // Eject: full reset to clean state
    void eject();
    
    // Clear loop buffer only (keep settings)
    void clear();

    // Volume control
    void setVolume(float v) { volume_ = v; }
    float volume() const { return volume_; }

    // Status getters for UI
    float playheadProgress() const;  // 0.0..1.0
    float loopLengthSeconds() const;
    bool hasLoop() const { return storageReady() && length_ > 0; }
    bool isFirstRecordPass() const { return mode_ == TapeMode::Rec && firstRecord_; }
    float recordElapsedSeconds() const { return static_cast<float>(playheadSamples()) / static_cast<float>(kSampleRate); }
    uint32_t loopLengthSamples() const { return length_; }
    uint32_t playheadSamples() const { return static_cast<uint32_t>(playhead_); }
    float getPeak() { float p = peak_; peak_ = 0; return p; } // logically read-and-clear

    // Process a single sample
    // input: signal to be potentially recorded
    // loopPart: output of looper to be mixed into final signal
    void process(float input, float* loopPart);

private:
    int16_t* buffer_ = nullptr;
    uint32_t maxSamples_ = 0;
    uint32_t length_ = 0;         // Current loop length in samples
    float playhead_ = 0;          // Float for interpolated playback
    
    TapeMode mode_ = TapeMode::Stop;
    uint8_t speed_ = 1;           // 0=0.5x, 1=1.0x, 2=2.0x
    float speedMultiplier_ = 1.0f;
    
    bool stutterActive_ = false;
    float stutterStart_ = 0;
    bool dubAutoExit_ = false;
    
    float volume_ = 1.0f;
    bool firstRecord_ = false;
    float peak_ = 0.0f;

    void forceStoppedWithoutStorage_();

    // Read sample with linear interpolation (for speed changes)
    float readInterpolated(float pos) const;

    // Write sample to buffer (with soft clipping for overdub)
    void writeSample(uint32_t pos, float value);

    // Pre-bake crossfade at loop boundary to eliminate clicks
    void bakeLoopCrossfade();
};
