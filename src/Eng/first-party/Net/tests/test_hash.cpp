#include "test_common.h"

#include <string>

#include "../hash/Crc32.h"
#include "../hash/base64.h"
#include "../hash/md5.h"
#include "../hash/murmur.h"
#include "../hash/sha1.h"

void test_hash() {
    printf("Test hash               | ");

    { // crc32_fast must agree with every reference variant (Eng mixes them)
        std::string big;
        for (int i = 0; i < 4000; ++i) {
            big += char('A' + i % 26);
        }
        const std::string inputs[] = {"", "a", "abc", "123456789", big};
        for (const auto &s : inputs) {
            const uint32_t fast = crc32_fast(s.data(), s.size());
            require(fast == crc32_bitwise(s.data(), s.size()));
            require(fast == crc32_halfbyte(s.data(), s.size()));
            require(fast == crc32_1byte(s.data(), s.size()));
            require(fast == crc32_4bytes(s.data(), s.size()));
            require(fast == crc32_8bytes(s.data(), s.size()));
            require(fast == crc32_16bytes(s.data(), s.size()));
        }
    }
    { // published CRC-32 (IEEE 802.3) check values
        require(crc32_fast("", 0) == 0x00000000u);
        require(crc32_fast("123456789", 9) == 0xCBF43926u);
    }
    { // published MD5 vectors (RFC 1321)
        require(md5("") == "d41d8cd98f00b204e9800998ecf8427e");
        require(md5("a") == "0cc175b9c0f1b6a831c399e269772661");
        require(md5("abc") == "900150983cd24fb0d6963f7d28e17f72");
        require(md5("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
        require(md5("The quick brown fox jumps over the lazy dog") == "9e107d9d372bb6826bd81d3542a419d6");
    }
    { // published SHA-1 vectors
        SHA1 empty_hash;
        require(empty_hash.final_str() == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        SHA1 abc_hash;
        abc_hash.update("abc");
        require(abc_hash.final_str() == "a9993e364706816aba3e25717850c26c9cd0d89d");
        SHA1 fox_hash;
        fox_hash.update("The quick brown fox jumps over the lazy dog");
        require(fox_hash.final_str() == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
    }
    { // murmur3-32 golden values (canonical Austin Appleby reference algorithm)
        require(murmur3_32(reinterpret_cast<const uint8_t *>(""), 0, 0) == 0x00000000u);
        require(murmur3_32(reinterpret_cast<const uint8_t *>("a"), 1, 0) == 0x3C2569B2u);
        require(murmur3_32(reinterpret_cast<const uint8_t *>("abc"), 3, 0) == 0xB3DD93FAu);
        require(murmur3_32(reinterpret_cast<const uint8_t *>("abcd"), 4, 0) == 0x43ED676Au);
        const std::string fox = "The quick brown fox jumps over the lazy dog";
        require(murmur3_32(reinterpret_cast<const uint8_t *>(fox.data()), fox.size(), 0) == 0x2E4FF723u);
        const std::string name = "ArchRender";
        require(murmur3_32(reinterpret_cast<const uint8_t *>(name.data()), name.size(), 0x9747B28Cu) == 0x741A3493u);
        static const uint8_t binary[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        require(murmur3_32(binary, sizeof(binary), 0x12345678u) == 0x7A56FD86u);
    }
    { // base64: published vectors + round-trip over every padding length
        require(base64_encode(reinterpret_cast<const unsigned char *>(""), 0) == "");
        require(base64_encode(reinterpret_cast<const unsigned char *>("f"), 1) == "Zg==");
        require(base64_encode(reinterpret_cast<const unsigned char *>("fo"), 2) == "Zm8=");
        require(base64_encode(reinterpret_cast<const unsigned char *>("foo"), 3) == "Zm9v");
        require(base64_encode(reinterpret_cast<const unsigned char *>("foob"), 4) == "Zm9vYg==");
        require(base64_encode(reinterpret_cast<const unsigned char *>("fooba"), 5) == "Zm9vYmE=");
        require(base64_encode(reinterpret_cast<const unsigned char *>("foobar"), 6) == "Zm9vYmFy");
        require(base64_decode("Zm9vYmFy") == "foobar");

        std::string all;
        for (int i = 0; i < 256; ++i) {
            all += char(i);
        }
        for (size_t len = 0; len <= 64; ++len) { // every 1/2/0-padding case
            const std::string enc = base64_encode(reinterpret_cast<const unsigned char *>(all.data()), (unsigned int) len);
            require(base64_decode(enc) == all.substr(0, len));
        }
    }

    printf("OK\n");
}
