/*
    sha1.h - header of
 
    ============
    SHA-1 in C++
    ============
 
    100% Public Domain.
 
    Original C Code
        -- Steve Reid <steve@edmweb.com>
    Small changes to fit into bglibs
        -- Bruce Guenter <bruce@untroubled.org>
    Translation to simpler C++ Code
        -- Volker Grabsch <vog@notjusthosting.com>
*/
 
#pragma once
 
#include <cstdint>

#include <iostream>
#include <string>
 
class SHA1
{
public:
    SHA1();
    void update(const std::string &s);
    void update(std::istream &is);
    std::string final_str();
    void final_digest(unsigned int digest[5]);
    static std::string from_file(const std::string &filename);

private:
    static const unsigned int DIGEST_INTS = 5;  // number of 32bit integers per SHA1 digest
    static const unsigned int BLOCK_INTS = 16;  // number of 32bit integers per SHA1 block
    static const unsigned int BLOCK_BYTES = BLOCK_INTS * 4;
 
    uint32_t digest[DIGEST_INTS];
    std::string buffer;
    uint64_t transforms;
 
    void reset();
    void transform(uint32_t block[BLOCK_BYTES]);
    void finish();
 
    static void buffer_to_block(const std::string &buffer, uint32_t block[BLOCK_BYTES]);
    static void read(std::istream &is, std::string &s, int max);
};
 
std::string sha1(const std::string &string);
void sha1(const std::string &str, unsigned int digest[5]);
