#include "test_common.h"

#include <cstdio>
#include <cstring>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "../Compress.h"
#include "../hash/Crc32.h"

void test_compress() {
    using namespace Net;

    printf("Test compress           | ");

    Packet test_buf(1024);
    for (int i = 0; i < 256; i++) {
        test_buf[i] = (uint8_t)(i % 255);
    }

    for (int i = 256; i < 768; i++) {
        test_buf[i] = 0;
    }

    for (int i = 768; i < int(test_buf.size()); i++) {
        test_buf[i] = uint8_t(i % 255);
    }

    Packet compr = CompressLZO(test_buf);

    // printf("size before %i, size after %i\n", int(test_buf.size()), int(compr.size()));

    Packet decompr = DecompressLZO(compr);

    require(decompr.size() == test_buf.size());
    require(test_buf == decompr);

    { // 1 MiB through both APIs: the Packet overloads used to hard-code a 4 KiB
      // output buffer and ignored every lzo return code
        const int n = 1 << 20;
        std::vector<uint8_t> buf((size_t) n);
        for (int i = 0; i < n; ++i) {
            buf[(size_t) i] = (uint8_t) ((i / 97) % 7);
        }
        const Packet big(buf.begin(), buf.end());

        const Packet comp = CompressLZO(big);
        require(!comp.empty());
        require(comp.size() < big.size());
        const Packet back = DecompressLZO(comp);
        require(back == big);

        std::vector<uint8_t> comp_raw((size_t) CalcLZOOutSize(n));
        const int comp_size = CompressLZO(buf.data(), n, comp_raw.data());
        require(comp_size > 0);
        std::vector<uint8_t> back_raw((size_t) n);
        require(DecompressLZO(comp_raw.data(), comp_size, back_raw.data(), n) == n);
        require(back_raw == buf);

        // corrupt input must fail cleanly instead of returning a garbage-sized packet
        Packet broken = comp;
        for (size_t i = 0; i < broken.size(); i += 7) {
            broken[i] = (unsigned char) ~broken[i];
        }
        require(DecompressLZO(broken).empty());
    }
    { // the LZO encoder is deterministic: pinning the compressed size and a CRC of
      // the stream catches any change to the byte format the asset database relies on
        const int n = 4096;
        std::vector<uint8_t> in((size_t) n);
        for (int i = 0; i < n; ++i) {
            in[(size_t) i] = (uint8_t) ((i % 16 == 0) ? 0 : ('A' + (i % 23)));
        }
        const Packet src(in.begin(), in.end());

        const Packet comp = CompressLZO(src);
        require(comp.size() == 210);
        require(crc32_fast(comp.data(), comp.size()) == 0xB3DE4866u);

        std::vector<uint8_t> raw((size_t) CalcLZOOutSize(n));
        require(CompressLZO(in.data(), n, raw.data()) == 210);
        require(std::equal(comp.begin(), comp.begin() + 210, raw.begin())); // both APIs, same bytes
        require(DecompressLZO(comp) == src); // the pinned stream still decodes
    }
    { // compressing from 4 threads at once must not race on one shared workspace
        const int n = 1 << 18;
        std::vector<uint8_t> buf((size_t) n);
        for (int i = 0; i < n; ++i) {
            buf[(size_t) i] = (uint8_t) (i % 13);
        }
        std::atomic<int> ok{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&]() {
                std::vector<uint8_t> comp((size_t) CalcLZOOutSize(n));
                for (int iter = 0; iter < 10; ++iter) {
                    if (CompressLZO(buf.data(), n, comp.data()) > 0) {
                        ++ok;
                    }
                }
            });
        }
        for (auto &th : threads) {
            th.join();
        }
        require(ok == 40);
    }

    printf("OK\n");
}
