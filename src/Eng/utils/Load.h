#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace Eng {
std::vector<uint8_t> LoadHDR(std::string_view name, int &w, int &h);
}