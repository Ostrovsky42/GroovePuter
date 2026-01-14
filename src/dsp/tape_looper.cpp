#include "tape_looper.h"
#include "tape_presets.h"
#include <cstring>
#include <algorithm>
#include <cmath>

// Platform-specific memory allocation
#if defined(ESP_PLATFORM) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define LOOPER_MALLOC_PSRAM(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LOOPER_MALLOC_DRAM(size) heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#else
#define LOOPER_MALLOC_PSRAM(size) malloc(size)
#define LOOPER_MALLOC_DRAM(size) malloc(size)
#endif

TapeLooper::TapeLooper() {
    // Initial state: no buffer allocated
}

TapeLooper::~TapeLooper() {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }
}

bool TapeLooper::init(uint32_t maxSeconds) {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }
    
    maxSamples_ = maxSeconds * kSampleRate;
    size_t size = maxSamples_ * sizeof(int16_t);

    // Try PSRAM first, then DRAM
    buffer_ = static_cast<int16_t*>(LOOPER_MALLOC_PSRAM(size));
    if (!buffer_) {
        buffer_ = static_cast<int16_t*>(LOOPER_MALLOC_DRAM(size));
    }
    
    if (buffer_) {
        clear();
        return true;
    }
    
    maxSamples_ = 0;
    return false;
}

void TapeLooper::clear() {
    if (buffer_ && maxSamples_ > 0) {
        std::memset(buffer_, 0, maxSamples_ * sizeof(int16_t));
    }
    length_ = 0;
    playhead_ = 0;
    firstRecord_ = false;
    stutterActive_ = false;
}

void TapeLooper::eject() {
    clear();
    mode_ = TapeMode::Stop;
    speed_ = 1;
    speedMultiplier_ = 1.0f;
    volume_ = 1.0f;
}

void TapeLooper::setMode(TapeMode mode) {
    TapeMode oldMode = mode_;
    mode_ = mode;
    
    // Handle mode transitions
    if (mode == TapeMode::Rec && oldMode != TapeMode::Rec) {
        // Starting fresh recording
        if (length_ == 0) {
            firstRecord_ = true;
            playhead_ = 0;
        }
    } else if (mode == TapeMode::Dub && oldMode != TapeMode::Dub) {
        // Starting overdub
        if (length_ == 0) {
            // Can't overdub without existing loop - switch to rec
            mode_ = TapeMode::Rec;
            firstRecord_ = true;
            playhead_ = 0;
        }
    } else if (mode == TapeMode::Play && oldMode == TapeMode::Rec && firstRecord_) {
        // Ending first recording - set loop length
        length_ = static_cast<uint32_t>(playhead_);
        if (length_ < 100) length_ = 0; // Too short, discard
        playhead_ = 0;
        firstRecord_ = false;
    } else if (mode == TapeMode::Stop) {
        if (oldMode == TapeMode::Rec && firstRecord_) {
            // Stopping during first record - set loop length
            length_ = static_cast<uint32_t>(playhead_);
            if (length_ < 100) length_ = 0;
            firstRecord_ = false;
        }
        playhead_ = 0;
    }
}

void TapeLooper::setSpeed(uint8_t speed) {
    speed_ = std::min(speed, (uint8_t)2);
    speedMultiplier_ = tapeSpeedMultiplier(speed_);
}

void TapeLooper::setStutter(bool active) {
    if (active && !stutterActive_) {
        // Starting stutter - remember current position
        stutterStart_ = playhead_;
    }
    stutterActive_ = active;
}

float TapeLooper::playheadProgress() const {
    if (length_ == 0) {
        if (firstRecord_ && maxSamples_ > 0) {
            return playhead_ / static_cast<float>(maxSamples_);
        }
        return 0.0f;
    }
    return playhead_ / static_cast<float>(length_);
}

float TapeLooper::loopLengthSeconds() const {
    return static_cast<float>(length_) / static_cast<float>(kSampleRate);
}

float TapeLooper::readInterpolated(float pos) const {
    if (!buffer_ || maxSamples_ == 0) return 0.0f;
    
    // Handle negative positions (can happen with stutter)
    while (pos < 0) pos += static_cast<float>(maxSamples_);
    
    uint32_t maxIdx = (length_ > 0) ? length_ : maxSamples_;
    
    uint32_t idx0 = static_cast<uint32_t>(pos) % maxIdx;
    uint32_t idx1 = (idx0 + 1) % maxIdx;
    float frac = pos - floorf(pos);
    
    float s0 = buffer_[idx0] / 32768.0f;
    float s1 = buffer_[idx1] / 32768.0f;
    
    return s0 + frac * (s1 - s0); // Linear interpolation
}

void TapeLooper::writeSample(uint32_t pos, float value) {
    if (!buffer_ || pos >= maxSamples_) return;
    
    // Soft clip before writing
    if (value > 1.0f) value = 1.0f;
    else if (value < -1.0f) value = -1.0f;
    
    buffer_[pos] = static_cast<int16_t>(value * 32767.0f);
}

void TapeLooper::process(float input, float* loopPart) {
    if (!buffer_) {
        *loopPart = 0.0f;
        return;
    }

    float out = 0.0f;
    
    // Handle stutter: constrain playhead to small window
    float effectivePlayhead = playhead_;
    if (stutterActive_ && length_ > 0) {
        float stutterWindow = static_cast<float>(kStutterFrames);
        float offset = fmodf(playhead_ - stutterStart_, stutterWindow);
        if (offset < 0) offset += stutterWindow;
        effectivePlayhead = stutterStart_ + offset;
        
        // Keep stutter window within loop bounds
        while (effectivePlayhead >= static_cast<float>(length_)) {
            effectivePlayhead -= static_cast<float>(length_);
        }
    }

    // Playback
    if ((mode_ == TapeMode::Play || mode_ == TapeMode::Dub) && length_ > 0) {
        out = readInterpolated(effectivePlayhead);
    }

    // Recording
    if (mode_ == TapeMode::Rec) {
        if (firstRecord_) {
            // First recording - define loop length
            uint32_t writePos = static_cast<uint32_t>(playhead_);
            if (writePos < maxSamples_) {
                writeSample(writePos, input);
            }
        } else if (length_ > 0) {
            // Punch-in recording (replace)
            uint32_t writePos = static_cast<uint32_t>(playhead_) % length_;
            writeSample(writePos, input);
        }
    }
    
    // Overdub
    if (mode_ == TapeMode::Dub && length_ > 0) {
        uint32_t writePos = static_cast<uint32_t>(playhead_) % length_;
        float existing = buffer_[writePos] / 32768.0f;
        float mixed = existing * 0.9f + input * 0.7f; // Feedback + input
        writeSample(writePos, mixed);
    }

    // Advance playhead
    if (mode_ != TapeMode::Stop) {
        if (firstRecord_) {
            // During first recording, advance at 1x regardless of speed
            playhead_ += 1.0f;
            if (playhead_ >= static_cast<float>(maxSamples_)) {
                // Hit max length - finalize loop
                length_ = maxSamples_;
                firstRecord_ = false;
                mode_ = TapeMode::Play;
                playhead_ = 0;
            }
        } else if (length_ > 0) {
            // Normal playback - use speed multiplier
            playhead_ += speedMultiplier_;
            while (playhead_ >= static_cast<float>(length_)) {
                playhead_ -= static_cast<float>(length_);
            }
            while (playhead_ < 0) {
                playhead_ += static_cast<float>(length_);
            }
        }
    }

    *loopPart = out * volume_;
}
