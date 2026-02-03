#pragma once
#include <cstdint>
#include <cmath>

// Audio diagnostics for tracking peaks, clipping, and mix sources
// Usage: call diagAcc() in audio loop, diagFlushIfReady() periodically

struct AudioDiag {
  float peakPre = 0.0f;    // Peak before limiter
  float peakPost = 0.0f;   // Peak after limiter
  uint32_t clipPre = 0;    // Clips before limiter (|x|>1.0)
  uint32_t clickCount = 0; // Large sample-to-sample jumps (discontinuities)
  float mean = 0.0f;       // DC offset
  uint32_t nanCount = 0;   // NaN/Inf detections
  uint32_t frames = 0;     // Sample count
  
  // Per-source peaks
  float peak303 = 0.0f;
  float peakDrums = 0.0f;
  float peakSampler = 0.0f;
  float peakDelay = 0.0f;
  float peakLooper = 0.0f;
  float peakTapeFX = 0.0f;
};

class AudioDiagnostics {
public:
  static AudioDiagnostics& instance() {
    static AudioDiagnostics inst;
    return inst;
  }
  
  // Call once per sample in audio loop
  inline void accumulate(float preLimiter, float postLimiter) {
    float absPre = fabsf(preLimiter);
    if (absPre > diag_.peakPre) diag_.peakPre = absPre;
    if (absPre > 1.0f) diag_.clipPre++;
    
    float absPost = fabsf(postLimiter);
    if (absPost > diag_.peakPost) diag_.peakPost = absPost;
    
    if (!std::isfinite(preLimiter) || !std::isfinite(postLimiter)) {
      diag_.nanCount++;
    }
    
    // Click detection: large sample-to-sample jumps
    float diff = preLimiter - prevSample_;
    if (fabsf(diff) > 0.5f) {
      diag_.clickCount++;
    }
    prevSample_ = preLimiter;
    
    diag_.mean += preLimiter;
    diag_.frames++;
  }
  
  // Track individual source peaks
  inline void trackSource(float val303, float valDrums, float valSampler, 
                          float valDelay, float valLooper, float valTapeFX) {
    peakUp(diag_.peak303, val303);
    peakUp(diag_.peakDrums, valDrums);
    peakUp(diag_.peakSampler, valSampler);
    peakUp(diag_.peakDelay, valDelay);
    peakUp(diag_.peakLooper, valLooper);
    peakUp(diag_.peakTapeFX, valTapeFX);
  }
  
  // Call periodically (e.g. every 250ms) to print stats
  void flushIfReady(uint32_t currentMillis) {
    if (currentMillis - lastFlush_ < 250) return; // 4x per second
    lastFlush_ = currentMillis;
    
    if (diag_.frames == 0) return;
    
    float mean = diag_.mean / static_cast<float>(diag_.frames);
    
    Serial.printf("[AUD] pre:%.3f clip:%u clk:%u post:%.3f dc:%.4f nan:%u | 303:%.2f dr:%.2f smp:%.2f dly:%.2f lp:%.2f fx:%.2f\n",
                  diag_.peakPre, 
                  static_cast<unsigned>(diag_.clipPre),
                  static_cast<unsigned>(diag_.clickCount),
                  diag_.peakPost,
                  mean,
                  static_cast<unsigned>(diag_.nanCount),
                  diag_.peak303,
                  diag_.peakDrums,
                  diag_.peakSampler,
                  diag_.peakDelay,
                  diag_.peakLooper,
                  diag_.peakTapeFX);
    
    // Reset for next period
    diag_ = AudioDiag{};
  }
  
  void enable(bool enabled) { enabled_ = enabled; }
  bool isEnabled() const { return enabled_; }
  
private:
  AudioDiagnostics() = default;
  
  inline void peakUp(float& peak, float val) {
    float absVal = fabsf(val);
    if (absVal > peak) peak = absVal;
  }
  
  AudioDiag diag_;
  float prevSample_ = 0.0f;  // For click detection
  uint32_t lastFlush_ = 0;
  bool enabled_ = false;  // Disable by default for performance
};
