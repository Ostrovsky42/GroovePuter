#pragma once

#ifndef ARDUINO

#include <stdint.h>
#include <string>
#include <iostream>
#include <chrono>

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
    // Basic busy wait or sleep - for SDL loop usually handled differently but 
    // for simple logic this might suffice if not blocking the main thread too long.
    // However, in a game loop, extensive delays are bad.
    // For now, no-op or simple sleep?
    // Using SDL_Delay if available would be better but we don't want to enforce SDL dependency here if possible.
    // But since this IS platform_sdl...
    extern void SDL_Delay(uint32_t ms);
    SDL_Delay(ms); 
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

    template<typename T, typename... Args>
    void printf(const char* format, T val, Args... args) {
        // Simple shim, might not handle Arduino Flash strings (F())
        ::printf(format, val, args...);
    }
    
    void printf(const char* format) {
        ::printf("%s", format);
    }
    
    void flush() {
        std::cout.flush();
    }
};

extern SerialMock Serial;
// Define instance in a cpp file or make it static/inline?
// For simplicity in a single-binary build, we can perhaps cheat and define it here if guarded... 
// But correct way is extern here.
// Let's define a static instance to avoid linker errors in simple setups, 
// or require including a .cpp. 
// Actually, `sdl_main.cpp` can define it.
// Let's go with extern and define in sdl_main.cpp.

#endif // !ARDUINO
