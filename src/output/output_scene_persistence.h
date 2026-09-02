#pragma once
#ifndef GROOVEPUTER_OUTPUT_SCENE_PERSISTENCE_H
#define GROOVEPUTER_OUTPUT_SCENE_PERSISTENCE_H

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "output_ownership.h"

namespace GroovePuterOutput {

struct PersistedOutputModes {
    bool present{false};
    bool valid{true};
    uint8_t values[3]{0, 0, 0};

    bool commit() const {
        if (!valid) return false;
        static constexpr Track kTracks[3] = {
            Track::SynthA, Track::SynthB, Track::Drums,
        };
        for (std::size_t i = 0; i < 3; ++i) {
            const uint8_t raw = present ? values[i] : 0u;
            if (!restorePersistedModeValue(kTracks[i], raw)) return false;
        }
        return true;
    }
};

// Streaming root-field observer. The JSON itself is passed through unchanged;
// this object only captures the compact top-level `"out":[A,B,D]` extension.
// Runtime ownership is not mutated until commit() is called after the complete
// SceneManager parse succeeds.
class OutputSceneReadState {
public:
    bool accept(char c) {
        if (failed_) return false;

        if (inString_) {
            if (escape_) {
                escape_ = false;
                if (collectingRootKey_) keyOverflow_ = true;
                return true;
            }
            if (c == '\\') {
                escape_ = true;
                return true;
            }
            if (c == '"') {
                inString_ = false;
                if (collectingRootKey_) {
                    const bool isOut = !keyOverflow_ && keyLen_ == 3 &&
                        key_[0] == 'o' && key_[1] == 'u' && key_[2] == 't';
                    if (isOut) {
                        if (sawOutKey_) return fail();
                        sawOutKey_ = true;
                        lastRootKeyWasOut_ = true;
                    } else {
                        lastRootKeyWasOut_ = false;
                    }
                }
                collectingRootKey_ = false;
                return true;
            }
            if (collectingRootKey_) {
                if (keyLen_ < sizeof(key_)) key_[keyLen_++] = c;
                else keyOverflow_ = true;
            }
            return true;
        }

        if (parsingOutArray_) return acceptOutArray(c);

        if (waitingOutArray_) {
            if (std::isspace(static_cast<unsigned char>(c))) return true;
            if (c != '[') return fail();
            ++depth_;
            parsingOutArray_ = true;
            waitingOutArray_ = false;
            outCount_ = 0;
            outExpectValue_ = true;
            return true;
        }

        if (c == '"') {
            inString_ = true;
            escape_ = false;
            collectingRootKey_ = rootStarted_ && depth_ == 1 && rootExpectKey_;
            keyLen_ = 0;
            keyOverflow_ = false;
            return true;
        }

        if (c == '{') {
            ++depth_;
            if (!rootStarted_) {
                if (depth_ != 1) return fail();
                rootStarted_ = true;
                rootExpectKey_ = true;
            }
            return true;
        }
        if (c == '[') {
            ++depth_;
            return true;
        }
        if (c == '}' || c == ']') {
            if (depth_ <= 0) return fail();
            --depth_;
            return true;
        }

        if (rootStarted_ && depth_ == 1) {
            if (c == ':') {
                if (lastRootKeyWasOut_) waitingOutArray_ = true;
                rootExpectKey_ = false;
                return true;
            }
            if (c == ',') {
                rootExpectKey_ = true;
                lastRootKeyWasOut_ = false;
                return true;
            }
        }
        return true;
    }

    bool finish() {
        if (finished_) return !failed_;
        finished_ = true;
        if (!rootStarted_ || inString_ || escape_ || waitingOutArray_ ||
            parsingOutArray_ || depth_ != 0) {
            failed_ = true;
        }
        result_.valid = !failed_;
        return !failed_;
    }

    bool failed() const { return failed_; }
    const PersistedOutputModes& result() const { return result_; }

    bool commit() {
        if (!finish()) return false;
        return result_.commit();
    }

private:
    bool fail() {
        failed_ = true;
        result_.valid = false;
        return false;
    }

    bool acceptOutArray(char c) {
        if (std::isspace(static_cast<unsigned char>(c))) return true;
        if (c >= '0' && c <= '3') {
            if (!outExpectValue_ || outCount_ >= 3) return fail();
            result_.values[outCount_++] = static_cast<uint8_t>(c - '0');
            outExpectValue_ = false;
            return true;
        }
        if (c == ',') {
            if (outExpectValue_ || outCount_ == 0 || outCount_ >= 3) return fail();
            outExpectValue_ = true;
            return true;
        }
        if (c == ']') {
            if (outExpectValue_ || outCount_ != 3 || depth_ != 2) return fail();
            --depth_;
            parsingOutArray_ = false;
            result_.present = true;
            return true;
        }
        return fail();
    }

