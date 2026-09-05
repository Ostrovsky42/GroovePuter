"""Compile the product mount path against failing SPI/SD drivers."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

STUBS = {
    "Arduino.h": r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
struct Console {
    std::string output;
    template<class... Args> void printf(const char* format, Args... args) {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), format, args...);
        output += buffer;
    }
};
extern Console Serial;
''',
    "SPI.h": r'''
#pragma once
struct Spi {
    bool result = true;
    int calls = 0;
    bool begin(int = -1, int = -1, int = -1, int = -1) {
        ++calls;
        return result;
    }
};
extern Spi SPI;
''',
    "SD.h": r'''
#pragma once
#include "SPI.h"
constexpr int CARD_NONE = 0;
struct Sd {
    bool result = true;
    int type = CARD_NONE;
    int calls = 0;
    int cardType() { return type; }
    bool begin(int, Spi&, unsigned) {
        ++calls;
        type = result ? 2 : CARD_NONE;
        return result;
    }
};
extern Sd SD;
''',
    "esp_heap_caps.h": r'''
#pragma once
#include <cstddef>
constexpr int MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2;
inline std::size_t heap_caps_get_free_size(int) { return 40000; }
inline std::size_t heap_caps_get_largest_free_block(int) { return 20000; }
''',
}

TEST = r'''
#include <cassert>
#include <cstdlib>
#include "Arduino.h"
#include "SD.h"
#include "src/platform/cardputer_sd.h"
Console Serial;
Spi SPI;
Sd SD;
int notifications = 0;
void ready() { ++notifications; }
int main(int, char** argv) {
    using namespace GroovePuterPlatform;
    setCardputerSdReadyHook(ready);
    const int scenario = std::atoi(argv[1]);
    if (scenario == 0) {
        SPI.result = false;
        assert(!ensureCardputerSdMounted());
        assert(SD.calls == 0);
        assert(notifications == 0);
        assert(Serial.output.find("stage=spi") != std::string::npos);
        SPI.result = true;
    } else if (scenario == 1) {
        SD.result = false;
        assert(!ensureCardputerSdMounted());
        assert(!cardputerSdMounted());
        assert(notifications == 0);
        SD.result = true;
    }
    assert(ensureCardputerSdMounted());
    const int calls = SD.calls;
    assert(ensureCardputerSdMounted());
    assert(SD.calls == calls);
    assert(notifications == 1);
}
'''


class SdMountTests(unittest.TestCase):
    def test_failures_retry_and_success_notifies_once(self):
        with tempfile.TemporaryDirectory(prefix="grooveputer-sd-test-") as temp:
            root = Path(temp)
            for name, content in STUBS.items():
                (root / name).write_text(content)
            test = root / "test.cpp"
            test.write_text(TEST)
            binary = root / "test"
            subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17",
                            "-Wall", "-Wextra", "-Werror", "-DARDUINO",
                            f"-I{root}", f"-I{ROOT}", str(test),
                            str(ROOT / "src/platform/cardputer_sd.cpp"),
                            "-o", str(binary)], check=True)
            for scenario in range(3):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(binary), str(scenario)], check=True,
                                   cwd=temp)


if __name__ == "__main__":
    unittest.main()
