#pragma once
#include <cstdint>

/**
 * PerfStats: Lock-free snapshot using sequence number pattern.
 * 
 * USAGE (audio task writes):
 *   stats.seq++; // odd = writing
 *   stats.audioUnderruns = ...;
 *   stats.cpuAudioPctIdeal = ...;
 *   stats.cpuAudioPctActual = ...;
 *   stats.seq++; // even = consistent
 * 
 * USAGE (UI reads):
 *   uint32_t s1, s2;
 *   do {
 *     s1 = stats.seq;
 *     values = read stats fields;
 *     s2 = stats.seq;
 *   } while (s1 != s2 || (s1 & 1));  // retry if torn read or mid-write
 */
struct PerfStats {
  volatile uint32_t seq = 0;           // Sequence number (even = valid snapshot)
  volatile uint32_t audioUnderruns = 0;
  volatile float cpuAudioPctIdeal = 0.0f;    // DSP time / ideal period
  volatile float cpuAudioPctActual = 0.0f;   // DSP time / measured period
  volatile uint32_t heapFree = 0;
  volatile uint32_t heapMinFree = 0;
  volatile uint32_t lastCallbackMicros = 0;  // For measuring actual period
};

