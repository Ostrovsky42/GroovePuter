#pragma once
#ifndef GROOVEPUTER_SMF_STRUCTURAL_INSPECTOR_H
#define GROOVEPUTER_SMF_STRUCTURAL_INSPECTOR_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "smf_document.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfStructuralMaxLayers = 9;
constexpr uint16_t kSmfStructuralBarLimit = 64;

// Stage 1B deliberately analyzes only the first nine audible physical tracks.
// The source track index remains the identity; conductor/meta tracks never
// consume a layer slot because a layer is created only after its first Note On.
enum class SmfStructuralRole : uint8_t {
    Other = 0,
    Drums,
    Bass,
    Chords,
    Pad,
    Lead,
};

enum class SmfStructuralMotion : uint8_t {
    Low = 0,
    Medium,
    High,
};

inline const char* smfStructuralRoleName(SmfStructuralRole role) {
    switch (role) {
        case SmfStructuralRole::Drums: return "DRUMS";
        case SmfStructuralRole::Bass: return "BASS";
        case SmfStructuralRole::Chords: return "CHORDS";
        case SmfStructuralRole::Pad: return "PAD";
        case SmfStructuralRole::Lead: return "LEAD";
        case SmfStructuralRole::Other:
        default: return "OTHER";
    }
}

inline const char* smfStructuralMotionName(SmfStructuralMotion motion) {
    switch (motion) {
        case SmfStructuralMotion::High: return "HIGH";
        case SmfStructuralMotion::Medium: return "MED";
        case SmfStructuralMotion::Low:
        default: return "LOW";
    }
}

struct SmfStructuralLayerSnapshot {
    uint16_t trackIndex{0};
    uint16_t channelMask{0};
    uint16_t notesPerBarX10{0};
    uint16_t activePermille{0};
    uint8_t gridDenominator{0};
    uint8_t loopBars{0};
    uint8_t swingPercent{50};
    uint8_t minNote{0};
    uint8_t maxNote{0};
    uint8_t maxPolyphony{0};
    uint8_t overlapChordsPercent{0};
    uint8_t overlapLeadPercent{0};
    SmfStructuralRole role{SmfStructuralRole::Other};
    SmfStructuralMotion motion{SmfStructuralMotion::Low};
    uint8_t form[4]{};

    bool hasNotes() const { return notesPerBarX10 != 0; }
};

struct SmfStructuralInspectorSnapshot {
    uint16_t sourceTrackCount{0};
    uint16_t analyzedBars{0};
    uint8_t layerCount{0};
    bool partial{false};
    SmfStructuralLayerSnapshot layers[kSmfStructuralMaxLayers]{};
};

class SmfStructuralInspectorState {
public:
    void reset(uint16_t division, uint16_t sourceTrackCount) {
        sequence_.fetch_add(1u, std::memory_order_acq_rel);
        division_ = division == 0 ? 96u : division;
        sourceTrackCount_ = sourceTrackCount;
        ticksPerBar_ = static_cast<uint32_t>(division_) * 4u;
        maxObservedTick_ = 0;
        layerCount_ = 0;
        partial_ = false;
        finalized_ = false;
        for (std::size_t i = 0; i < kSmfStructuralMaxLayers; ++i) {
            storage_.accum[i] = LayerAccumulator{};
        }
        publishedLayerCount_ = 0;
        publishedAnalyzedBars_ = 0;
        publishedPartial_ = false;
        sequence_.fetch_add(1u, std::memory_order_release);
    }

    void observe(uint16_t trackIndex, const SmfEvent& event) {
        if (finalized_) return;
        if (event.tick > maxObservedTick_) maxObservedTick_ = event.tick;
        const uint32_t limitTick = ticksPerBar_ * kSmfStructuralBarLimit;
        if (event.tick >= limitTick) {
            partial_ = true;
            return;
        }
        if (event.kind == SmfEventKind::TimeSignature && event.data1 > 0u) {
            const uint8_t denominatorPow2 = event.data2 > 5u ? 5u : event.data2;
            const uint32_t denominator = uint32_t{1} << denominatorPow2;
            const uint32_t quarterNotesX4 =
                (static_cast<uint32_t>(event.data1) * 16u) / denominator;
            if (quarterNotesX4 > 0u) {
                ticksPerBar_ = (static_cast<uint32_t>(division_) * quarterNotesX4) / 4u;
                if (ticksPerBar_ == 0u) ticksPerBar_ = static_cast<uint32_t>(division_) * 4u;
            }
            return;
        }
        if (event.kind != SmfEventKind::NoteOn &&
            event.kind != SmfEventKind::NoteOff) {
            return;
        }

        int layer = findLayer(trackIndex);
        if (layer < 0) {
            if (event.kind != SmfEventKind::NoteOn) return;
            layer = insertLayer(trackIndex);
            if (layer < 0) return;
        }

        LayerAccumulator& item = storage_.accum[static_cast<std::size_t>(layer)];
        advanceActive(item, event.tick);
        item.channelMask |= static_cast<uint16_t>(uint16_t{1} << (event.channel & 0x0Fu));
        if (event.kind == SmfEventKind::NoteOn) observeNoteOn(item, event);
        else observeNoteOff(item);
    }

