#include <cassert>
#include <cstring>

#include "src/ui/pages/smf_panic_toast_policy.h"

int main() {
    // Nothing loaded: the caller must not have been able to claim success,
    // regardless of whether the command queue happened to accept the enqueue.
    assert(std::strcmp(smfPanicToastMessage(false, true),
                       "PANIC: NO FILE LOADED") == 0);
    assert(std::strcmp(smfPanicToastMessage(false, false),
                       "PANIC: NO FILE LOADED") == 0);

    // Loaded and the command queue accepted it: the original confirmation.
    assert(std::strcmp(smfPanicToastMessage(true, true),
                       "MIDI PANIC / PAUSE") == 0);

    // Loaded but the command queue was full: unchanged busy message.
    assert(std::strcmp(smfPanicToastMessage(true, false),
                       "PANIC QUEUE BUSY") == 0);

    return 0;
}
