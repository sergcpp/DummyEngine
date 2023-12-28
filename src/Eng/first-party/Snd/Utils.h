#pragma once

#include <cstdint>

#include <istream>
#include <memory>

namespace Snd {
enum class eWaveFormat : uint16_t {
    PCM = 0x0001,
    IEEE_FLOAT = 0x0003,
    ALAW = 0x0006,
    MULAW = 0x0007,
    EXTENSIBLE = 0xFFFE
};

struct WaveChunk {
    char id[4];
    uint32_t size;
    eWaveFormat format;
    uint16_t channels_count;
    uint32_t samples_per_second;
    uint32_t bytes_per_second;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint16_t ext_size;
    uint16_t valid_bits_per_sample;
    uint32_t channel_mask;
    char sub_format[16];
};
static_assert(sizeof(WaveChunk) == 48, "!");

int LoadWAV(std::istream &in_data, int &channels, int &samples_per_second,
            int &bits_per_sample, std::unique_ptr<uint8_t[]> &samples);
} // namespace Snd