    PersistedOutputModes result_{};
    bool failed_{false};
    bool finished_{false};
    bool rootStarted_{false};
    bool rootExpectKey_{false};
    bool inString_{false};
    bool escape_{false};
    bool collectingRootKey_{false};
    bool keyOverflow_{false};
    bool lastRootKeyWasOut_{false};
    bool sawOutKey_{false};
    bool waitingOutArray_{false};
    bool parsingOutArray_{false};
    bool outExpectValue_{false};
    int depth_{0};
    std::size_t keyLen_{0};
    std::size_t outCount_{0};
    char key_[8]{};
};

class OutputSceneWriteState {
public:
    bool accept(char c, const char*& data, std::size_t& len) {
        data = nullptr;
        len = 0;
        if (failed_ || injected_) {
            if (failed_) return false;
            single_[0] = c;
            data = single_;
            len = 1;
            return true;
        }

        if (inString_) {
            if (escape_) escape_ = false;
            else if (c == '\\') escape_ = true;
            else if (c == '"') inString_ = false;
            single_[0] = c;
            data = single_;
            len = 1;
            return true;
        }

        if (c == '"') {
            inString_ = true;
            single_[0] = c;
            data = single_;
            len = 1;
            return true;
        }
        if (c == '{') {
            ++objectDepth_;
            rootStarted_ = true;
            single_[0] = c;
            data = single_;
            len = 1;
            return true;
        }
        if (c == '}') {
            if (objectDepth_ <= 0) return fail();
            if (objectDepth_ == 1 && rootStarted_) {
                const int written = std::snprintf(
                    output_, sizeof(output_),
                    ",\"out\":[%u,%u,%u]}",
                    static_cast<unsigned>(persistedModeValue(Track::SynthA)),
                    static_cast<unsigned>(persistedModeValue(Track::SynthB)),
                    static_cast<unsigned>(persistedModeValue(Track::Drums)));
                if (written <= 0 ||
                    static_cast<std::size_t>(written) >= sizeof(output_)) {
                    return fail();
                }
                --objectDepth_;
                injected_ = true;
                data = output_;
                len = static_cast<std::size_t>(written);
                return true;
            }
            --objectDepth_;
        }

        single_[0] = c;
        data = single_;
        len = 1;
        return true;
    }

    bool finish() {
        if (finished_) return !failed_;
        finished_ = true;
        if (!rootStarted_ || !injected_ || objectDepth_ != 0 || inString_) {
            failed_ = true;
        }
        return !failed_;
    }

    bool failed() const { return failed_; }

private:
    bool fail() {
        failed_ = true;
        return false;
    }

    bool failed_{false};
    bool finished_{false};
    bool rootStarted_{false};
    bool injected_{false};
    bool inString_{false};
    bool escape_{false};
    int objectDepth_{0};
    char single_[1]{};
    char output_[48]{};
};

namespace output_scene_detail {
template <typename Writer>
auto writeBytesImpl(Writer& writer, const char* data, std::size_t len, int)
    -> decltype(writer.write(reinterpret_cast<const uint8_t*>(data), len),
                std::size_t()) {
    return static_cast<std::size_t>(
        writer.write(reinterpret_cast<const uint8_t*>(data), len));
}

template <typename Writer>
auto writeBytesImpl(Writer& writer, const char* data, std::size_t len, long)
    -> decltype(writer.write(data, len), std::size_t()) {
    return static_cast<std::size_t>(writer.write(data, len));
}

template <typename Writer>
std::size_t writeBytes(Writer& writer, const char* data, std::size_t len) {
    return writeBytesImpl(writer, data, len, 0);
}
}  // namespace output_scene_detail

template <typename Writer>
class OutputSceneWriteFilter {
public:
    explicit OutputSceneWriteFilter(Writer& writer) : writer_(writer) {}

    std::size_t write(const uint8_t* data, std::size_t len) {
        std::size_t consumed = 0;
        for (; consumed < len; ++consumed) {
            const char* out = nullptr;
            std::size_t outLen = 0;
            if (!state_.accept(static_cast<char>(data[consumed]), out, outLen)) break;
            if (outLen > 0 &&
                output_scene_detail::writeBytes(writer_, out, outLen) != outLen) {
                break;
            }
        }
        return consumed;
    }

    std::size_t write(const char* data, std::size_t len) {
        return write(reinterpret_cast<const uint8_t*>(data), len);
    }

    bool finish() { return state_.finish(); }
    bool failed() const { return state_.failed(); }

private:
    Writer& writer_;
    OutputSceneWriteState state_;
};

template <typename Reader>
class OutputSceneReadFilter {
public:
    explicit OutputSceneReadFilter(Reader& reader) : reader_(reader) {}

    int read() {
        const int value = reader_.read();
        if (value < 0) {
            eof_ = true;
            finalize();
            return -1;
        }
        if (!state_.accept(static_cast<char>(value))) {
            failed_ = true;
            return -1;
        }
        return value;
    }

    bool failed() {
        finalize();
        return failed_ || state_.failed();
    }

    bool commit() {
        finalize();
        if (failed_ || state_.failed()) return false;
        return state_.commit();
    }

    bool eof() const { return eof_; }

private:
    void finalize() {
        if (finalized_) return;
        finalized_ = true;
        if (!state_.finish()) failed_ = true;
    }

    Reader& reader_;
    OutputSceneReadState state_;
    bool failed_{false};
    bool eof_{false};
    bool finalized_{false};
};

class OutputSceneStringWriter {
public:
    explicit OutputSceneStringWriter(std::string& output) : output_(output) {}
    std::size_t write(const uint8_t* data, std::size_t len) {
        output_.append(reinterpret_cast<const char*>(data), len);
        return len;
    }
private:
    std::string& output_;
};

inline bool injectOutputModesIntoScene(const std::string& input,
                                       std::string& output) {
    output.clear();
    OutputSceneStringWriter writer(output);
    OutputSceneWriteFilter<OutputSceneStringWriter> filtered(writer);
    const std::size_t written = filtered.write(
        reinterpret_cast<const uint8_t*>(input.data()), input.size());
    return written == input.size() && filtered.finish();
}

inline bool captureOutputModesFromScene(const std::string& input,
                                        PersistedOutputModes& output) {
    OutputSceneReadState state;
    for (char c : input) {
        if (!state.accept(c)) return false;
    }
    if (!state.finish()) return false;
    output = state.result();
    return output.valid;
}

}  // namespace GroovePuterOutput

#endif  // GROOVEPUTER_OUTPUT_SCENE_PERSISTENCE_H
