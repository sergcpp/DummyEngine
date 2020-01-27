#include "Compress.h"

#include "minilzo/minilzo.h"

namespace {
    struct LZOContext {
        unsigned char *working_mem;
        LZOContext() {
            working_mem = new unsigned char[LZO1X_1_MEM_COMPRESS];
        }
        ~LZOContext() {
            delete[] working_mem;
        }
    } lzo_ctx;
}

Net::Packet Net::CompressLZO(const Packet &pack) {
    Packet res(4096);
    lzo_uint compressed_size;
    lzo1x_1_compress(&pack[0], pack.size(), &res[0], &compressed_size, lzo_ctx.working_mem);

    res.resize(compressed_size);

    return res;
}

Net::Packet Net::DecompressLZO(const Packet &pack) {
    Packet res(4096);
    lzo_uint decompressed_size = res.size();
    lzo1x_decompress_safe(&pack[0], pack.size(), &res[0], &decompressed_size, nullptr);

    res.resize(decompressed_size);

    return res;
}

int Net::CalcLZOOutSize(const int in_size) {
    return (in_size + in_size / 16 + 64 + 3);
}

int Net::CompressLZO(const uint8_t *in_buf, int in_size, uint8_t *out_buf) {
    lzo_uint compressed_size;
    lzo1x_1_compress(in_buf, (lzo_uint)in_size, out_buf, &compressed_size, lzo_ctx.working_mem);
    return (int)compressed_size;
}

int Net::DecompressLZO(const uint8_t *in_buf, int in_size, uint8_t *out_buf, int out_size) {
    lzo_uint decompressed_size = out_size;
    lzo1x_decompress_safe(in_buf, in_size, out_buf, &decompressed_size, nullptr);
    return (int)decompressed_size;
}