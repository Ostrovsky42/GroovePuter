#ifndef EYE_OUTPUT_MODE_H
#define EYE_OUTPUT_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Cardputer ADV is DRAM-only. ESP-NOW remains an explicit experimental opt-in
// until the hardware memory and audio-jitter gates pass.
#ifndef GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW
#define GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW 0
#endif

// R0.1 is a hardware-only memory feasibility probe. It records internal-heap
// snapshots and AudioTask stack high-water mark while the experimental radio
// is enabled. Keep disabled in normal builds.
#ifndef GROOVEPUTER_GVEP_R01_MEMORY_PROBE
#define GROOVEPUTER_GVEP_R01_MEMORY_PROBE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EYE_SYNC_MAGIC_OUTPUT_MODE 0xAF
#define EYE_SYNC_VERSION_OUTPUT_MODE 2
#define EYE_SYNC_MAGIC_GVEP 0xB0
#define EYE_SYNC_VERSION_GVEP 2

typedef enum {
    EYE_TRACK_SYNTH_A = 0,
    EYE_TRACK_SYNTH_B = 1,
    EYE_TRACK_DRUMS   = 2
} eye_track_t;

typedef enum {
    EYE_OUT_LEGACY   = 0,
    EYE_OUT_INTERNAL = 1,
    EYE_OUT_MIDI     = 2,
    EYE_OUT_LAYER    = 3
} eye_output_mode_t;

typedef struct __attribute__((packed)) {
    uint8_t magic;         // EYE_SYNC_MAGIC_OUTPUT_MODE (0xAF)
    uint8_t version;       // EYE_SYNC_VERSION_OUTPUT_MODE (2)
    uint32_t session_id;   // Randomized Master boot session ID
    uint32_t seq;          // Shared monotonic visual-stream sequence
    int64_t effect_t0_us;  // Master esp_timer_get_time() for animated effects
    uint8_t track;         // 0=SYNTH_A, 1=SYNTH_B, 2=DRUMS
    uint8_t mode;          // 0=LEGACY, 1=INTERNAL, 2=MIDI, 3=LAYER
    uint8_t flags;         // Bit 0: animate (1=notify, 0=silent restore)
    uint8_t reserved;      // Must be zero
    uint8_t crc;           // CRC-8-CCITT over the first 22 bytes
} eye_output_mode_packet_t;

typedef enum {
    EYE_GVEP_TRANSPORT = 0,
    EYE_GVEP_KICK = 1,
    EYE_GVEP_BAR = 2,
} eye_gvep_event_type_t;

typedef struct __attribute__((packed)) {
    uint8_t magic;         // EYE_SYNC_MAGIC_GVEP (0xB0)
    uint8_t version;       // EYE_SYNC_VERSION_GVEP (2)
    uint32_t session_id;   // Randomized Master boot session ID
    uint32_t seq;          // Shared monotonic visual-stream sequence
    int64_t timestamp_us;  // Master esp_timer_get_time() event timestamp
    uint8_t event_type;    // EYE_GVEP_TRANSPORT/KICK/BAR
    uint8_t value0;        // Transport state or kick velocity
    uint16_t value1;       // Bar number for EYE_GVEP_BAR
    uint8_t crc;           // CRC-8-CCITT over the first 22 bytes
} eye_gvep_packet_t;

typedef struct {
    uint32_t send_attempts;
    uint32_t send_accepted;
    uint32_t send_rejected;
    uint32_t queue_dropped;
    uint32_t radio_init_failures;
    uint32_t free_internal_before_radio;
    uint32_t largest_internal_before_radio;
    uint32_t free_internal_after_radio;
    uint32_t largest_internal_after_radio;
    uint32_t audio_stack_hwm_bytes;
    uint8_t radio_init_attempted;
    uint8_t radio_ready;
} eye_transport_diagnostics_t;

#ifdef __cplusplus
static_assert(sizeof(eye_output_mode_packet_t) == 23,
              "Output Mode v2 packet layout changed");
static_assert(offsetof(eye_output_mode_packet_t, session_id) == 2,
              "Output Mode session_id offset changed");
static_assert(offsetof(eye_output_mode_packet_t, seq) == 6,
              "Output Mode seq offset changed");
static_assert(offsetof(eye_output_mode_packet_t, effect_t0_us) == 10,
              "Output Mode effect_t0_us offset changed");
static_assert(offsetof(eye_output_mode_packet_t, crc) == 22,
              "Output Mode crc offset changed");
static_assert(sizeof(eye_gvep_packet_t) == 23,
              "GVEP packet layout changed");
#else
_Static_assert(sizeof(eye_output_mode_packet_t) == 23,
               "Output Mode v2 packet layout changed");
_Static_assert(sizeof(eye_gvep_packet_t) == 23,
               "GVEP packet layout changed");
#endif

void eye_output_mode_init(void);
void eye_output_mode_flush(void);
bool eye_output_mode_transport_enabled(void);
eye_transport_diagnostics_t eye_output_mode_transport_diagnostics(void);
uint32_t eye_output_mode_session_id(void);
bool eye_output_mode_build_packet(eye_output_mode_packet_t* packet,
                                   eye_track_t track,
                                   eye_output_mode_t mode,
                                   bool animate);
void eye_output_mode_notify(eye_track_t track, eye_output_mode_t mode);
void eye_output_mode_restore(eye_track_t track, eye_output_mode_t mode);

uint8_t eye_output_mode_calc_crc8(const eye_output_mode_packet_t* pkt);
uint8_t eye_gvep_calc_crc8(const eye_gvep_packet_t* pkt);
bool eye_gvep_build_packet(eye_gvep_packet_t* packet,
                           eye_gvep_event_type_t event_type,
                           uint8_t value0,
                           uint16_t value1,
                           int64_t timestamp_us);
void eye_gvep_notify_transport(bool is_playing);
void eye_gvep_notify_kick(uint8_t velocity);
void eye_gvep_notify_bar(uint16_t bar_number);
void eye_gvep_notify_transport_at(bool is_playing, int64_t timestamp_us);
void eye_gvep_notify_kick_at(uint8_t velocity, int64_t timestamp_us);
void eye_gvep_notify_bar_at(uint16_t bar_number, int64_t timestamp_us);

#ifdef __cplusplus
}
#endif

#endif // EYE_OUTPUT_MODE_H