    void finalize() {
        if (finalized_) return;
        finalized_ = true;
        const uint32_t boundedEnd = maxObservedTick_ < ticksPerBar_ * kSmfStructuralBarLimit
            ? maxObservedTick_
            : ticksPerBar_ * kSmfStructuralBarLimit;
        for (uint8_t i = 0; i < layerCount_; ++i) {
            advanceActive(storage_.accum[i], boundedEnd);
        }

        SmfStructuralLayerSnapshot computed[kSmfStructuralMaxLayers]{};
        uint16_t analyzedBars = boundedEnd == 0u
            ? 0u
            : static_cast<uint16_t>((boundedEnd / ticksPerBar_) + 1u);
        if (analyzedBars > kSmfStructuralBarLimit) analyzedBars = kSmfStructuralBarLimit;
        const bool publishedPartial = partial_ ||
            maxObservedTick_ >= ticksPerBar_ * kSmfStructuralBarLimit;

        for (uint8_t i = 0; i < layerCount_; ++i) {
            const LayerAccumulator& source = storage_.accum[i];
            SmfStructuralLayerSnapshot& output = computed[i];
            output.trackIndex = source.trackIndex;
            output.channelMask = source.channelMask;
            output.minNote = source.hasNotes ? source.minNote : 0u;
            output.maxNote = source.hasNotes ? source.maxNote : 0u;
            output.maxPolyphony = source.maxPolyphony;
            output.notesPerBarX10 = analyzedBars == 0u
                ? 0u
                : static_cast<uint16_t>((source.noteCount * 10u) / analyzedBars);
            output.activePermille = boundedEnd == 0u
                ? 0u
                : static_cast<uint16_t>((source.activeTicks * 1000u) / boundedEnd);
            if (output.activePermille > 1000u) output.activePermille = 1000u;
            output.gridDenominator = inferGrid(source);
            output.loopBars = inferLoop(source, analyzedBars);
            output.swingPercent = source.swingSamples == 0u
                ? 50u
                : static_cast<uint8_t>(source.swingPercentSum / source.swingSamples);
            output.motion = inferMotion(source, analyzedBars);

            uint16_t formMax = 1u;
            for (uint8_t bin = 0; bin < 4u; ++bin) {
                if (source.formNotes[bin] > formMax) formMax = source.formNotes[bin];
            }
            for (uint8_t bin = 0; bin < 4u; ++bin) {
                output.form[bin] = static_cast<uint8_t>(
                    (source.formNotes[bin] * 8u) / formMax);
            }
        }

        SmfStructuralInspectorSnapshot roleSnapshot{};
        roleSnapshot.sourceTrackCount = sourceTrackCount_;
        roleSnapshot.analyzedBars = analyzedBars;
        roleSnapshot.layerCount = layerCount_;
        roleSnapshot.partial = publishedPartial;
        for (uint8_t i = 0; i < layerCount_; ++i) roleSnapshot.layers[i] = computed[i];
        assignRolesAndOverlap(roleSnapshot);

        sequence_.fetch_add(1u, std::memory_order_acq_rel);
        for (uint8_t i = 0; i < layerCount_; ++i) {
            storage_.published[i] = roleSnapshot.layers[i];
        }
        publishedLayerCount_ = layerCount_;
        publishedAnalyzedBars_ = analyzedBars;
        publishedPartial_ = publishedPartial;
        sequence_.fetch_add(1u, std::memory_order_release);
    }

