#include <cassert>
#include <iostream>
#include "src/input/midi_input_settings.h"
int main(){
 using namespace GroovePuterMidiInput;
 MidiInputRoutingConfig d=defaultRoutingConfig(); assert(!d.enabled); assert(d.channelMode==MidiInputChannelMode::Omni); assert(d.target==MidiInputTarget::SynthA);
 for(int target=0;target<3;++target) for(int ch=0;ch<16;++ch){ MidiInputRoutingConfig c{}; c.enabled=true; c.channelMode=MidiInputChannelMode::Single; c.channel=(uint8_t)ch; c.target=(MidiInputTarget)target; MidiInputRoutingConfig o{}; assert(decodeRoutingConfig(encodeRoutingConfig(c),o)); assert(o==c); }
 MidiInputRoutingConfig o{}; assert(!decodeRoutingConfig(0u,o)); assert(o==d); assert(!decodeRoutingConfig((uint32_t(kSettingsMagic)<<24)|(uint32_t(kSettingsVersion)<<16)|0xC0u,o)); assert(o==d);
 std::cout<<"0.9.10 R7 input settings codec: PASS\n";
}
