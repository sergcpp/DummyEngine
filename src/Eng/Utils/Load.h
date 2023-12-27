#pragma once

#include <string>
#include <vector>

namespace Eng {
std::vector<uint8_t> LoadHDR(const char *name, int &w, int &h);
}