    SmfStructuralInspectorSnapshot snapshot() const {
        SmfStructuralInspectorSnapshot result{};
        while (true) {
            const uint32_t before = sequence_.load(std::memory_order_acquire);
            if ((before & 1u) != 0u) continue;
            result.sourceTrackCount = sourceTrackCount_;
            result.analyzedBars = publishedAnalyzedBars_;
            result.layerCount = publishedLayerCount_;
            result.partial = publishedPartial_;
            for (uint8_t i = 0; i < result.layerCount; ++i) {
                result.layers[i] = storage_.published[i];
            }
            const uint32_t after = sequence_.load(std::memory_order_acquire);
            if (before == after) return result;
        }
    }

private:
    struct LayerAccumulator {
        uint16_t trackIndex{0};
        uint16_t channelMask{0};
        uint16_t noteCount{0};
        uint16_t activeCount{0};
        uint16_t maxPolyphony{0};
        uint32_t lastTick{0};
        uint32_t activeTicks{0};
        uint32_t gridHits8{0};
        uint32_t gridHits16{0};
        uint32_t gridHits32{0};
        uint32_t swingPercentSum{0};
        uint16_t swingSamples{0};
        uint8_t minNote{127};
        uint8_t maxNote{0};
        bool hasNotes{false};
        uint16_t formNotes[4]{};
        uint32_t barHash[4]{};
    };

    int findLayer(uint16_t trackIndex) const {
        for (uint8_t i = 0; i < layerCount_; ++i) {
            if (storage_.accum[i].trackIndex == trackIndex) return i;
        }
        return -1;
    }

    int insertLayer(uint16_t trackIndex) {
        if (layerCount_ == kSmfStructuralMaxLayers) {
            if (trackIndex >= storage_.accum[layerCount_ - 1u].trackIndex) return -1;
            --layerCount_;
        }
        uint8_t insert = layerCount_;
        while (insert > 0u && storage_.accum[insert - 1u].trackIndex > trackIndex) {
            storage_.accum[insert] = storage_.accum[insert - 1u];
            --insert;
        }
        storage_.accum[insert] = LayerAccumulator{};
        storage_.accum[insert].trackIndex = trackIndex;
        ++layerCount_;
        return insert;
    }

    void advanceActive(LayerAccumulator& item, uint32_t tick) {
        if (tick > item.lastTick && item.activeCount > 0u) {
            item.activeTicks += tick - item.lastTick;
        }
        if (tick > item.lastTick) item.lastTick = tick;
    }

    void observeNoteOn(LayerAccumulator& item, const SmfEvent& event) {
        ++item.noteCount;
        ++item.activeCount;
        if (item.activeCount > item.maxPolyphony) item.maxPolyphony = item.activeCount;
        item.hasNotes = true;
        if (event.data1 < item.minNote) item.minNote = event.data1;
        if (event.data1 > item.maxNote) item.maxNote = event.data1;

        const uint32_t bar = event.tick / ticksPerBar_;
        if (bar < kSmfStructuralBarLimit) {
            const uint8_t formBin = static_cast<uint8_t>(bar / 16u);
            if (formBin < 4u && item.formNotes[formBin] < 65535u) ++item.formNotes[formBin];
            if (bar < 4u) {
                const uint32_t position = event.tick % ticksPerBar_;
                item.barHash[bar] = item.barHash[bar] * 16777619u ^
                    (position + static_cast<uint32_t>(event.data1) * 131u + event.data2);
            }
        }

        const uint32_t position = event.tick % ticksPerBar_;
        observeGrid(item, position, 8u, item.gridHits8);
        observeGrid(item, position, 16u, item.gridHits16);
        observeGrid(item, position, 32u, item.gridHits32);

        const uint32_t eighth = ticksPerBar_ / 8u;
        if (eighth > 0u) {
            const uint32_t phase = position % (eighth * 2u);
            if (phase >= eighth / 2u && phase <= (eighth * 3u) / 2u) {
                const uint32_t percent = (phase * 100u) / (eighth * 2u);
                if (percent >= 45u && percent <= 75u) {
                    item.swingPercentSum += percent;
                    ++item.swingSamples;
                }
            }
        }
    }

    static void observeNoteOff(LayerAccumulator& item) {
        if (item.activeCount > 0u) --item.activeCount;
    }

    void observeGrid(LayerAccumulator& item,
                     uint32_t position,
                     uint8_t denominator,
                     uint32_t& hits) const {
        const uint32_t stepsPerBar = static_cast<uint32_t>(denominator);
        const uint32_t step = ticksPerBar_ / stepsPerBar;
        if (step == 0u) return;
        const uint32_t remainder = position % step;
        const uint32_t distance = remainder < step - remainder ? remainder : step - remainder;
        if (distance * 12u <= step) ++hits;
        (void)item;
    }

    static uint8_t inferGrid(const LayerAccumulator& item) {
        if (item.noteCount == 0u) return 0u;
        const uint32_t threshold = (static_cast<uint32_t>(item.noteCount) * 3u) / 4u;
        if (item.gridHits8 >= threshold) return 8u;
        if (item.gridHits16 >= threshold) return 16u;
        if (item.gridHits32 >= threshold) return 32u;
        return 0u;
    }

