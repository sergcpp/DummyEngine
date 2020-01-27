#pragma once

#include "VarContainer.h"

namespace Net {
    Packet CompressLZO(const Packet &pack);
    Packet DecompressLZO(const Packet &pack);

    int CalcLZOOutSize(int in_size);

    int CompressLZO(const uint8_t *in_buf, int in_size, uint8_t *out_buf);
    int DecompressLZO(const uint8_t *in_buf, int in_size, uint8_t *out_buf, int out_size);
}
