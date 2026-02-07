#pragma once

#ifndef ARDUINO

#include <stdint.h>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdarg>

// Basic types
typedef uint8_t byte;
typedef bool boolean;

// Time functions
inline unsigned long millis() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now - start).count();
}

inline unsigned long micros() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<microseconds>(now - start).count();
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Serial Mock
class SerialMock {
public:
    void begin(long baud) { (void)baud; }
    
    template<typename T>
    void print(T val) {
        std::cout << val;
    }
    
    template<typename T>
    void println(T val) {
        std::cout << val << std::endl;
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

// SD/FS Mock
class File {
public:
    operator bool() const { return false; }
    size_t write(const uint8_t*, size_t) { return 0; }
    size_t read(uint8_t*, size_t) { return 0; }
    bool seek(size_t) { return false; }
    void close() {}
    size_t size() const { return 0; }
    bool isDirectory() const { return false; }
    File openNextFile() { return File(); }
    const char* name() const { return ""; }
};

class SDMock {
public:
    bool exists(const char*) const { return false; }
    bool mkdir(const char*) { return false; }
    bool remove(const char*) { return false; }
    File open(const char*, int mode = 0) { return File(); }
};

extern SDMock SD;
#define FILE_WRITE 1
#define FILE_READ 0

#endif // !ARDUINO
