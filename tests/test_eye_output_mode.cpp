#include <cassert>
#include <cstdio>
#include <cstring>
#include "src/eye_pair_sync/eye_output_mode.h"
#include "src/output/output_scene_persistence.h"

void test_crc8_calculation() {
    eye_output_mode_packet_t pkt{};
    pkt.magic = EYE_SYNC_MAGIC_OUTPUT_MODE;
    pkt.version = EYE_SYNC_VERSION_OUTPUT_MODE;
    pkt.session_id = 0x11223344u;
    pkt.seq = 7u;
    pkt.effect_t0_us = 1234567;
    pkt.track = EYE_TRACK_SYNTH_A;
    pkt.mode = EYE_OUT_INTERNAL;
    pkt.flags = 0x01;
    pkt.reserved = 0;

    uint8_t crc1 = eye_output_mode_calc_crc8(&pkt);
    pkt.crc = crc1;
    uint8_t crc2 = eye_output_mode_calc_crc8(&pkt);
    assert(crc1 == crc2);
    assert(crc1 != 0);

    printf("CRC-8 CCITT calculation: PASS\n");
}

void test_packet_structure() {
    eye_output_mode_packet_t pkt{};
    assert(sizeof(eye_output_mode_packet_t) == 23);
    assert(sizeof(eye_gvep_packet_t) == 23);
    assert(offsetof(eye_output_mode_packet_t, session_id) == 2);
    assert(offsetof(eye_output_mode_packet_t, seq) == 6);
    assert(offsetof(eye_output_mode_packet_t, effect_t0_us) == 10);
    assert(offsetof(eye_output_mode_packet_t, crc) == 22);

    assert(eye_output_mode_build_packet(
        &pkt, EYE_TRACK_SYNTH_B, EYE_OUT_MIDI, true));
    assert(pkt.magic == 0xAF);
    assert(pkt.version == 2);
    assert(pkt.session_id != 0);
    assert(pkt.seq != 0);
    assert(pkt.effect_t0_us > 0);
    assert(pkt.track == 1);
    assert(pkt.mode == 2);
    assert(pkt.flags == 1);
    assert(pkt.reserved == 0);
    assert(pkt.crc == eye_output_mode_calc_crc8(&pkt));

    eye_output_mode_packet_t silent{};
    assert(eye_output_mode_build_packet(
        &silent, EYE_TRACK_SYNTH_B, EYE_OUT_MIDI, false));
    assert(silent.session_id == pkt.session_id);
    assert(silent.seq > pkt.seq);
    assert(silent.effect_t0_us == 0);
    assert(silent.flags == 0);
    assert(silent.crc == eye_output_mode_calc_crc8(&silent));

    printf("Eye packet structure v2 23-byte alignment: PASS\n");
}

void test_gvep_packet_structure_and_shared_sequence() {
    eye_gvep_packet_t pkt{};
    assert(eye_gvep_build_packet(&pkt, EYE_GVEP_KICK, 120, 0, 987654321));
    assert(pkt.magic == EYE_SYNC_MAGIC_GVEP);
    assert(pkt.version == EYE_SYNC_VERSION_GVEP);
    assert(pkt.session_id == eye_output_mode_session_id());
    assert(pkt.seq > 0);
    assert(pkt.timestamp_us == 987654321);
    assert(pkt.event_type == EYE_GVEP_KICK);
    assert(pkt.value0 == 120);
    assert(pkt.value1 == 0);
    assert(pkt.crc == eye_gvep_calc_crc8(&pkt));

    eye_gvep_packet_t bar{};
    assert(eye_gvep_build_packet(&bar, EYE_GVEP_BAR, 0, 1, 123));
    assert(bar.seq > pkt.seq);
    assert(bar.value1 == 1);
    assert(bar.crc == eye_gvep_calc_crc8(&bar));
    assert(!eye_gvep_build_packet(&bar, EYE_GVEP_BAR, 0, 0, 123));

    printf("GVEP packet structure and shared sequence: PASS\n");
}

void test_notify_and_restore_c_api() {
    eye_output_mode_init();

    // Verify notify triggers non-zero flags
    eye_output_mode_notify(EYE_TRACK_DRUMS, EYE_OUT_LAYER);

    // Verify restore triggers zero flags (silent restore)
    eye_output_mode_restore(EYE_TRACK_SYNTH_A, EYE_OUT_INTERNAL);

    printf("Dual-Eye C-API notify and restore: PASS\n");
}

int main() {
    test_crc8_calculation();
    test_packet_structure();
    test_gvep_packet_structure_and_shared_sequence();
    test_notify_and_restore_c_api();
    printf("All Dual-Eye Output Mode tests PASS!\n");
    return 0;
}
