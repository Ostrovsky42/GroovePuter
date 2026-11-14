// D1: the DIN enable toggle is requested from control code and applied only by
// the dispatcher. These checks cover the state machine that decides whether the
// all-notes-off recovery runs, and that readers only see applied state.
#include <cassert>

#include "src/midi/din_enable_request.h"

using GroovePuterMidi::DinEnableRequest;

int main() {
    {
        DinEnableRequest req;
        bool enabled = true;
        assert(!req.takeChange(enabled));                 // never requested
        assert(req.appliedOr(true) == true);              // fallback before any apply
        assert(req.appliedOr(false) == false);
    }
    {
        DinEnableRequest req;
        bool enabled = true;
        req.request(false);
        assert(req.takeChange(enabled) && enabled == false);
        req.markApplied(false);
        assert(req.appliedOr(true) == false);             // readers see applied state
        assert(!req.takeChange(enabled));                 // repeating off is no change
        req.request(false);
        assert(!req.takeChange(enabled));
    }
    {
        DinEnableRequest req;
        bool enabled = false;
        req.request(true);
        assert(req.takeChange(enabled) && enabled == true);
        req.markApplied(true);
        assert(req.appliedOr(false) == true);
        assert(!req.takeChange(enabled));
    }
    {
        // Two requests before the dispatcher runs: the last one wins, and only
        // one change is applied.
        DinEnableRequest req;
        bool enabled = true;
        req.request(false);
        req.request(true);
        assert(req.takeChange(enabled) && enabled == true);
        req.markApplied(true);
        assert(!req.takeChange(enabled));
    }
    {
        // A request that lands while the dispatcher is between take and mark is
        // not lost: the next pass applies it.
        DinEnableRequest req;
        bool enabled = true;
        req.request(false);
        assert(req.takeChange(enabled) && enabled == false);
        req.request(true);
        req.markApplied(false);
        assert(req.takeChange(enabled) && enabled == true);
        req.markApplied(true);
        assert(req.appliedOr(false) == true);
    }
    return 0;
}
