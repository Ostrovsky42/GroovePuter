#include "tape_looper.h"
#include <cstring>
#include <algorithm>
#include <esp_heap_caps.h>

TapeLooper::TapeLooper() {
    // Initial state: no buffer
}

TapeLooper::~TapeLooper() {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }
}

bool TapeLooper::init(uint32_t maxSeconds) {
    if (buffer_) free(buffer_);
    
    maxSamples_ = maxSeconds * kSampleRate;
    size_t size = maxSamples_ * sizeof(int16_t);

    // Attempt PSRAM first, then DRAM
    buffer_ = (int16_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer_) {
        buffer_ = (int16_t*)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
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
    pos_ = 0;
    recording_ = false;
    playing_ = false;
    overdub_ = false;
    firstRecord_ = false;
}

void TapeLooper::process(float input, float* loopPart) {
    if (!buffer_) {
        *loopPart = 0.0f;
        return;
    }

    float out = 0.0f;
    
    // Playback
    if (playing_ && length_ > 0) {
        out = (float)buffer_[pos_] / 32768.0f;
    }

    // Recording / Overdubbing
    if (recording_) {
        if (firstRecord_) {
            // Define loop length on the fly
            buffer_[pos_] = (int16_t)(input * 32767.0f);
            pos_++;
            if (pos_ >= maxSamples_) {
                length_ = maxSamples_;
                firstRecord_ = false;
                recording_ = false; // auto-stop at limit
                pos_ = 0;
            }
        } else if (length_ > 0) {
            // Overdub: Add to existing
            float existing = (float)buffer_[pos_] / 32768.0f;
            float mixed = existing + input;
            // Simple soft-clip for overdub
            if (mixed > 1.0f) mixed = 1.0f;
            else if (mixed < -1.0f) mixed = -1.0f;
            
            buffer_[pos_] = (int16_t)(mixed * 32767.0f);
        }
    }

    // Advance position if playing or first-recording
    if ((playing_ && length_ > 0) || (recording_ && firstRecord_)) {
        if (!firstRecord_) {
            pos_++;
            if (pos_ >= length_) pos_ = 0;
        }
    }

    // If recording was just stopped, set length
    if (firstRecord_ && !recording_) {
        length_ = pos_;
        pos_ = 0;
        firstRecord_ = false;
    }

    *loopPart = out * volume_;
}
