#pragma once
#ifndef GROOVEPUTER_SMF_PLAYER_DIRTY_H
#define GROOVEPUTER_SMF_PLAYER_DIRTY_H

#include <cstdint>

namespace GroovePuterUi {

enum class SmfPlayerDirty : uint16_t {
    None = 0,
    Header = 1u << 0,
    Progress = 1u << 1,
    Transport = 1u << 2,
    Track = 1u << 3,
    Browser = 1u << 4,
    Overlay = 1u << 5,
    Footer = 1u << 6,
    Full = 1u << 15,
};

inline constexpr SmfPlayerDirty operator|(SmfPlayerDirty lhs,
                                          SmfPlayerDirty rhs) {
    return static_cast<SmfPlayerDirty>(
        static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

inline constexpr SmfPlayerDirty operator&(SmfPlayerDirty lhs,
                                          SmfPlayerDirty rhs) {
    return static_cast<SmfPlayerDirty>(
        static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs));
}

class SmfPlayerDirtyState {
public:
    void invalidate(SmfPlayerDirty flags) {
        flags_ = flags_ | flags;
    }

    bool any(SmfPlayerDirty flags) const {
        return static_cast<uint16_t>(flags_ & flags) != 0u;
    }

    SmfPlayerDirty take() {
        const SmfPlayerDirty result = flags_;
        flags_ = SmfPlayerDirty::None;
        return result;
    }

    void invalidateFull() {
        flags_ = SmfPlayerDirty::Full;
    }

private:
    SmfPlayerDirty flags_{SmfPlayerDirty::Full};
};

}  // namespace GroovePuterUi

#endif  // GROOVEPUTER_SMF_PLAYER_DIRTY_H
