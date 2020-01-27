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
    //lzo1x_decompress(&pack[0], pack.size(), &res[0], &decompressed_size, NULL);
    lzo1x_decompress_safe(&pack[0], pack.size(), &res[0], &decompressed_size, NULL);

    res.resize(decompressed_size);

    return res;
}
