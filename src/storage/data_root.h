#ifndef GROOVEPUTER_SRC_STORAGE_DATA_ROOT_H
#define GROOVEPUTER_SRC_STORAGE_DATA_ROOT_H

// Single source of the SD data root. Persistent GroovePuter folders live under
// it. The firmware reads and writes only here; moving older data from the card
// root is a manual step on the computer.
//
// The macro keeps path literals compile-time constants:
//   GROOVEPUTER_DATA_ROOT "/scenes"  ->  "/grooveputer/scenes"

#define GROOVEPUTER_DATA_ROOT "/grooveputer"

#include <string>

namespace GroovePuterStorage {

inline constexpr char kDataRoot[] = GROOVEPUTER_DATA_ROOT;
inline constexpr char kScenesDirectory[] = GROOVEPUTER_DATA_ROOT "/scenes";
inline constexpr char kPatternsDirectory[] = GROOVEPUTER_DATA_ROOT "/patterns";
inline constexpr char kProjectsDirectory[] = GROOVEPUTER_DATA_ROOT "/projects";
inline constexpr char kMidiDirectory[] = GROOVEPUTER_DATA_ROOT "/midi";
inline constexpr char kTiltSettingsFile[] =
    GROOVEPUTER_DATA_ROOT "/grooveputer_tilt.txt";

// Runtime join for a path below the data root: dataPath("/scenes") is
// "/grooveputer/scenes".
inline std::string dataPath(const std::string& relative) {
  return std::string(kDataRoot) + relative;
}

}  // namespace GroovePuterStorage

#endif  // GROOVEPUTER_SRC_STORAGE_DATA_ROOT_H
