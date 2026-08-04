#pragma once
#ifndef GROOVEPUTER_SMF_STRUCTURAL_INSPECTOR_H
#define GROOVEPUTER_SMF_STRUCTURAL_INSPECTOR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "smf_document.h"
#include "smf_track_inspector.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfStructuralMaxLayers = 9;
constexpr uint16_t kSmfStructuralBarLimit = 64;

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
        beginPublish();
        division_ = division == 0u ? 96u : division;
        sourceTrackCount_ = sourceTrackCount > 127u ? 127u : sourceTrackCount;
        ticksPerBar_ = static_cast<uint32_t>(division_) * 4u;
        maxObservedTick_ = 0u;
        layerCount_ = 0u;
        partial_ = false;
        finalized_ = false;
        for (std::size_t i = 0; i < kSmfStructuralMaxLayers; ++i) {
            accum_[i] = LayerAccumulator{};
            for (uint8_t word = 0; word < 4u; ++word) {
                publishedWords_[i][word].store(0u, std::memory_order_relaxed);
            }
        }
        publishedMeta_.store(0u, std::memory_order_relaxed);
        endPublish();
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
            const uint8_t pow2 = event.data2 > 5u ? 5u : event.data2;
            const uint32_t denominator = uint32_t{1} << pow2;
            const uint32_t quartersX4 =
                (static_cast<uint32_t>(event.data1) * 16u) / denominator;
            if (quartersX4 != 0u) {
                ticksPerBar_ =
                    (static_cast<uint32_t>(division_) * quartersX4) / 4u;
                if (ticksPerBar_ == 0u) {
                    ticksPerBar_ = static_cast<uint32_t>(division_) * 4u;
                }
            }
            return;
        }

        if (event.kind != SmfEventKind::NoteOn &&
            event.kind != SmfEventKind::NoteOff) {
            return;
        }

        int index = findLayer(trackIndex);
        if (index < 0) {
            if (event.kind != SmfEventKind::NoteOn) return;
            index = insertLayer(trackIndex);
            if (index < 0) return;
        }

        LayerAccumulator& layer = accum_[static_cast<std::size_t>(index)];
        advanceActive(layer, event.tick);
        layer.channelMask |= static_cast<uint16_t>(
            uint16_t{1} << (event.channel & 0x0Fu));
        if (event.kind == SmfEventKind::NoteOn) {
            observeNoteOn(layer, event);
        } else {
            observeNoteOff(layer, event.tick);
        }
    }

    void finalize() {
        if (finalized_) return;
        finalized_ = true;

        const uint32_t limitTick = ticksPerBar_ * kSmfStructuralBarLimit;
        const uint32_t boundedEnd = maxObservedTick_ < limitTick
            ? maxObservedTick_
            : limitTick;
        for (uint8_t i = 0; i < layerCount_; ++i) {
            if (accum_[i].activeCount != 0u) {
                advanceActive(accum_[i], boundedEnd);
                extendSpan(accum_[i], boundedEnd);
            }
        }

        SmfStructuralInspectorSnapshot result{};
        result.sourceTrackCount = sourceTrackCount_;
        result.analyzedBars = boundedEnd == 0u
            ? 0u
            : static_cast<uint16_t>((boundedEnd / ticksPerBar_) + 1u);
        if (result.analyzedBars > kSmfStructuralBarLimit) {
            result.analyzedBars = kSmfStructuralBarLimit;
        }
        result.partial = partial_ || maxObservedTick_ >= limitTick;
        result.layerCount = layerCount_;

        for (uint8_t i = 0; i < layerCount_; ++i) {
            const LayerAccumulator& source = accum_[i];
            SmfStructuralLayerSnapshot& out = result.layers[i];
            out.trackIndex = source.trackIndex;
            out.channelMask = source.channelMask;
            out.minNote = source.hasNotes() ? source.minNote : 0u;
            out.maxNote = source.hasNotes() ? source.maxNote : 0u;
            out.maxPolyphony = source.maxPolyphony;

            const uint16_t spanBars = layerSpanBars(source);
            out.notesPerBarX10 = spanBars == 0u
                ? 0u
                : saturatingU16(
                    (static_cast<uint32_t>(source.noteCount) * 10u) / spanBars);

            const uint32_t spanTicks = layerSpanTicks(source, boundedEnd);
            out.activePermille = spanTicks == 0u
                ? 0u
                : static_cast<uint16_t>(
                    (static_cast<uint64_t>(source.activeTicks) * 1000u) /
                    spanTicks);
            if (out.activePermille > 1000u) out.activePermille = 1000u;

            out.gridDenominator = inferGrid(source);
            out.loopBars = inferLoop(source);
            out.swingPercent = inferSwing(source, out.gridDenominator);
            out.motion = inferMotion(source);

            uint8_t formMax = 1u;
            for (uint8_t bin = 0; bin < 4u; ++bin) {
                if (source.formNotes[bin] > formMax) {
                    formMax = source.formNotes[bin];
                }
            }
            for (uint8_t bin = 0; bin < 4u; ++bin) {
                out.form[bin] = static_cast<uint8_t>(
                    (static_cast<uint16_t>(source.formNotes[bin]) * 8u) /
                    formMax);
            }
        }

        assignRolesAndOverlap(result);
        publish(result);
        smfTrackInspectorState().freeze();
    }

    SmfStructuralInspectorSnapshot snapshot() const {
        SmfStructuralInspectorSnapshot out{};
        while (true) {
            const uint32_t before = sequence_.load(std::memory_order_acquire);
            if ((before & 1u) != 0u) continue;

            const uint32_t meta =
                publishedMeta_.load(std::memory_order_relaxed);
            out.sourceTrackCount = static_cast<uint16_t>(meta & 0x7Fu);
            out.analyzedBars = static_cast<uint16_t>((meta >> 7u) & 0x7Fu);
            out.layerCount = static_cast<uint8_t>((meta >> 14u) & 0x0Fu);
            out.partial = ((meta >> 18u) & 1u) != 0u;
            if (out.layerCount > kSmfStructuralMaxLayers) {
                out.layerCount = static_cast<uint8_t>(kSmfStructuralMaxLayers);
            }
            for (uint8_t i = 0; i < out.layerCount; ++i) {
                out.layers[i] = unpackLayer(i);
            }

            const uint32_t after = sequence_.load(std::memory_order_acquire);
            if (before == after) return out;
        }
    }

