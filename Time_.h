#pragma once

#include <cstdint>

namespace Sys {
unsigned int GetTicks();
uint64_t GetTimeUs();
uint64_t GetTimeNs();
extern unsigned int cached_time;
}
