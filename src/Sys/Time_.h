#pragma once

#include <cstdint>

namespace Sys {
uint32_t GetTimeMs();
uint64_t GetTimeUs();
uint64_t GetTimeNs();
double GetTimeS();
}
