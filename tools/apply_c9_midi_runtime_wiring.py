#!/usr/bin/env python3
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count == 0 and new in text:
        return text
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)

root = Path(__file__).resolve().parents[1]

# Product USB-MIDI runtime wiring.
p = root / "src/platform/cardputer_usb_midi_transport.cpp"
s = p.read_text()
s = replace_once(
    s,
    '#include "src/midi/midi_control_event_queue.h"\n',
    '#include "src/midi/midi_control_event_queue.h"\n'
    '#include "src/midi/midi_input_dispatcher.h"\n'
    '#include "src/midi/midi_input_parser.h"\n'
    '#include "src/midi/midi_input_queue.h"\n'
    '#include "src/midi/midi_io_state.h"\n',
    "midi input includes")
s = replace_once(
    s,
    'MidiControlEventQueue g_controlQueue;\n',
    'MidiControlEventQueue g_controlQueue;\n'
    'MidiInputQueue g_inputQueue;\n'
    'MidiInputParser g_inputParser;\n'
    'MidiIoState g_midiIoState;\n'
    'MidiInputDispatcher g_inputDispatcher;\n'
    'bool g_usbInputMounted = false;\n',
    "midi input globals")
old_drain = '''void drainIncomingMidiPackets() {
    if (g_externalTransportQueue == nullptr) return;

    midiEventPacket_t packet{};
    for (std::size_t drained = 0;
         drained < kMidiRxDrainBudget && g_transport.readPacket(packet);
         ++drained) {
        ExternalMidiTransportEventType type{};
        if (!GroovePuterMidi::parseUsbMidiRealtimeTransport(
                packet.header, packet.byte1, type)) {
            ++g_diagnostics.externalRxIgnored;
            continue;
        }
        if (GroovePuterMidi::transportClockRuntime().source() !=
            GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
            ++g_diagnostics.externalRxMasterIgnored;
            continue;
        }

        const uint32_t receivedAtMicros = micros();
        switch (type) {
            case ExternalMidiTransportEventType::Clock:
                ++g_externalRxPulseOrdinal;
                if (g_externalTransportQueue->tryPushClock(
                        receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxClock;
                }
                break;
            case ExternalMidiTransportEventType::Start:
                if (g_externalTransportQueue->tryPushCritical(
                        type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxStart;
                }
                break;
            case ExternalMidiTransportEventType::Continue:
                if (g_externalTransportQueue->tryPushCritical(
                        type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxContinue;
                }
                break;
            case ExternalMidiTransportEventType::Stop:
                if (g_externalTransportQueue->tryPushCritical(
                        type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxStop;
                }
                break;
        }
    }
}
'''
new_drain = '''void syncUsbMidiInputLifecycle() {
    const bool mounted = g_transport.mounted();
    if (mounted && !g_usbInputMounted) {
        g_midiIoState.usbAttached();
        g_midiIoState.usbReady(true, true);
        g_inputParser.reset(InputSession{
            InputSource::Usb, g_midiIoState.usbInputGeneration()});
        g_usbInputMounted = true;
    } else if (!mounted && g_usbInputMounted) {
        g_midiIoState.usbDetached();
        g_inputParser.reset(InputSession{
            InputSource::Usb, g_midiIoState.usbInputGeneration()});
        g_usbInputMounted = false;
    }
}

void drainIncomingMidiPackets() {
    if (g_externalTransportQueue == nullptr) return;

    syncUsbMidiInputLifecycle();
    midiEventPacket_t packet{};
    for (std::size_t drained = 0;
         drained < kMidiRxDrainBudget && g_transport.readPacket(packet);
         ++drained) {
        ExternalMidiTransportEventType type{};
        if (GroovePuterMidi::parseUsbMidiRealtimeTransport(
                packet.header, packet.byte1, type)) {
            if (GroovePuterMidi::transportClockRuntime().source() !=
                GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
                ++g_diagnostics.externalRxMasterIgnored;
                continue;
            }

            const uint32_t receivedAtMicros = micros();
            switch (type) {
                case ExternalMidiTransportEventType::Clock:
                    ++g_externalRxPulseOrdinal;
                    if (g_externalTransportQueue->tryPushClock(
                            receivedAtMicros, g_externalRxPulseOrdinal)) {
                        ++g_diagnostics.externalRxClock;
                    }
                    break;
                case ExternalMidiTransportEventType::Start:
                case ExternalMidiTransportEventType::Continue:
                case ExternalMidiTransportEventType::Stop:
                    if (g_externalTransportQueue->tryPushCritical(
                            type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                        if (type == ExternalMidiTransportEventType::Start) ++g_diagnostics.externalRxStart;
                        else if (type == ExternalMidiTransportEventType::Continue) ++g_diagnostics.externalRxContinue;
                        else ++g_diagnostics.externalRxStop;
                    }
                    break;
            }
            continue;
        }

        const uint8_t raw[4] = {
            packet.header, packet.byte1, packet.byte2, packet.byte3};
        const ParseResult parsed = g_inputParser.usbPacket(raw, micros());
        if (parsed.hasInput) {
            if (!g_inputQueue.tryPush(parsed.input)) {
                ++g_diagnostics.externalRxIgnored;
            }
            continue;
        }
        ++g_diagnostics.externalRxIgnored;
    }
    (void)g_inputDispatcher.service(g_inputQueue, kMidiRxDrainBudget);
}
'''
s = replace_once(s, old_drain, new_drain, "incoming MIDI drain")
s = replace_once(
    s,
    '    g_patternQueue = &patternQueue;\n    g_externalTransportQueue = &externalTransportQueue;\n',
    '    g_patternQueue = &patternQueue;\n    g_externalTransportQueue = &externalTransportQueue;\n'
    '    g_midiIoState.setRoutes(MidiRoutes{true, false, true, true});\n'
    '    g_midiIoState.requestUsbRole(UsbRole::Device);\n'
    '    g_midiIoState.boot();\n'
    '    g_inputDispatcher.bind(router, g_midiIoState);\n'
    '    g_inputParser.reset(InputSession{\n'
    '        InputSource::Usb, g_midiIoState.usbInputGeneration()});\n',
    "register input runtime")
