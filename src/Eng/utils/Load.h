#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <Ren/TextureParams.h>

namespace Eng {
std::vector<uint8_t> LoadDDS(std::string_view path, Ren::TexParams *p);
std::vector<uint8_t> LoadHDR(std::string_view path, int &w, int &h);
}