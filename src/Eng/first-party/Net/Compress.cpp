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
    };

    // lzo work memory is scratch for one call at a time: one per thread, so
    // compressing concurrently from several threads is safe
    thread_local LZOContext lzo_ctx;
}

Net::Packet Net::CompressLZO(const Packet &pack) {
    if (pack.empty()) {
        return {};
    }
    Packet res(Net::CalcLZOOutSize(int(pack.size())));
    lzo_uint compressed_size = res.size();
    if (lzo1x_1_compress(&pack[0], pack.size(), &res[0], &compressed_size, lzo_ctx.working_mem) != LZO_E_OK) {
        return {};
    }

    res.resize(compressed_size);

    return res;
}

Net::Packet Net::DecompressLZO(const Packet &pack) {
    if (pack.empty()) {
        return {};
    }
    // the stream stores no original size: grow the output while lzo reports
    // the buffer too small (this minilzo returns LZO_E_OUTPUT_OVERRUN for it),
    // and give up on anything that does not fit a sane bound
    constexpr size_t max_out = 256u << 20;
    Packet res(4096);
    for (;;) {
        lzo_uint decompressed_size = res.size();
        const int res_code = lzo1x_decompress_safe(&pack[0], pack.size(), &res[0], &decompressed_size, nullptr);
        if (res_code == LZO_E_OK) {
            res.resize(decompressed_size);
            return res;
        }
        if (res_code != LZO_E_OUTPUT_OVERRUN || res.size() >= max_out) {
            return {};
        }
        res.resize(res.size() * 2);
    }
}

int Net::CalcLZOOutSize(const int in_size) {
    return (in_size + in_size / 16 + 64 + 3);
}

int Net::CompressLZO(const uint8_t *in_buf, int in_size, uint8_t *out_buf) {
    lzo_uint out_len = (lzo_uint) CalcLZOOutSize(in_size);
    if (in_size < 0 ||
        lzo1x_1_compress(in_buf, (lzo_uint) in_size, out_buf, &out_len, lzo_ctx.working_mem) != LZO_E_OK) {
        return -1;
    }
    return int(out_len);
}

int Net::DecompressLZO(const uint8_t *in_buf, int in_size, uint8_t *out_buf, int out_size) {
    lzo_uint out_len = (lzo_uint) out_size;
    if (in_size < 0 || out_size < 0 ||
        lzo1x_decompress_safe(in_buf, (lzo_uint) in_size, out_buf, &out_len, nullptr) != LZO_E_OK) {
        return -1;
    }
    return int(out_len);
}
