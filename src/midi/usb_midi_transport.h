#pragma once
#ifndef GROOVEPUTER_USB_MIDI_TRANSPORT_H
#define GROOVEPUTER_USB_MIDI_TRANSPORT_H

#include "midi_transport.h"

// Compatibility alias: the output contract is transport-neutral, while the
// existing 0.9.9 call sites may continue to name the historical USB boundary.
using IUsbMidiTransport = IMidiTransport;

#endif  // GROOVEPUTER_USB_MIDI_TRANSPORT_H
