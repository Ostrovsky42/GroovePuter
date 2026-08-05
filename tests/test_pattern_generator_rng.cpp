#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "src/dsp/pattern_generator.h"

#if __has_include("src/dsp/deterministic_rng.h")
#include "src/dsp/deterministic_rng.h"
#define GROOVEPUTER_HAS_DETERMINISTIC_RNG 1
#else
#define GROOVEPUTER_HAS_DETERMINISTIC_RNG 0
#endif

namespace {
constexpr size_t kSequenceLength = 32;
constexpr size_t kGrooveboxGenerationStageCount = 3;
constexpr size_t kGrooveboxDrawsPerStage = 32;
using PatternSequence = std::array<uint32_t, kSequenceLength>;
using GrooveboxGlobalRngTrace =
    std::array<std::array<int, kGrooveboxDrawsPerStage>,
               kGrooveboxGenerationStageCount>;
int g_failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++g_failures;
    }
}

PatternSequence generateSequence(uint32_t seed, bool disturbGlobalAfterSeed) {
    SmartPatternGenerator generator;
    generator.setSeed(seed);
    if (disturbGlobalAfterSeed) {
        std::srand(999);
        for (int i = 0; i < 100; ++i) {
            (void)std::rand();
        }
    }

    PatternSequence sequence{};
    for (uint32_t& value : sequence) {
        value = generator.generatePattern(
            SmartPatternGenerator::PG_RANDOM,
            GenerativeMode::Acid,
            0);
    }
    return sequence;
}

GrooveboxGlobalRngTrace captureGrooveboxGlobalRngTrace(
    unsigned int bootSeed,
    bool openSongPage) {
    std::srand(bootSeed);

    if (openSongPage) {
        SmartPatternGenerator songPageGenerator;
        (void)songPageGenerator;
    }

    // PR1 boundary: GrooveboxModeManager still consumes the global libc RNG.
    // These three blocks model the Generate A -> B -> Drums call order that
    // PR2 will isolate. This test intentionally compares state continuity,
    // not platform-specific rand() golden values.
    GrooveboxGlobalRngTrace trace{};
    for (auto& stage : trace) {
        for (int& value : stage) {
            value = std::rand();
        }
    }
    return trace;
}

void testSameSeedSameCalls() {
    expect(generateSequence(123, false) == generateSequence(123, false),
           "same seed and call order must produce the same sequence");
}

void testExternalGlobalCallsDoNotInterfere() {
    const PatternSequence clean = generateSequence(123, false);
    const PatternSequence disturbed = generateSequence(123, true);
    expect(clean == disturbed,
           "external rand() calls between setSeed() and generation changed the sequence");
}

void testInstancesAreIndependent() {
    const PatternSequence expectedA = generateSequence(111, false);
    const PatternSequence expectedB = generateSequence(222, false);

    SmartPatternGenerator generatorA;
    SmartPatternGenerator generatorB;
    generatorA.setSeed(111);
    generatorB.setSeed(222);

    PatternSequence actualA{};
    PatternSequence actualB{};
    for (size_t i = 0; i < kSequenceLength; ++i) {
        actualA[i] = generatorA.generatePattern(
            SmartPatternGenerator::PG_RANDOM,
            GenerativeMode::Acid,
            0);
        actualB[i] = generatorB.generatePattern(
            SmartPatternGenerator::PG_RANDOM,
            GenerativeMode::Acid,
            0);
    }

    expect(actualA == expectedA,
           "the first SmartPatternGenerator instance was changed by the second instance");
    expect(actualB == expectedB,
           "the second SmartPatternGenerator instance was changed by the first instance");
}

void testDifferentSeedsProduceDifferentSequences() {
    expect(generateSequence(123, false) != generateSequence(456, false),
           "different seeds unexpectedly produced identical sequences");
}

void testConstructorDoesNotChangeGlobalRand() {
    std::srand(777);
    const int before = std::rand();

    std::srand(777);
    { SmartPatternGenerator generator; }
    const int after = std::rand();

    expect(before == after,
           "SmartPatternGenerator construction changed the global rand() sequence");
}

void testOpeningSongPagePreservesGrooveboxBootSequence() {
    constexpr unsigned int kBootSeed = 0x13579BDFu;
    const GrooveboxGlobalRngTrace withoutSongPage =
        captureGrooveboxGlobalRngTrace(kBootSeed, false);
    const GrooveboxGlobalRngTrace afterOpeningSongPage =
        captureGrooveboxGlobalRngTrace(kBootSeed, true);

    expect(afterOpeningSongPage == withoutSongPage,
           "opening Song Page reset the boot-seeded global RNG sequence consumed by GrooveboxModeManager");
}

void testSetSeedDoesNotChangeGlobalRand() {
    SmartPatternGenerator generator;

    std::srand(777);
    const int before = std::rand();

    std::srand(777);
    generator.setSeed(42);
    const int after = std::rand();

    expect(before == after,
           "SmartPatternGenerator::setSeed() changed the global rand() sequence");
}

void testExplicitReseedRestoresSequence() {
    SmartPatternGenerator generator;
    generator.setSeed(123);
    const uint32_t first = generator.generatePattern(
        SmartPatternGenerator::PG_RANDOM,
        GenerativeMode::Acid,
        0);

    std::srand(999);
    for (int i = 0; i < 100; ++i) {
        (void)std::rand();
    }

    generator.setSeed(123);
    const uint32_t second = generator.generatePattern(
        SmartPatternGenerator::PG_RANDOM,
        GenerativeMode::Acid,
        0);

    expect(first == second,
           "explicit reseeding did not restore the initial generator result");
}

#if GROOVEPUTER_HAS_DETERMINISTIC_RNG
void testDeterministicRngGoldenSequence() {
    DeterministicRng rng(0x12345678u);
    constexpr std::array<uint32_t, 6> expected = {
        0x87985AA5u,
        0x155B24A3u,
        0x4820F4C4u,
        0x81B3AC98u,
        0x703A0788u,
        0x29A8E24Du,
    };
    for (uint32_t value : expected) {
        expect(rng.next() == value,
               "xorshift32 sequence differs from the platform-independent golden values");
    }
}

void testZeroSeedIsNotDegenerate() {
    DeterministicRng rng(0);
    constexpr std::array<uint32_t, 3> expected = {
        0x40AEC71Fu,
        0x91E00C19u,
        0x9C0FE128u,
    };
    for (uint32_t value : expected) {
        expect(rng.next() == value,
               "zero-seed normalization differs from the documented golden values");
    }
}
#endif
}  // namespace

int main() {
    testSameSeedSameCalls();
    testExternalGlobalCallsDoNotInterfere();
    testInstancesAreIndependent();
    testDifferentSeedsProduceDifferentSequences();
    testConstructorDoesNotChangeGlobalRand();
    testOpeningSongPagePreservesGrooveboxBootSequence();
    testSetSeedDoesNotChangeGlobalRand();
    testExplicitReseedRestoresSequence();
#if GROOVEPUTER_HAS_DETERMINISTIC_RNG
    testDeterministicRngGoldenSequence();
    testZeroSeedIsNotDegenerate();
#endif

    if (g_failures != 0) {
        std::fprintf(stderr,
                     "SmartPatternGenerator RNG tests failed: %d\n",
                     g_failures);
        return 1;
    }

    std::puts("SmartPatternGenerator RNG tests passed");
    return 0;
}
