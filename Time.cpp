#include "Time_.h"

#include <chrono>

namespace Sys {
std::chrono::steady_clock::time_point init_time = std::chrono::steady_clock::now();

unsigned int cached_time = 0;
}

unsigned int Sys::GetTicks() {
    auto t = (std::chrono::steady_clock::now() - init_time);
    auto tt = std::chrono::duration_cast<std::chrono::milliseconds>(t);
    return (unsigned int)tt.count();
}

uint64_t Sys::GetTimeNs() {
    auto t = (std::chrono::steady_clock::now() - init_time);
    auto tt = std::chrono::duration_cast<std::chrono::nanoseconds>(t);
    return (unsigned int)tt.count();
}