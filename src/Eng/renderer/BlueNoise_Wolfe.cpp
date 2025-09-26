#include "BlueNoise_Wolfe.h"

#include <cassert>
#include <cfloat>

#include <array>
#include <bitset>
#include <fstream>
#include <memory>
#include <random>

#include <Ren/Utils.h>

namespace Eng::BNInternal {
namespace STBN {
static const int TileRes = 64;
static const int InitialPointsPercentage = 10;

static const float GaussOmega = 7.22f;

std::array<int, 3> xyz_from_index(const int index) {
    return std::array{index % TileRes, (index / TileRes) % TileRes, (index / TileRes) / TileRes};
}

template <int SampleCount>
Ren::Vec2i splat_pixel_energy(const int ox, const int oy, const int oz,
                              const std::bitset<TileRes> bitmap[SampleCount][TileRes], const float val,
                              float energy[SampleCount][TileRes][TileRes]) {
    float min_value = FLT_MAX, max_value = -1.0f;
    int imin = -1, imax = -1;
    for (int z = 0; z < SampleCount; ++z) {
        int dz = std::abs(z - oz);
        dz = std::min(dz, SampleCount - dz);
        for (int y = 0; y < TileRes; ++y) {
            int dy = std::abs(y - oy);
            dy = std::min(dy, TileRes - dy);
            for (int x = 0; x < TileRes; ++x) {
                int dx = std::abs(x - ox);
                dx = std::min(dx, TileRes - dx);

                float proximity = 0.0f;
                if (z == oz) {
                    proximity = std::exp(-(dx * dx + dy * dy) / GaussOmega);
                } else if (x == ox && y == oy) {
                    proximity = std::exp(-(dz * dz) / GaussOmega);
                }
                energy[z][y][x] += proximity * val;

                if (bitmap[z][y][x] && energy[z][y][x] > max_value) {
                    max_value = energy[z][y][x];
                    imax = z * TileRes * TileRes + y * TileRes + x;
                } else if (!bitmap[z][y][x] && energy[z][y][x] < min_value) {
                    min_value = energy[z][y][x];
                    imin = z * TileRes * TileRes + y * TileRes + x;
                }
            }
        }
    }
    return Ren::Vec2i{imin, imax};
}
} // namespace STBN

void WriteDDS(const float *data, int w, int h, int d, const char *name);
} // namespace Eng::BNInternal

template <int Log2SampleCount> void Eng::Generate1D_STBN() {
    using namespace BNInternal;
    using namespace STBN;

    static const int SampleCount = (1 << Log2SampleCount);

    // Dynamic allocation is used to avoid stack overflow
    struct bn_data_t {
        std::bitset<TileRes> bitmap[SampleCount][TileRes];
        float energy[SampleCount][TileRes][TileRes] = {};
        int points_count = 0;
        Ren::Vec2i iminmax;

        float noise[SampleCount][TileRes][TileRes] = {};

        // temp data
        float debug_values[SampleCount][TileRes][TileRes] = {};
    };
    auto data = std::make_unique<bn_data_t>();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> uniform_index(0, SampleCount * TileRes * TileRes - 1);

    // Void and cluster algorithm extended to include time dimension
    // See: https://blog.demofox.org/2019/06/25/generating-blue-noise-textures-with-void-and-cluster/

    // Set initial points
    while (data->points_count < (SampleCount * TileRes * TileRes * InitialPointsPercentage / 100)) {
        const int index = uniform_index(gen);
        const auto [ox, oy, oz] = xyz_from_index(index);
        assert(index == oz * (TileRes * TileRes) + oy * TileRes + ox);

        if (!data->bitmap[oz][oy][ox]) {
            data->bitmap[oz][oy][ox] = true;
            data->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, data->bitmap, 1.0f, data->energy);
            ++data->points_count;
        }
    }

    char name_buf[128];

    { // Debug energy values
        float min_value = FLT_MAX, max_value = 0.0f;
        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            data->debug_values[z][y][x] = data->energy[z][y][x];
            min_value = std::min(min_value, data->debug_values[z][y][x]);
            max_value = std::max(max_value, data->debug_values[z][y][x]);
        }
        // normalize values (for easier debugging)
        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            e = (e - min_value) / (max_value - min_value);
        }

        snprintf(name_buf, sizeof(name_buf), "debug_energy_%i.dds", SampleCount);
        WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);
    }

    // Redistribute initial points
    int last_point = -1;
    while (data->iminmax[1] != last_point) {
        { // Remove max
            const auto [ox, oy, oz] = xyz_from_index(data->iminmax[1]);

            data->bitmap[oz][oy][ox] = false;
            data->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, data->bitmap, -1.0f, data->energy);
            last_point = data->iminmax[0];
        }
        { // Add min
            const auto [ox, oy, oz] = xyz_from_index(data->iminmax[0]);

            data->bitmap[oz][oy][ox] = true;
            data->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, data->bitmap, 1.0f, data->energy);
        }
    }

    { // Debug energy values
        float min_value = FLT_MAX, max_value = 0.0f;
        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            e = data->energy[z][y][x];
            min_value = std::min(min_value, e);
            max_value = std::max(max_value, e);
        }
        // normalize values (for easier debugging)
        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            e = (e - min_value) / (max_value - min_value);
        }

        snprintf(name_buf, sizeof(name_buf), "debug_energy_redist_%i.dds", SampleCount);
        WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);
    }

    for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
        const auto [x, y, z] = xyz_from_index(j);
        data->noise[z][y][x] = 1.0f;
    }

    // Phase I - Initial Pattern Ordering
    auto temp = std::make_unique<bn_data_t>(*data);
    while (temp->points_count) {
        const auto [ox, oy, oz] = xyz_from_index(temp->iminmax[1]);

        data->noise[oz][oy][ox] = float(--temp->points_count) / (SampleCount * TileRes * TileRes);
        temp->bitmap[oz][oy][ox] = false;
        temp->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, temp->bitmap, -1.0f, temp->energy);
    }

    // Phase II - Order First Half of Pixels
    while (data->points_count < (SampleCount * TileRes * TileRes / 2)) {
        const auto [ox, oy, oz] = xyz_from_index(data->iminmax[0]);

        data->noise[oz][oy][ox] = float(data->points_count++) / (SampleCount * TileRes * TileRes);
        data->bitmap[oz][oy][ox] = true;
        data->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, data->bitmap, 1.0f, data->energy);
    }

    // Invert energy map
    for (int z = 0; z < SampleCount; ++z) {
        for (int y = 0; y < TileRes; ++y) {
            data->bitmap[z][y].flip();
        }
    }
    for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
        const auto [x, y, z] = xyz_from_index(j);
        data->energy[z][y][x] = 0.0f;
    }
    for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
        const auto [ox, oy, oz] = xyz_from_index(j);
        if (data->bitmap[oz][oy][ox]) {
            data->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, data->bitmap, 1.0f, data->energy);
        }
    }

    // Phase III - Order Second Half of Pixels
    while (data->points_count < SampleCount * TileRes * TileRes) {
        const auto [ox, oy, oz] = xyz_from_index(temp->iminmax[1]);

        data->noise[oz][oy][ox] = float(data->points_count++) / (SampleCount * TileRes * TileRes);
        data->bitmap[oz][oy][ox] = false;
        data->iminmax = splat_pixel_energy<SampleCount>(ox, oy, oz, data->bitmap, -1.0f, data->energy);
    }

    { // Debug noise values
        float min_value = FLT_MAX, max_value = 0.0f;
        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            e = data->noise[z][y][x];
            min_value = std::min(min_value, e);
            max_value = std::max(max_value, e);
        }
        // normalize values (for easier debugging)
        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            e = (e - min_value) / (max_value - min_value);
        }

        snprintf(name_buf, sizeof(name_buf), "debug_noise_%i.dds", SampleCount);
        WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);
    }

    { // dump C array
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/__stbn_sampler_1D_%ispp.inl", SampleCount);
        std::ofstream out_file(name_buf, std::ios::binary);
        out_file << "const int w = " << TileRes << ";\n";
        out_file << "const int h = " << TileRes << ";\n";
        out_file << "const int d = " << SampleCount << ";\n";
        out_file << "const uint8_t stbn_samples[" << SampleCount * TileRes * TileRes << "] = {\n    ";
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    out_file << std::clamp(int(data->noise[z][y][x] * 255.0f), 0, 255);
                    if (x != TileRes - 1 || y != TileRes - 1 || z != SampleCount - 1) {
                        out_file << "u, ";
                    } else {
                        out_file << "u\n";
                    }
                }
            }
        }
        out_file << "};\n";
    }
}

