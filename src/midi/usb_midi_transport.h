#pragma once
#ifndef GROOVEPUTER_USB_MIDI_TRANSPORT_H
#define GROOVEPUTER_USB_MIDI_TRANSPORT_H

#include "midi_transport.h"

// The output boundary was already device independent in substance - no USB
// type appears in any signature - and only its name said USB. It now lives in
// midi_transport.h as IMidiTransport so a DIN/UART endpoint can implement the
// same contract.
//
// This alias keeps existing call sites and test doubles compiling. New code
// should name IMidiTransport directly.
using IUsbMidiTransport = IMidiTransport;

#endif  // GROOVEPUTER_USB_MIDI_TRANSPORT_H
