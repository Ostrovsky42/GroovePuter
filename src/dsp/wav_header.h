#pragma once
#include <cstdint>

struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t overall_size = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt_chunk_marker[4] = {'f', 'm', 't', ' '};
    uint32_t length_of_fmt = 16;
    uint16_t format_type = 1;
    uint16_t channels = 1;
    uint32_t sample_rate = 44100;
    uint32_t byterate = 44100 * 1 * 2;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    char data_chunk_header[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size = 0;
};
