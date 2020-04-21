#include "Time_.h"

#include <chrono>

namespace Sys {
std::chrono::steady_clock::time_point init_time = std::chrono::steady_clock::now();
}

uint32_t Sys::GetTimeMs() {
    auto t = (std::chrono::steady_clock::now() - init_time);
    auto tt = std::chrono::duration_cast<std::chrono::milliseconds>(t);
    return (uint32_t)tt.count();
}

uint64_t Sys::GetTimeUs() {
    auto t = (std::chrono::steady_clock::now() - init_time);
    auto tt = std::chrono::duration_cast<std::chrono::microseconds>(t);
    return (uint64_t)tt.count();
}

uint64_t Sys::GetTimeNs() {
    auto t = (std::chrono::steady_clock::now() - init_time);
    auto tt = std::chrono::duration_cast<std::chrono::nanoseconds>(t);
    return (uint64_t)tt.count();
}