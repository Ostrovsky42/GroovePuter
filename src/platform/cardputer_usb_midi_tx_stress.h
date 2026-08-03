#pragma once

#include <cstddef>

class IUsbMidiTransport;

// Initializes the bounded producer queue and timer. The timer only enqueues;
// MidiDispatchTask remains the sole physical USB writer.
bool beginCardputerUsbMidiTxStress(void (*notifyDispatcher)());

// Called only by MidiDispatchTask. Normal scheduled/control traffic retains
// priority because the transport invokes this drain only from idle branches.
void drainCardputerUsbMidiTxStress(IUsbMidiTransport& transport,
                         std::size_t budget = 8);
