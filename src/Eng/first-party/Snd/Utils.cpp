#include "Utils.h"

int Snd::LoadWAV(std::istream &in_data, int &channels, int &samples_per_second,
                 int &bits_per_sample, std::unique_ptr<uint8_t[]> &samples) {
    { // check identifiers
        char chunk_id[4];
        if (!in_data.read(chunk_id, 4)) {
            return 0;
        }
        if (chunk_id[0] != 'R' || chunk_id[1] != 'I' || chunk_id[2] != 'F' ||
            chunk_id[3] != 'F') {
            return 0;
        }

        uint32_t chunk_size;
        if (!in_data.read((char *)&chunk_size, sizeof(uint32_t))) {
            return 0;
        }

        char wave_id[4];
        if (!in_data.read(wave_id, 4)) {
            return 0;
        }
        if (wave_id[0] != 'W' || wave_id[1] != 'A' || wave_id[2] != 'V' ||
            wave_id[3] != 'E') {
            return 0;
        }
    }

    WaveChunk chunk = {};
    if (!in_data.read((char *)&chunk, 8)) {
        return 0;
    }
    if (chunk.id[0] != 'f' || chunk.id[1] != 'm' || chunk.id[2] != 't' ||
        chunk.id[3] != ' ') {
        return 0;
    }

    if (!in_data.read((char *)&chunk.format, chunk.size)) {
        return 0;
    }

    if (chunk.format != eWaveFormat::PCM) {
        // not supported
        return 0;
    }

    char chunk_id[4];
    if (!in_data.read(chunk_id, 4)) {
        return 0;
    }

    if (chunk_id[0] == 'f' && chunk_id[1] == 'a' && chunk_id[2] == 'c' &&
        chunk_id[3] == 't') {
        uint32_t chunk_size;
        if (!in_data.read((char *)&chunk_size, sizeof(uint32_t))) {
            return 0;
        }

        // skip
        if (!in_data.seekg(chunk_size, std::ios::cur)) {
            return 0;
        }

        if (!in_data.read(chunk_id, 4)) {
            return 0;
        }
    }

    if (chunk_id[0] != 'd' || chunk_id[1] != 'a' || chunk_id[2] != 't' ||
        chunk_id[3] != 'a') {
        return 0;
    }

    uint32_t chunk_size;
    if (!in_data.read((char *)&chunk_size, sizeof(uint32_t))) {
        return 0;
    }

    samples.reset(new uint8_t[chunk_size]);
    if (!in_data.read((char *)&samples[0], chunk_size)) {
        return 0;
    }

    channels = (int)chunk.channels_count;
    samples_per_second = (int)chunk.samples_per_second;
    bits_per_sample = (int)chunk.bits_per_sample;

    return (int)chunk_size;
}