#pragma once

#include <cstdint>

namespace Ren {
const int MaxFramesInFlight = 3;
const int MaxTimestampQueries = 512;

template <typename T> inline T RoundDown(const T size, const T alignment) { return alignment * (size / alignment); }

template <typename T> inline T RoundUp(const T size, const T alignment) {
    return alignment * ((size + alignment - 1) / alignment);
}

template <typename T> inline T DivCeil(const T size, const T alignment) {
    return ((size + alignment - 1) / alignment);
}
} // namespace Ren