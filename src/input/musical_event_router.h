#pragma once

#include <cstddef>

#include "musical_event.h"

class IMusicalEventSink {
public:
    virtual ~IMusicalEventSink() = default;
    virtual void handleMusicalEvent(const MusicalEvent& event) = 0;
};

class MusicalEventRouter {
public:
    static constexpr std::size_t kMaxSinks = 4;

    bool addSink(IMusicalEventSink& sink) {
        for (std::size_t i = 0; i < sinkCount_; ++i) {
            if (sinks_[i] == &sink) return true;
        }
        if (sinkCount_ >= kMaxSinks) return false;
        sinks_[sinkCount_++] = &sink;
        return true;
    }

    void removeSink(IMusicalEventSink& sink) {
        for (std::size_t i = 0; i < sinkCount_; ++i) {
            if (sinks_[i] != &sink) continue;
            for (std::size_t j = i + 1; j < sinkCount_; ++j) {
                sinks_[j - 1] = sinks_[j];
            }
            sinks_[--sinkCount_] = nullptr;
            return;
        }
    }

    void route(const MusicalEvent& event) const {
        for (std::size_t i = 0; i < sinkCount_; ++i) {
            sinks_[i]->handleMusicalEvent(event);
        }
    }

    std::size_t sinkCount() const { return sinkCount_; }

private:
    IMusicalEventSink* sinks_[kMaxSinks]{};
    std::size_t sinkCount_{0};
};
