// Properties P1..P6 of src/midi/smf_project_gate_policy.h (design:
// docs/midi/2026-11-10-snapshot-miss-and-smf-retrigger-design.md, part B).
// The dispatcher loop itself runs on FreeRTOS and has no host test; this
// covers the pure policy and its bounded re-read driver.
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/midi/smf_project_gate_policy.h"

static constexpr uint8_t kMaxImmediateRereads = kSmfProjectGateMaxImmediateRereads;

using Type = ScheduledSmfMidiEventType;

int main() {
    const Type types[] = {Type::NoteOn, Type::NoteOff, Type::SongPositionPointer};
    const SmfGatePhase phases[] = {SmfGatePhase::Early, SmfGatePhase::Late};
    for (Type type : types) {
        for (SmfGatePhase phase : phases) {
            for (int verified = 0; verified < 2; ++verified) {
                for (uint8_t done = 0; done <= kMaxImmediateRereads + 1; ++done) {
                    const auto a = smfProjectGateAction(type, phase, SmfProjectRead::Unavailable, verified != 0, done);
                    // P1: only a NoteOn may ask for a re-read, and only within the bound.
                    if (a == SmfProjectGateAction::ReadAgainNow) {
                        assert(type == Type::NoteOn && done < kMaxImmediateRereads);
                    }
                    // P2: NoteOff and SPP are never dropped or delayed by a miss.
                    if (type != Type::NoteOn) assert(a == SmfProjectGateAction::Dispatch);
                    // P3: a miss alone never drops a NoteOn that an earlier read verified.
                    if (type == Type::NoteOn && verified != 0) assert(a != SmfProjectGateAction::DropStale);
                }
            }
        }
    }
    // P4: the Early phase never drops on a miss (the Late phase decides).
    assert(smfProjectGateAction(Type::NoteOn, SmfGatePhase::Early, SmfProjectRead::Unavailable, false,
                                kMaxImmediateRereads) == SmfProjectGateAction::Dispatch);
    // P5: a definite STOP/other epoch still drops a NoteOn.
    assert(smfProjectGateAction(Type::NoteOn, SmfGatePhase::Late, SmfProjectRead::NotPlaying, true, 0) ==
           SmfProjectGateAction::DropStale);
    assert(smfProjectGateAction(Type::NoteOn, SmfGatePhase::Late, SmfProjectRead::PlayingOtherEpoch, true, 0) ==
           SmfProjectGateAction::DropStale);
    // P6: late miss, never verified, budget spent: dropped, but as its own counter
    // (smfSnapshotMissDrops), not as an epoch drop.
    assert(smfProjectGateAction(Type::NoteOn, SmfGatePhase::Late, SmfProjectRead::Unavailable, false,
                                kMaxImmediateRereads) == SmfProjectGateAction::DropStale);
    
    // Driver: bounded re-reads, no unbounded loop, no sleep.
    {
        int calls = 0;
        auto r = resolveSmfProjectGate(Type::NoteOn, SmfGatePhase::Late, false, [&]() {
            ++calls;
            return SmfProjectRead::Unavailable;
        });
        assert(calls == 1 + kMaxImmediateRereads);
        assert(r.action == SmfProjectGateAction::DropStale && r.missDrop);
    }
    {
        int calls = 0;
        auto r = resolveSmfProjectGate(Type::NoteOn, SmfGatePhase::Late, false, [&]() {
            return ++calls == 1 ? SmfProjectRead::Unavailable
                                : SmfProjectRead::PlayingCurrentEpoch;
        });
        assert(calls == 2 && r.action == SmfProjectGateAction::Dispatch && r.verifiedCurrent);
    }
    {  // verified earlier (Early phase), late read misses: not lost.
        auto r = resolveSmfProjectGate(Type::NoteOn, SmfGatePhase::Late, true, [&]() {
            return SmfProjectRead::Unavailable;
        });
        assert(r.action == SmfProjectGateAction::Dispatch && !r.missDrop);
    }
    {  // NoteOff: one read, never waits, never dropped by a miss.
        int calls = 0;
        auto r = resolveSmfProjectGate(Type::NoteOff, SmfGatePhase::Early, false, [&]() {
            ++calls;
            return SmfProjectRead::Unavailable;
        });
        assert(calls == 1 && r.action == SmfProjectGateAction::Dispatch);
    }
    {  // Definite STOP is an epoch drop, not a miss drop.
        auto r = resolveSmfProjectGate(Type::NoteOn, SmfGatePhase::Late, true, [&]() {
            return SmfProjectRead::NotPlaying;
        });
        assert(r.action == SmfProjectGateAction::DropStale && !r.missDrop);
    }
    assert(smfProjectGateClassify(false, true, true, true) == SmfProjectRead::Unavailable);
    assert(smfProjectGateClassify(true, false, true, true) == SmfProjectRead::NotPlaying);
    assert(smfProjectGateClassify(true, true, false, true) == SmfProjectRead::NotPlaying);
    assert(smfProjectGateClassify(true, true, true, false) == SmfProjectRead::PlayingOtherEpoch);
    assert(smfProjectGateClassify(true, true, true, true) == SmfProjectRead::PlayingCurrentEpoch);
    std::puts("smf project gate policy: PASS");
    return 0;
}
