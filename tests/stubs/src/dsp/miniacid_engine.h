#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

struct TestSampleId {
    uint32_t value{0};
};

struct TestSamplerPad {
    TestSampleId id{};
    bool loop{false};
};

class TestSampleStore {};

class TestSamplerTrack {
public:
    bool isEnabled() const { return false; }

    TestSamplerPad& pad(uint8_t lane) {
        return pads_[static_cast<std::size_t>(lane)];
    }

    const TestSamplerPad& pad(uint8_t lane) const {
        return pads_[static_cast<std::size_t>(lane)];
    }

    void triggerPad(uint8_t lane, float velocity, TestSampleStore& store) {
        (void)lane;
        (void)velocity;
        (void)store;
    }

    void stopPad(uint8_t lane) { (void)lane; }

private:
    std::array<TestSamplerPad, 8> pads_{};
};

class MiniAcid {
public:
    static constexpr uint8_t kMin303Note = 24;
    static constexpr uint8_t kMax303Note = 96;

    enum class CallType : uint8_t {
        NoteOn,
        NoteOff,
        SuspendLiveProjection,
    };

    struct Call {
        CallType type{CallType::NoteOn};
        int voice{0};
        uint8_t note{0};
        uint8_t velocity{0};
    };

    TestSampleStore* sampleStore{nullptr};
    TestSamplerTrack* samplerTrack{nullptr};
    std::array<bool, 2> patternOwned{{false, false}};
    std::array<int, 2> liveNote{{-1, -1}};
    std::vector<Call> calls{};

    bool patternOwnsInternalSynth(int voice) const {
        return patternOwned[static_cast<std::size_t>(voice)];
    }

    void liveNoteOn(int voice, uint8_t note, uint8_t velocity) {
        liveNote[static_cast<std::size_t>(voice)] = static_cast<int>(note);
        calls.push_back(Call{CallType::NoteOn, voice, note, velocity});
    }

    void liveNoteOff(int voice, uint8_t note) {
        liveNote[static_cast<std::size_t>(voice)] = -1;
        calls.push_back(Call{CallType::NoteOff, voice, note, 0});
    }

    void suspendLiveNoteProjection(int voice) {
        liveNote[static_cast<std::size_t>(voice)] = -1;
        calls.push_back(Call{CallType::SuspendLiveProjection, voice, 0, 0});
    }
};

inline bool triggerRegisteredLocalDrumVoice(uint8_t logicalVoice,
                                            uint8_t velocity) {
    (void)logicalVoice;
    (void)velocity;
    return true;
}
