#include <cassert>
#include <iostream>
#include "src/input/midi_input_router.h"
class Sink final : public IMusicalEventSink { public: void handleMusicalEvent(const MusicalEvent&) override {} };
int main() {
 MusicalEventRouter fanout; Sink sink; assert(fanout.addSink(sink)); MidiInputRouter r(fanout);
 MidiInputRoutingConfig c{}; c.enabled=true; assert(r.setConfig(c));
 NormalizedMidiInputMessage n{}; assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(0x90,48,100,1,1,1,n)); assert(r.handle(n));
 const auto active=r.activeNoteCount(); const auto ignored=r.diagnostics().ignoredUnsupported;
 NormalizedMidiInputMessage cc{}; assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(0xB0,64,127,1,1,2,cc)); assert(!r.handle(cc)); assert(r.activeNoteCount()==active);
 NormalizedMidiInputMessage bend{}; assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(0xE0,0,64,1,1,3,bend)); assert(!r.handle(bend)); assert(r.activeNoteCount()==active);
 assert(r.diagnostics().ignoredUnsupported==ignored+2); std::cout<<"0.9.10 R6 controller boundary: PASS\n"; }