private:
    static constexpr uint8_t kGridCount = 3u;
    static constexpr uint8_t kHashBars = 8u;

    struct LayerAccumulator {
        uint32_t lastTick{0};
        uint32_t activeTicks{0};
        uint16_t channelMask{0};
        uint16_t noteCount{0};
        uint16_t gridHits[kGridCount]{};
        uint16_t barHash[kHashBars]{};
        uint8_t trackIndex{0};
        uint8_t activeCount{0};
        uint8_t maxPolyphony{0};
        uint8_t formNotes[4]{};
        uint8_t swingMean[kGridCount]{50u, 50u, 50u};
        uint8_t swingSamples[kGridCount]{};
        uint8_t minNote{127};
        uint8_t maxNote{0};
        uint8_t firstBar{0};
        uint8_t lastBar{0};
        uint8_t flags{0};

        bool hasNotes() const { return (flags & 1u) != 0u; }
        void markNotes() { flags |= 1u; }
    };

    static uint16_t saturatingU16(uint32_t value) {
        return value > std::numeric_limits<uint16_t>::max()
            ? std::numeric_limits<uint16_t>::max()
            : static_cast<uint16_t>(value);
    }

    void beginPublish() {
        sequence_.fetch_add(1u, std::memory_order_acq_rel);
    }

    void endPublish() {
        sequence_.fetch_add(1u, std::memory_order_release);
    }

    int findLayer(uint16_t trackIndex) const {
        for (uint8_t i = 0; i < layerCount_; ++i) {
            if (accum_[i].trackIndex == trackIndex) return i;
        }
        return -1;
    }

    int insertLayer(uint16_t trackIndex) {
        if (trackIndex >= 64u) return -1;
        if (layerCount_ == kSmfStructuralMaxLayers) {
            if (trackIndex >= accum_[layerCount_ - 1u].trackIndex) return -1;
            --layerCount_;
        }
        uint8_t at = layerCount_;
        while (at > 0u && accum_[at - 1u].trackIndex > trackIndex) {
            accum_[at] = accum_[at - 1u];
            --at;
        }
        accum_[at] = LayerAccumulator{};
        accum_[at].trackIndex = static_cast<uint8_t>(trackIndex);
        ++layerCount_;
        return at;
    }

    void advanceActive(LayerAccumulator& layer, uint32_t tick) {
        if (tick > layer.lastTick && layer.activeCount != 0u) {
            const uint32_t delta = tick - layer.lastTick;
            const uint32_t room =
                std::numeric_limits<uint32_t>::max() - layer.activeTicks;
            layer.activeTicks += delta > room ? room : delta;
        }
        if (tick > layer.lastTick) layer.lastTick = tick;
    }

    void extendSpan(LayerAccumulator& layer, uint32_t tick) const {
        if (!layer.hasNotes()) return;
        uint32_t bar = ticksPerBar_ == 0u ? 0u : tick / ticksPerBar_;
        if (bar >= kSmfStructuralBarLimit) bar = kSmfStructuralBarLimit - 1u;
        if (bar > layer.lastBar) layer.lastBar = static_cast<uint8_t>(bar);
    }

    void observeNoteOn(LayerAccumulator& layer, const SmfEvent& event) {
        const uint32_t bar32 = event.tick / ticksPerBar_;
        const uint8_t bar = static_cast<uint8_t>(bar32);
        if (!layer.hasNotes()) {
            layer.markNotes();
            layer.firstBar = bar;
            layer.lastBar = bar;
        } else if (bar > layer.lastBar) {
            layer.lastBar = bar;
        }

        if (layer.noteCount != std::numeric_limits<uint16_t>::max()) {
            ++layer.noteCount;
        }
        if (layer.activeCount != std::numeric_limits<uint8_t>::max()) {
            ++layer.activeCount;
        }
        if (layer.activeCount > layer.maxPolyphony) {
            layer.maxPolyphony = layer.activeCount;
        }
        if (event.data1 < layer.minNote) layer.minNote = event.data1;
        if (event.data1 > layer.maxNote) layer.maxNote = event.data1;

        const uint8_t formBin = static_cast<uint8_t>(bar32 / 16u);
        if (formBin < 4u && layer.formNotes[formBin] != 255u) {
            ++layer.formNotes[formBin];
        }

        const uint32_t position = event.tick % ticksPerBar_;
        observeGridAndSwing(layer, position, 8u, 0u);
        observeGridAndSwing(layer, position, 16u, 1u);
        observeGridAndSwing(layer, position, 32u, 2u);

        const uint32_t relativeBar = bar32 - layer.firstBar;
        if (relativeBar < kHashBars) {
            uint16_t& hash = layer.barHash[relativeBar];
            const uint32_t token =
                position + static_cast<uint32_t>(event.data1) * 131u + 1u;
            hash = static_cast<uint16_t>((hash * 251u) ^
                (token & 0xFFFFu) ^ (token >> 16u));
            if (hash == 0u) hash = 1u;
        }
    }

    void observeNoteOff(LayerAccumulator& layer, uint32_t tick) {
        if (layer.activeCount != 0u) --layer.activeCount;
        extendSpan(layer, tick);
    }

    void observeGridAndSwing(LayerAccumulator& layer,
                             uint32_t position,
                             uint8_t denominator,
                             uint8_t slot) const {
        const uint32_t step = ticksPerBar_ / denominator;
        if (step == 0u) return;

        const uint32_t remainder = position % step;
        const uint32_t distance = remainder < step - remainder
            ? remainder
            : step - remainder;
        bool gridHit = distance * 12u <= step;
        bool swung = false;
        uint32_t percent = 0u;

        const uint32_t nearest = (position + step / 2u) / step;
        if ((nearest & 1u) != 0u && nearest != 0u) {
            const uint32_t pairStart = (nearest - 1u) * step;
            if (position >= pairStart) {
                const uint32_t pair = step * 2u;
                percent = ((position - pairStart) * 100u) / pair;
                if (percent >= 40u && percent <= 80u) {
                    swung = true;
                    if (percent >= 55u) gridHit = true;
                }
            }
        }

        if (gridHit && layer.gridHits[slot] != 65535u) {
            ++layer.gridHits[slot];
        }
        if (!swung) return;

        uint8_t& count = layer.swingSamples[slot];
        uint8_t& mean = layer.swingMean[slot];
        if (count < 255u) {
            const uint16_t total =
                static_cast<uint16_t>(mean) * count + percent;
            ++count;
            mean = static_cast<uint8_t>(total / count);
        } else {
            mean = static_cast<uint8_t>(
                (static_cast<uint16_t>(mean) * 7u + percent) / 8u);
        }
    }

    static uint8_t inferGrid(const LayerAccumulator& layer) {
        if (layer.noteCount == 0u) return 0u;
        const uint32_t threshold =
            (static_cast<uint32_t>(layer.noteCount) * 3u) / 4u;
        if (layer.gridHits[0] >= threshold) return 8u;
        if (layer.gridHits[1] >= threshold) return 16u;
        if (layer.gridHits[2] >= threshold) return 32u;
        return 0u;
    }

    static uint8_t inferSwing(const LayerAccumulator& layer, uint8_t grid) {
        uint8_t slot = 0u;
        if (grid == 16u) slot = 1u;
        else if (grid == 32u) slot = 2u;
        else if (grid != 8u) return 50u;
        return layer.swingSamples[slot] == 0u
            ? 50u
            : layer.swingMean[slot];
    }

    static uint8_t inferLoop(const LayerAccumulator& layer) {
        const uint16_t* h = layer.barHash;
        if (h[0] != 0u && h[1] != 0u && h[0] == h[1]) return 1u;
        if (h[0] != 0u && h[1] != 0u && h[2] != 0u && h[3] != 0u &&
            h[0] == h[2] && h[1] == h[3]) return 2u;
        for (uint8_t i = 0u; i < 4u; ++i) {
            if (h[i] == 0u || h[i + 4u] == 0u || h[i] != h[i + 4u]) {
                return 0u;
            }
        }
        return 4u;
    }

    static SmfStructuralMotion inferMotion(const LayerAccumulator& layer) {
        uint8_t comparisons = 0u;
        uint8_t changes = 0u;
        uint16_t previous = 0u;
        for (uint8_t i = 0u; i < kHashBars; ++i) {
            const uint16_t current = layer.barHash[i];
            if (current == 0u) continue;
            if (previous != 0u) {
                ++comparisons;
                if (current != previous) ++changes;
            }
            previous = current;
        }
        if (comparisons == 0u || changes == 0u) {
            return SmfStructuralMotion::Low;
        }
        if (static_cast<uint16_t>(changes) * 3u <= comparisons) {
            return SmfStructuralMotion::Medium;
        }
        return SmfStructuralMotion::High;
    }

    static uint16_t layerSpanBars(const LayerAccumulator& layer) {
        if (!layer.hasNotes() || layer.lastBar < layer.firstBar) return 0u;
        return static_cast<uint16_t>(layer.lastBar - layer.firstBar + 1u);
    }

    uint32_t layerSpanTicks(const LayerAccumulator& layer,
                            uint32_t boundedEnd) const {
        if (!layer.hasNotes()) return 0u;
        const uint32_t start =
            static_cast<uint32_t>(layer.firstBar) * ticksPerBar_;
        uint32_t end =
            (static_cast<uint32_t>(layer.lastBar) + 1u) * ticksPerBar_;
        if (boundedEnd > start && boundedEnd < end) end = boundedEnd;
        return end > start ? end - start : ticksPerBar_;
    }

    static uint8_t overlapPercent(const SmfStructuralLayerSnapshot& a,
                                  const SmfStructuralLayerSnapshot& b) {
        if (!a.hasNotes() || !b.hasNotes()) return 0u;
        const int low = a.minNote > b.minNote ? a.minNote : b.minNote;
        const int high = a.maxNote < b.maxNote ? a.maxNote : b.maxNote;
        if (high < low) return 0u;
        const int span = static_cast<int>(a.maxNote) - a.minNote + 1;
        return static_cast<uint8_t>(((high - low + 1) * 100) /
                                    (span > 0 ? span : 1));
    }

    static void assignRolesAndOverlap(SmfStructuralInspectorSnapshot& data) {
        int chords = -1;
        int lead = -1;
        for (uint8_t i = 0; i < data.layerCount; ++i) {
            SmfStructuralLayerSnapshot& layer = data.layers[i];
            const bool drums =
                (layer.channelMask & (uint16_t{1} << 9u)) != 0u;
            const uint8_t middle = static_cast<uint8_t>(
                (static_cast<uint16_t>(layer.minNote) + layer.maxNote) / 2u);
            if (drums) layer.role = SmfStructuralRole::Drums;
            else if (middle < 48u && layer.maxPolyphony <= 2u) {
                layer.role = SmfStructuralRole::Bass;
            } else if (layer.maxPolyphony >= 3u &&
                       layer.activePermille >= 700u) {
                layer.role = SmfStructuralRole::Pad;
            } else if (layer.maxPolyphony >= 3u) {
                layer.role = SmfStructuralRole::Chords;
            } else if (middle >= 60u) {
                layer.role = SmfStructuralRole::Lead;
            } else {
                layer.role = SmfStructuralRole::Other;
            }

            if (chords < 0 &&
                (layer.role == SmfStructuralRole::Chords ||
                 layer.role == SmfStructuralRole::Pad)) {
                chords = i;
            }
            if (lead < 0 && layer.role == SmfStructuralRole::Lead) lead = i;
        }

        for (uint8_t i = 0; i < data.layerCount; ++i) {
            if (chords >= 0 && chords != i) {
                data.layers[i].overlapChordsPercent = overlapPercent(
                    data.layers[i], data.layers[static_cast<uint8_t>(chords)]);
            }
            if (lead >= 0 && lead != i) {
                data.layers[i].overlapLeadPercent = overlapPercent(
                    data.layers[i], data.layers[static_cast<uint8_t>(lead)]);
            }
        }
    }

    static uint8_t encodeGrid(uint8_t grid) {
        return grid == 8u ? 1u : (grid == 16u ? 2u : (grid == 32u ? 3u : 0u));
    }

    static uint8_t decodeGrid(uint8_t grid) {
        return grid == 1u ? 8u : (grid == 2u ? 16u : (grid == 3u ? 32u : 0u));
    }

    void publish(const SmfStructuralInspectorSnapshot& data) {
        beginPublish();
        for (uint8_t i = 0u; i < data.layerCount; ++i) {
            packLayer(i, data.layers[i]);
        }
        const uint32_t meta =
            (static_cast<uint32_t>(data.sourceTrackCount & 0x7Fu)) |
            (static_cast<uint32_t>(data.analyzedBars & 0x7Fu) << 7u) |
            (static_cast<uint32_t>(data.layerCount & 0x0Fu) << 14u) |
            (data.partial ? (uint32_t{1} << 18u) : 0u);
        publishedMeta_.store(meta, std::memory_order_relaxed);
        endPublish();
    }

    void packLayer(uint8_t index, const SmfStructuralLayerSnapshot& layer) {
        const uint32_t word0 =
            (static_cast<uint32_t>(layer.trackIndex) & 0x3Fu) |
            (static_cast<uint32_t>(layer.channelMask) << 6u) |
            (static_cast<uint32_t>(encodeGrid(layer.gridDenominator)) << 22u) |
            (static_cast<uint32_t>(layer.loopBars & 0x07u) << 24u) |
            (static_cast<uint32_t>(layer.role) << 27u) |
            (static_cast<uint32_t>(layer.motion) << 30u);
        const uint32_t word1 =
            (static_cast<uint32_t>(layer.notesPerBarX10) & 0x7FFFu) |
            (static_cast<uint32_t>(layer.activePermille & 0x03FFu) << 15u) |
            (static_cast<uint32_t>(layer.swingPercent & 0x7Fu) << 25u);
        const uint32_t word2 =
            static_cast<uint32_t>(layer.minNote & 0x7Fu) |
            (static_cast<uint32_t>(layer.maxNote & 0x7Fu) << 7u) |
            (static_cast<uint32_t>(layer.maxPolyphony) << 14u) |
            (static_cast<uint32_t>(layer.overlapChordsPercent & 0x7Fu) << 22u);
        const uint32_t word3 =
            static_cast<uint32_t>(layer.overlapLeadPercent & 0x7Fu) |
            (static_cast<uint32_t>(layer.form[0] & 0x0Fu) << 7u) |
            (static_cast<uint32_t>(layer.form[1] & 0x0Fu) << 11u) |
            (static_cast<uint32_t>(layer.form[2] & 0x0Fu) << 15u) |
            (static_cast<uint32_t>(layer.form[3] & 0x0Fu) << 19u);
        publishedWords_[index][0].store(word0, std::memory_order_relaxed);
        publishedWords_[index][1].store(word1, std::memory_order_relaxed);
        publishedWords_[index][2].store(word2, std::memory_order_relaxed);
        publishedWords_[index][3].store(word3, std::memory_order_relaxed);
    }

    SmfStructuralLayerSnapshot unpackLayer(uint8_t index) const {
        SmfStructuralLayerSnapshot layer{};
        const uint32_t word0 = publishedWords_[index][0].load(std::memory_order_relaxed);
        const uint32_t word1 = publishedWords_[index][1].load(std::memory_order_relaxed);
        const uint32_t word2 = publishedWords_[index][2].load(std::memory_order_relaxed);
        const uint32_t word3 = publishedWords_[index][3].load(std::memory_order_relaxed);
        layer.trackIndex = static_cast<uint16_t>(word0 & 0x3Fu);
        layer.channelMask = static_cast<uint16_t>((word0 >> 6u) & 0xFFFFu);
        layer.gridDenominator = decodeGrid(static_cast<uint8_t>((word0 >> 22u) & 3u));
        layer.loopBars = static_cast<uint8_t>((word0 >> 24u) & 7u);
        layer.role = static_cast<SmfStructuralRole>((word0 >> 27u) & 7u);
        layer.motion = static_cast<SmfStructuralMotion>((word0 >> 30u) & 3u);
        layer.notesPerBarX10 = static_cast<uint16_t>(word1 & 0x7FFFu);
        layer.activePermille = static_cast<uint16_t>((word1 >> 15u) & 0x03FFu);
        layer.swingPercent = static_cast<uint8_t>((word1 >> 25u) & 0x7Fu);
        layer.minNote = static_cast<uint8_t>(word2 & 0x7Fu);
        layer.maxNote = static_cast<uint8_t>((word2 >> 7u) & 0x7Fu);
        layer.maxPolyphony = static_cast<uint8_t>((word2 >> 14u) & 0xFFu);
        layer.overlapChordsPercent = static_cast<uint8_t>((word2 >> 22u) & 0x7Fu);
        layer.overlapLeadPercent = static_cast<uint8_t>(word3 & 0x7Fu);
        layer.form[0] = static_cast<uint8_t>((word3 >> 7u) & 0x0Fu);
        layer.form[1] = static_cast<uint8_t>((word3 >> 11u) & 0x0Fu);
        layer.form[2] = static_cast<uint8_t>((word3 >> 15u) & 0x0Fu);
        layer.form[3] = static_cast<uint8_t>((word3 >> 19u) & 0x0Fu);
        return layer;
    }

    uint16_t division_{96};
    uint16_t sourceTrackCount_{0};
    uint32_t ticksPerBar_{384};
    uint32_t maxObservedTick_{0};
    uint8_t layerCount_{0};
    bool partial_{false};
    bool finalized_{false};
    LayerAccumulator accum_[kSmfStructuralMaxLayers]{};
    std::atomic<uint32_t> sequence_{0};
    std::atomic<uint32_t> publishedMeta_{0};
    std::atomic<uint32_t> publishedWords_[kSmfStructuralMaxLayers][4]{};
};

inline SmfStructuralInspectorState& smfStructuralInspectorState() {
    static SmfStructuralInspectorState state;
    return state;
}

static_assert(sizeof(SmfStructuralInspectorState) <= 680,
              "SMF structural analysis state must remain bounded");

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_STRUCTURAL_INSPECTOR_H