    static uint8_t inferLoop(const LayerAccumulator& item, uint16_t bars) {
        if (bars >= 2u && item.barHash[0] != 0u && item.barHash[0] == item.barHash[1]) return 1u;
        if (bars >= 4u && item.barHash[0] != 0u && item.barHash[1] != 0u &&
            item.barHash[0] == item.barHash[2] && item.barHash[1] == item.barHash[3]) return 2u;
        return bars >= 4u ? 4u : (bars == 0u ? 0u : static_cast<uint8_t>(bars));
    }

    static SmfStructuralMotion inferMotion(const LayerAccumulator& item, uint16_t bars) {
        if (bars <= 1u) return SmfStructuralMotion::Low;
        uint8_t changed = 0;
        const uint8_t compared = bars >= 4u ? 4u : static_cast<uint8_t>(bars);
        for (uint8_t i = 1u; i < compared; ++i) {
            if (item.barHash[i] != item.barHash[i - 1u]) ++changed;
        }
        if (changed == 0u) return SmfStructuralMotion::Low;
        if (changed + 1u >= compared) return SmfStructuralMotion::High;
        return SmfStructuralMotion::Medium;
    }

    static uint8_t registerOverlapPercent(const SmfStructuralLayerSnapshot& a,
                                          const SmfStructuralLayerSnapshot& b) {
        if (!a.hasNotes() || !b.hasNotes()) return 0u;
        const int low = a.minNote > b.minNote ? a.minNote : b.minNote;
        const int high = a.maxNote < b.maxNote ? a.maxNote : b.maxNote;
        if (high < low) return 0u;
        const int aSpan = static_cast<int>(a.maxNote) - a.minNote + 1;
        const int overlap = high - low + 1;
        return static_cast<uint8_t>((overlap * 100) / (aSpan > 0 ? aSpan : 1));
    }

    static void assignRolesAndOverlap(SmfStructuralInspectorSnapshot& snapshot) {
        int chords = -1;
        int lead = -1;
        for (uint8_t i = 0; i < snapshot.layerCount; ++i) {
            SmfStructuralLayerSnapshot& layer = snapshot.layers[i];
            const bool drums = (layer.channelMask & (uint16_t{1} << 9u)) != 0u;
            const uint8_t median = static_cast<uint8_t>((layer.minNote + layer.maxNote) / 2u);
            if (drums) layer.role = SmfStructuralRole::Drums;
            else if (median < 48u && layer.maxPolyphony <= 2u) layer.role = SmfStructuralRole::Bass;
            else if (layer.maxPolyphony >= 3u && layer.activePermille >= 700u) layer.role = SmfStructuralRole::Pad;
            else if (layer.maxPolyphony >= 3u) layer.role = SmfStructuralRole::Chords;
            else if (median >= 60u) layer.role = SmfStructuralRole::Lead;
            else layer.role = SmfStructuralRole::Other;

            if (chords < 0 && (layer.role == SmfStructuralRole::Chords ||
                               layer.role == SmfStructuralRole::Pad)) chords = i;
            if (lead < 0 && layer.role == SmfStructuralRole::Lead) lead = i;
        }
        for (uint8_t i = 0; i < snapshot.layerCount; ++i) {
            if (chords >= 0 && chords != i) {
                snapshot.layers[i].overlapChordsPercent = registerOverlapPercent(
                    snapshot.layers[i], snapshot.layers[static_cast<uint8_t>(chords)]);
            }
            if (lead >= 0 && lead != i) {
                snapshot.layers[i].overlapLeadPercent = registerOverlapPercent(
                    snapshot.layers[i], snapshot.layers[static_cast<uint8_t>(lead)]);
            }
        }
    }

    mutable std::atomic<uint32_t> sequence_{0};
    uint16_t division_{96};
    uint16_t sourceTrackCount_{0};
    uint32_t ticksPerBar_{384};
    uint32_t maxObservedTick_{0};
    uint8_t layerCount_{0};
    bool partial_{false};
    bool finalized_{false};
    union Storage {
        LayerAccumulator accum[kSmfStructuralMaxLayers];
        SmfStructuralLayerSnapshot published[kSmfStructuralMaxLayers];
        constexpr Storage() : accum{} {}
    } storage_{};
    uint16_t publishedAnalyzedBars_{0};
    uint8_t publishedLayerCount_{0};
    bool publishedPartial_{false};
};

static_assert(sizeof(SmfStructuralInspectorState) <= 640,
              "Stage 1B structural analysis must remain bounded for Cardputer ADV DRAM");

inline SmfStructuralInspectorState& smfStructuralInspectorState() {
    static SmfStructuralInspectorState state;
    return state;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_STRUCTURAL_INSPECTOR_H
