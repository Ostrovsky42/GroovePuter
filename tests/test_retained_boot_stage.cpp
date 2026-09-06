#include <cassert>
#include <type_traits>
#include "src/platform/retained_boot_stage.h"

int main() {
    static_assert(std::is_trivial<RetainedBootStage>::value,
                  "RTC evidence must have no startup constructor");
    RetainedBootStage record{};
    assert(record.previous(true) == 0);
    record.record(10);
    assert(record.previous(true) == 10);
    assert(record.previous(false) == 0);  // Never trust a power-on record.
    record.stage = 11;  // Interrupted/corrupted update.
    assert(record.previous(true) == 0);
    record.record(100);
    assert(record.previous(true) == 100);
    record.magic = 0;
    assert(record.previous(true) == 0);
}