p.write_text(s)

# MidiInput is a valid local-drum source, matching the persisted A/B/Drums policy.
p = root / "src/input/internal_synth_output.cpp"
s = p.read_text()
s = replace_once(
    s,
    '    return source == MusicalEventSource::PerformanceKeyboard ||\n'
    '           source == MusicalEventSource::PerformanceKeyboardPoly ||\n'
    '           source == MusicalEventSource::Arpeggiator;\n',
    '    return source == MusicalEventSource::PerformanceKeyboard ||\n'
    '           source == MusicalEventSource::PerformanceKeyboardPoly ||\n'
    '           source == MusicalEventSource::Arpeggiator ||\n'
    '           source == MusicalEventSource::MidiInput;\n',
    "MidiInput local drums")
p.write_text(s)

# Expose a narrow configuration seam. Default remains OFF; UI/persistence owns enabling.
p = root / "src/platform/cardputer_usb_midi_service.h"
s = p.read_text()
s = replace_once(
    s,
    '#include <cstdint>\n',
    '#include <cstdint>\n\n#include "src/midi/midi_input_dispatcher.h"\n',
    "service input include")
s = replace_once(
    s,
    'void registerCardputerSmfMidiQueue(ScheduledSmfMidiEventQueue* queue);\n',
    'void registerCardputerSmfMidiQueue(ScheduledSmfMidiEventQueue* queue);\n\n'
    'GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig();\n'
    'bool setCardputerMidiInputRoutingConfig(\n'
    '    const GroovePuterMidi::MidiInputRoutingConfig& config);\n',
    "service input config declarations")
p.write_text(s)

p = root / "src/platform/cardputer_usb_midi_transport.cpp"
s = p.read_text()
s = replace_once(
    s,
    'void registerCardputerSmfMidiQueue(ScheduledSmfMidiEventQueue* queue) {\n',
    'GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {\n'
    '    return g_inputDispatcher.config();\n'
    '}\n\n'
    'bool setCardputerMidiInputRoutingConfig(\n'
    '        const GroovePuterMidi::MidiInputRoutingConfig& config) {\n'
    '    return g_inputDispatcher.setConfig(config);\n'
    '}\n\n'
    'void registerCardputerSmfMidiQueue(ScheduledSmfMidiEventQueue* queue) {\n',
    "service input config definitions")
p.write_text(s)

print("C9 MIDI runtime wiring applied")
