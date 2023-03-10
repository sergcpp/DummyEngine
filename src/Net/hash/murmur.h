#pragma once

#include <cstddef>
#include <cstdint>

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed);