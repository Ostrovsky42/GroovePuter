#pragma once

// Fallback identity for direct Arduino IDE builds. scripts/build.sh replaces
// this file in its staged source tree with the exact version and source SHA.
namespace GroovePuterBuild {
inline constexpr char kVersion[] = "0.0.0-dev";
inline constexpr char kSourceCommit[] = "unknown";
inline constexpr bool kSourceDirty = true;
}  // namespace GroovePuterBuild
