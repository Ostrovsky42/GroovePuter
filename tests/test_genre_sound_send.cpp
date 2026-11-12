// H8: the sound send. One step per frame; each request is written only after the
// dispatcher has published the previous one; the dispatcher applies every request
// (none lost), in order; the controller selected before the send is selected again.
#include <cassert>
#include <cstdio>

#include "src/midi/genre_sound_preset.h"

using namespace GenreSound;
using GroovePuterMidi::CutoffControlChannel;
using GroovePuterMidi::CutoffRequest;

namespace {
// The dispatcher as cardputer_usb_midi_transport applies it: a request word is applied
// once; kManual selects the controller and sets the value; the state is published.
struct FakeDispatcher {
    struct Applied {
        uint8_t voice;
        uint8_t op;
        uint8_t controller;
        uint8_t value;
    };
    Applied log[kStepsPerGenre * 4]{};
    int logged = 0;
    uint32_t applied[kVoiceCount]{};
    uint8_t selected[kVoiceCount]{};
    uint8_t value[kVoiceCount]{};
    bool known[kVoiceCount]{};

    void tick(CutoffControlChannel (&channels)[kVoiceCount]) {
        for (uint8_t v = 0; v < kVoiceCount; ++v) {
            const uint32_t word = channels[v].loadRequest();
            if (word == 0u || word == applied[v]) continue;
            applied[v] = word;
            const uint8_t op = static_cast<uint8_t>(CutoffRequest::op(word));
            log[logged++] = Applied{v, op, CutoffRequest::controller(word), CutoffRequest::value(word)};
            if (op == CutoffRequest::kSelectController) {
                selected[v] = CutoffRequest::controller(word);
            } else if (op == CutoffRequest::kManual) {
                selected[v] = CutoffRequest::controller(word);
                value[v] = CutoffRequest::value(word);
                known[v] = true;
            }
            channels[v].publish(value[v], false, 0, known[v], selected[v], false);
        }
    }
};
}  // namespace

int main() {
    CutoffControlChannel channels[kVoiceCount];
    // Before the send the synth page has CUTOFF selected on A and DELAY on B.
    channels[0].request(CutoffRequest::kSelectController, 0, 0);
    channels[1].request(CutoffRequest::kSelectController, 0, 4);

    const uint8_t modes[] = {9, 13, 10, 0, 5, 6};  // House, UKG, Techno, Acid, Dub, Trip-Hop
    for (uint8_t mode : modes) {
        FakeDispatcher dispatcher;
        dispatcher.tick(channels);  // the selection is published first
        dispatcher.logged = 0;      // and is not part of the send's log
        SoundSender sender;
        assert(sender.start(mode, channels));

        Step expected[kStepsPerGenre];
        assert(stepsFor(mode, expected, kStepsPerGenre) == kStepsPerGenre);

        int frames = 0;
        while (sender.sending() && frames < 500) {
            const int before = sender.written();
            sender.pump(channels);
            // One request per call, and only after the dispatcher has the previous one.
            assert(sender.written() - before <= 1);
            if (sender.written() != before) {
                // Written only after the dispatcher applied all earlier steps.
                assert(dispatcher.logged == before);
            }
            dispatcher.tick(channels);
            ++frames;
        }
        assert(!sender.sending());
        assert(sender.written() == kStepsPerGenre);

        // Every step reached the dispatcher once, in order, with its value and voice.
        // 10 manual steps, then at most one restore per voice whose selection changed.
        assert(dispatcher.logged >= kStepsPerGenre && dispatcher.logged <= kStepsPerGenre + 2);
        for (int i = 0; i < kStepsPerGenre; ++i) {
            assert(dispatcher.log[i].op == CutoffRequest::kManual);
            assert(dispatcher.log[i].voice == expected[i].voice);
            assert(dispatcher.log[i].controller == expected[i].controller);
            assert(dispatcher.log[i].value == expected[i].value);
        }
        // The restores put the controller back: CUTOFF on A, DELAY on B.
        for (int i = kStepsPerGenre; i < dispatcher.logged; ++i) {
            assert(dispatcher.log[i].op == CutoffRequest::kSelectController);
            assert(dispatcher.log[i].controller == (dispatcher.log[i].voice == 0 ? 0 : 4));
        }
        assert(dispatcher.selected[0] == 0 && dispatcher.selected[1] == 4);
    }

    // The selection a page sees after the send is the one it had before.
    assert(channels[0].controller() == 0);
    assert(channels[1].controller() == 4);

    // A genre without a preset starts nothing.
    SoundSender none;
    assert(!none.start(2, channels) && !none.sending());
    std::printf("genre sound send: OK (one step per frame, no lost request, selection restored)\n");
    return 0;
}
