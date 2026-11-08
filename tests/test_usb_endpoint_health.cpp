#include <cassert>

#include "src/midi/usb_endpoint_health.h"

int main() {
    UsbEndpointHealth health(50);

    health.observeWrite(true, false, 100);
    auto snapshot = health.snapshot(149);
    assert(snapshot.state == UsbEndpointHealthState::Backpressured);
    assert(snapshot.currentBlockedMs == 49);
    assert(snapshot.stalledTransitions == 0);

    // A short FIFO-full chord is expected and must not become a stall.
    health.observeWrite(true, true, 150);
    snapshot = health.snapshot(150);
    assert(snapshot.state == UsbEndpointHealthState::Ready);
    assert(snapshot.stalledTransitions == 0);

    health.observeWrite(true, false, 200);
    health.observeWrite(true, false, 250);
    snapshot = health.snapshot(250);
    assert(snapshot.state == UsbEndpointHealthState::Stalled);
    assert(snapshot.stalledTransitions == 1);
    assert(snapshot.currentBlockedMs == 50);

    health.observeWrite(true, true, 260);
    snapshot = health.snapshot(260);
    assert(snapshot.state == UsbEndpointHealthState::Ready);
    assert(snapshot.recoveredTransitions == 1);
    assert(snapshot.maximumBlockedMs == 60);

    // Disconnect is not a receiver stall and must reset the pending window.
    health.observeWrite(true, false, 300);
    health.observeWrite(false, false, 400);
    snapshot = health.snapshot(400);
    assert(snapshot.state == UsbEndpointHealthState::Ready);
    assert(snapshot.currentBlockedMs == 0);

    // A stall is only a live block while rejects keep arriving. Once the host
    // goes quiet, the sticky Stalled label must stop reading as BLOCKED.
    UsbEndpointHealth quiet(50);
    quiet.observeWrite(true, false, 1000);
    quiet.observeWrite(true, false, 1060);
    assert(quiet.isFreshlyStalled(1060, 1000));
    assert(quiet.isFreshlyStalled(2060, 1000));
    assert(!quiet.isFreshlyStalled(2061, 1000));
    assert(quiet.snapshot(2061).state == UsbEndpointHealthState::Stalled);
    quiet.observeWrite(true, true, 2100);
    assert(!quiet.isFreshlyStalled(2100, 1000));
    return 0;
}