template void Eng::Generate1D_STBN<0>();
template void Eng::Generate1D_STBN<1>();
template void Eng::Generate1D_STBN<2>();
template void Eng::Generate1D_STBN<3>();
template void Eng::Generate1D_STBN<4>();
template void Eng::Generate1D_STBN<5>();
template void Eng::Generate1D_STBN<6>();
template void Eng::Generate1D_STBN<7>();
template void Eng::Generate1D_STBN<8>();

void Eng::BNInternal::WriteDDS(const float *data, const int w, const int h, const int d, const char *name) {
    Ren::DDSHeader header = {};
    header.dwMagic = (unsigned('D') << 0u) | (unsigned('D') << 8u) | (unsigned('S') << 16u) | (unsigned(' ') << 24u);
    header.dwSize = 124;
    header.dwFlags = Ren::DDSD_CAPS | Ren::DDSD_HEIGHT | Ren::DDSD_WIDTH | Ren::DDSD_DEPTH | Ren::DDSD_PIXELFORMAT;
    header.dwWidth = w;
    header.dwHeight = h;
    header.dwDepth = d;
    header.sPixelFormat.dwSize = 32;
    header.sPixelFormat.dwFlags = Ren::DDPF_FOURCC;
    header.sPixelFormat.dwFourCC = Ren::FourCC_DX10;

    header.sCaps.dwCaps1 = Ren::DDSCAPS_TEXTURE | Ren::DDSCAPS_MIPMAP;

    Ren::DDS_HEADER_DXT10 dx10Header = {};
    dx10Header.dxgiFormat = Ren::DXGI_FORMAT_R8_UNORM;
    dx10Header.resourceDimension = Ren::D3D10_RESOURCE_DIMENSION_TEXTURE3D;
    dx10Header.miscFlag = 0;
    dx10Header.arraySize = 1;
    dx10Header.miscFlags2 = 0;

    std::ofstream out_file(name, std::ios::binary);
    out_file.write((char *)&header, sizeof(Ren::DDSHeader));
    out_file.write((char *)&dx10Header, sizeof(Ren::DDS_HEADER_DXT10));

    std::vector<uint8_t> u8data(w * h * d);
    for (int i = 0; i < w * h * d; ++i) {
        u8data[i] = uint8_t(std::clamp(int(data[i] * 255.0f), 0, 255));
    }

    out_file.write((char *)u8data.data(), w * h * d);
}