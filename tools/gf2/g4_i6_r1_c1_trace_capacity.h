#ifndef GROOVEPUTER_TOOLS_GF2_G4_I6_R1_C1_TRACE_CAPACITY_H
#define GROOVEPUTER_TOOLS_GF2_G4_I6_R1_C1_TRACE_CAPACITY_H

#include <cstdint>

// Analysis-only mirror of the production-private bound in
// generation_profile.cpp.  Keeping this outside src/** avoids widening the
// runtime API only for evidence instrumentation.
inline constexpr uint8_t kMaxWeightedCandidates = 16;

#endif  // GROOVEPUTER_TOOLS_GF2_G4_I6_R1_C1_TRACE_CAPACITY_H
