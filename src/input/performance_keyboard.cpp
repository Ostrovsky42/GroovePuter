#include "performance_keyboard.h"

#include <cctype>

namespace {
constexpr uint8_t kChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr uint8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint8_t kNaturalMinor[] = {0, 2, 3, 5, 7, 8, 10};
constexpr uint8_t kMinorPentatonic[] = {0, 3, 5, 7, 10};
constexpr uint8_t kDorian[] = {0, 2, 3, 5, 7, 9, 10};

struct ScaleDefinition {
    const uint8_t* intervals;
    uint8_t count;
    const char* name;
};

constexpr ScaleDefinition kScales[] = {
    {kChromatic, static_cast<uint8_t>(sizeof(kChromatic)), "CHROMATIC"},
    {kMajor, static_cast<uint8_t>(sizeof(kMajor)), "MAJOR"},
    {kNaturalMinor, static_cast<uint8_t>(sizeof(kNaturalMinor)), "NAT MINOR"},
    {kMinorPentatonic, static_cast<uint8_t>(sizeof(kMinorPentatonic)), "MIN PENTA"},
    {kDorian, static_cast<uint8_t>(sizeof(kDorian)), "DORIAN"},
};

constexpr char kLowerRow[] = "asdfghjkl";
constexpr char kUpperRow[] = "qwertyuiop";
}  // namespace

char PerformanceKeyboard::normalizeKey(char key) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
}

bool PerformanceKeyboard::containsKey(const char* keys,
                                      std::size_t count,
                                      char key) {
    if (!keys) return false;
    key = normalizeKey(key);
    for (std::size_t i = 0; i < count; ++i) {
        if (normalizeKey(keys[i]) == key) return true;
    }
    return false;
}

bool PerformanceKeyboard::scaleDegreeForKey(char physicalKey, uint8_t& degree) {
    physicalKey = normalizeKey(physicalKey);
    for (uint8_t i = 0; i < sizeof(kLowerRow) - 1; ++i) {
        if (kLowerRow[i] == physicalKey) {
            degree = i;
            return true;
        }
    }
    for (uint8_t i = 0; i < sizeof(kUpperRow) - 1; ++i) {
        if (kUpperRow[i] == physicalKey) {
            // The upper manual starts one scale degree above the lower manual.
            // Its maximum degree stays bounded so every supported scale remains
            // inside MiniAcid's 24..71 note range at every allowed octave.
            degree = static_cast<uint8_t>(i + 1);
            return true;
        }
    }
    return false;
}

uint8_t PerformanceKeyboard::intervalForDegree(PerformanceScale scale,
                                               uint8_t degree) {
    const uint8_t scaleIndex = static_cast<uint8_t>(scale);
    if (scaleIndex >= static_cast<uint8_t>(PerformanceScale::Count)) return 0;
    const ScaleDefinition& definition = kScales[scaleIndex];
    const uint8_t octave = static_cast<uint8_t>(degree / definition.count);
    const uint8_t index = static_cast<uint8_t>(degree % definition.count);
    return static_cast<uint8_t>(octave * 12 + definition.intervals[index]);
}

bool PerformanceKeyboard::noteForKey(char physicalKey, uint8_t& note) const {
    uint8_t degree = 0;
    if (!scaleDegreeForKey(physicalKey, degree)) return false;

    const int value = static_cast<int>(kRootC3) + octaveShift_ * 12 +
                      intervalForDegree(scale_, degree);
    if (value < kMinNote || value > kMaxNote) return false;
    note = static_cast<uint8_t>(value);
    return true;
}

int PerformanceKeyboard::findHeld(char physicalKey) const {
    physicalKey = normalizeKey(physicalKey);
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i].physicalKey == physicalKey) return static_cast<int>(i);
    }
    return -1;
}

void PerformanceKeyboard::emitNoteOn(const HeldNote& held) {
    router_.route(MusicalEvent{
        MusicalEventType::NoteOn,
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        0,
        held.note,
        held.velocity,
    });
}

void PerformanceKeyboard::emitNoteOff(uint8_t note) {
    router_.route(MusicalEvent{
        MusicalEventType::NoteOff,
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        0,
        note,
        0,
    });
}

