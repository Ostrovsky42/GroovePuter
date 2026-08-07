#pragma once

#ifndef ARDUINO

#include <stdint.h>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>
#include <vector>

// Basic types
typedef uint8_t byte;
typedef bool boolean;

// Time functions
inline unsigned long millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();
    return static_cast<unsigned long>(
        duration_cast<milliseconds>(now - start).count());
}

inline unsigned long micros() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();
    return static_cast<unsigned long>(
        duration_cast<microseconds>(now - start).count());
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Serial Mock
class SerialMock {
public:
    void begin(long baud) { (void)baud; }

    template<typename T>
    void print(const T& value) {
        std::cout << value;
    }

    template<typename T>
    void println(const T& value) {
        std::cout << value << std::endl;
    }

    void println() {
        std::cout << std::endl;
    }

    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        ::vprintf(format, args);
        va_end(args);
    }

    void flush() {
        std::cout.flush();
    }
};

extern SerialMock Serial;

#define FILE_READ 0
#define FILE_WRITE 1

class File {
public:
    File() = default;

    explicit operator bool() const {
        if (!state_) return false;
        if (state_->directory) return true;
        return state_->stream.is_open();
    }

    size_t write(const uint8_t* data, size_t length) {
        if (!state_ || state_->directory || !state_->stream.is_open() || !data) {
            return 0;
        }
        state_->stream.write(reinterpret_cast<const char*>(data),
                             static_cast<std::streamsize>(length));
        return state_->stream.good() ? length : 0;
    }

    size_t read(uint8_t* data, size_t length) {
        if (!state_ || state_->directory || !state_->stream.is_open() || !data) {
            return 0;
        }
        state_->stream.read(reinterpret_cast<char*>(data),
                            static_cast<std::streamsize>(length));
        return static_cast<size_t>(state_->stream.gcount());
    }

    int read() {
        uint8_t value = 0;
        return read(&value, 1) == 1 ? static_cast<int>(value) : -1;
    }

    size_t readBytes(char* data, size_t length) {
        return read(reinterpret_cast<uint8_t*>(data), length);
    }

    bool seek(size_t position) {
        if (!state_ || state_->directory || !state_->stream.is_open()) {
            return false;
        }
        state_->stream.clear();
        state_->stream.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        state_->stream.seekp(static_cast<std::streamoff>(position), std::ios::beg);
        return !state_->stream.fail();
    }

    size_t position() {
        if (!state_ || state_->directory || !state_->stream.is_open()) return 0;
        if (state_->stream.eof()) return size();
        const std::streampos readPos = state_->stream.tellg();
        if (readPos != std::streampos(-1)) {
            return static_cast<size_t>(readPos);
        }
        const std::streampos writePos = state_->stream.tellp();
        return writePos == std::streampos(-1)
            ? 0
            : static_cast<size_t>(writePos);
    }

    int available() {
        const size_t current = position();
        const size_t total = size();
        if (current >= total) return 0;
        const size_t remaining = total - current;
        return remaining > static_cast<size_t>(INT32_MAX)
            ? INT32_MAX
            : static_cast<int>(remaining);
    }

    void flush() {
        if (state_ && state_->stream.is_open()) state_->stream.flush();
    }

    void close() {
        if (state_ && state_->stream.is_open()) state_->stream.close();
        state_.reset();
    }

    size_t size() const {
        if (!state_ || state_->directory) return 0;
        std::error_code ec;
        const uintmax_t bytes = std::filesystem::file_size(state_->path, ec);
        return ec ? 0 : static_cast<size_t>(bytes);
    }

    bool isDirectory() const {
        return state_ && state_->directory;
    }

    File openNextFile() {
        if (!state_ || !state_->directory ||
            state_->directoryIndex >= state_->directoryEntries.size()) {
            return File();
        }
        const std::filesystem::path entry =
            state_->directoryEntries[state_->directoryIndex++];
        return openPath_(entry, FILE_READ);
    }

    const char* name() const {
        return state_ ? state_->displayName.c_str() : "";
    }

    size_t print(const char* text) {
        if (!text) return 0;
        return write(reinterpret_cast<const uint8_t*>(text),
                     std::char_traits<char>::length(text));
    }

private:
    struct State {
        std::filesystem::path path;
        std::fstream stream;
        bool directory = false;
        std::vector<std::filesystem::path> directoryEntries;
        size_t directoryIndex = 0;
        std::string displayName;
    };

    explicit File(std::shared_ptr<State> state) : state_(std::move(state)) {}

    static File openPath_(const std::filesystem::path& path, int mode) {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec) && !ec) {
            auto state = std::make_shared<State>();
            state->path = path;
            state->directory = true;
            state->displayName = path.filename().string();
            for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
                if (ec) break;
                state->directoryEntries.push_back(entry.path());
            }
            return File(std::move(state));
        }

        auto state = std::make_shared<State>();
        state->path = path;
        state->displayName = path.filename().string();
        std::ios::openmode flags = std::ios::binary;
        if (mode == FILE_WRITE) {
            flags |= std::ios::in | std::ios::out | std::ios::app;
            state->stream.open(path, flags);
            if (!state->stream.is_open()) {
                state->stream.clear();
                state->stream.open(path,
                    std::ios::binary | std::ios::out | std::ios::trunc);
                state->stream.close();
                state->stream.open(path, flags);
            }
        } else {
            flags |= std::ios::in;
            state->stream.open(path, flags);
        }
        if (!state->stream.is_open()) return File();
        return File(std::move(state));
    }

    std::shared_ptr<State> state_;

    friend class SDMock;
};

class SDMock {
public:
    SDMock() : root_(std::filesystem::current_path()) {}

    void setRoot(const std::filesystem::path& root) {
        root_ = root;
    }

    bool exists(const char* path) const {
        std::error_code ec;
        return std::filesystem::exists(resolve_(path), ec) && !ec;
    }

    bool mkdir(const char* path) {
        std::error_code ec;
        return std::filesystem::create_directories(resolve_(path), ec) ||
               (!ec && std::filesystem::is_directory(resolve_(path)));
    }

    bool remove(const char* path) {
        std::error_code ec;
        return std::filesystem::remove(resolve_(path), ec) && !ec;
    }

    bool rename(const char* from, const char* to) {
        std::error_code ec;
        std::filesystem::rename(resolve_(from), resolve_(to), ec);
        return !ec;
    }

    File open(const char* path, int mode = FILE_READ) {
        const std::filesystem::path resolved = resolve_(path);
        if (mode == FILE_WRITE) {
            std::error_code ec;
            const auto parent = resolved.parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent, ec);
            if (ec) return File();
        }
        return File::openPath_(resolved, mode);
    }

private:
    std::filesystem::path resolve_(const char* rawPath) const {
        std::filesystem::path path = rawPath ? rawPath : "";
        if (path.is_absolute()) path = path.relative_path();
        return (root_ / path).lexically_normal();
    }

    std::filesystem::path root_;
};

extern SDMock SD;

#endif // !ARDUINO