void PerformanceKeyboard::emitAllNotesOff() {
    router_.route(MusicalEvent{
        MusicalEventType::AllNotesOff,
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        0,
        0,
        0,
    });
}

bool PerformanceKeyboard::keyDown(char physicalKey, uint8_t velocity) {
    if (!liveInputAllowed()) return false;

    physicalKey = normalizeKey(physicalKey);
    uint8_t note = 0;
    if (!noteForKey(physicalKey, note)) return false;

    // Matrix repeats must not grow the stack or retrigger the voice.
    if (findHeld(physicalKey) >= 0) return true;
    if (heldCount_ >= kMaxHeldNotes) {
        panic();
        return false;
    }

    held_[heldCount_++] = HeldNote{physicalKey, note, velocity};
    emitNoteOn(held_[heldCount_ - 1]);
    return true;
}

bool PerformanceKeyboard::keyUp(char physicalKey) {
    physicalKey = normalizeKey(physicalKey);
    const int found = findHeld(physicalKey);
    if (found < 0) return false;

    const std::size_t index = static_cast<std::size_t>(found);
    const bool wasActive = index + 1 == heldCount_;
    const uint8_t releasedNote = held_[index].note;
    for (std::size_t i = index + 1; i < heldCount_; ++i) {
        held_[i - 1] = held_[i];
    }
    held_[--heldCount_] = HeldNote{};

    if (!wasActive) return true;
    if (heldCount_ > 0) emitNoteOn(held_[heldCount_ - 1]);
    else emitNoteOff(releasedNote);
    return true;
}

void PerformanceKeyboard::releaseMissingKeys(const char* pressedKeys,
                                             std::size_t pressedCount) {
    if (heldCount_ == 0) return;

    const char oldActiveKey = held_[heldCount_ - 1].physicalKey;
    const uint8_t oldActiveNote = held_[heldCount_ - 1].note;
    bool changed = false;
    std::size_t write = 0;
    for (std::size_t read = 0; read < heldCount_; ++read) {
        if (containsKey(pressedKeys, pressedCount, held_[read].physicalKey)) {
            held_[write++] = held_[read];
        } else {
            changed = true;
        }
    }
    for (std::size_t i = write; i < heldCount_; ++i) held_[i] = HeldNote{};
    heldCount_ = write;

    if (!changed || containsKey(pressedKeys, pressedCount, oldActiveKey)) return;
    if (heldCount_ > 0) emitNoteOn(held_[heldCount_ - 1]);
    else emitNoteOff(oldActiveNote);
}

void PerformanceKeyboard::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    if (!enabled) panic();
    enabled_ = enabled;
}

void PerformanceKeyboard::setTransportPlaying(bool playing) {
    if (transportPlaying_ == playing) return;
    if (playing) panic();
    transportPlaying_ = playing;
}

void PerformanceKeyboard::panic() {
    for (std::size_t i = 0; i < heldCount_; ++i) held_[i] = HeldNote{};
    heldCount_ = 0;
    emitAllNotesOff();
}

void PerformanceKeyboard::setScale(PerformanceScale scale) {
    if (scale >= PerformanceScale::Count || scale_ == scale) return;
    panic();
    scale_ = scale;
}

void PerformanceKeyboard::cycleScale(int direction) {
    int next = static_cast<int>(scale_) + direction;
    const int count = static_cast<int>(PerformanceScale::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    setScale(static_cast<PerformanceScale>(next));
}

const char* PerformanceKeyboard::scaleName() const {
    const uint8_t index = static_cast<uint8_t>(scale_);
    if (index >= static_cast<uint8_t>(PerformanceScale::Count)) return "UNKNOWN";
    return kScales[index].name;
}

bool PerformanceKeyboard::shiftOctave(int direction) {
    int next = octaveShift_ + direction;
    if (next < kMinOctaveShift) next = kMinOctaveShift;
    if (next > kMaxOctaveShift) next = kMaxOctaveShift;
    if (next == octaveShift_) return false;
    panic();
    octaveShift_ = static_cast<int8_t>(next);
    return true;
}

int PerformanceKeyboard::activeNote() const {
    return heldCount_ == 0 ? -1 : held_[heldCount_ - 1].note;
}
