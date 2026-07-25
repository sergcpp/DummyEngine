#include "BlueNoise_Ours.h"

#include <cassert>
#include <cfloat>

#include <bitset>
#include <complex>
#include <fstream>
#include <numeric>
#include <random>

#include <Ren/utils/Utils.h>

namespace Eng::BNInternal {
namespace TCBN {
static const int TileRes = 64;

static const float GaussOmegaI = 7.22f; // 3.6f;

// false value preserve histogram in each Z plane (keeps stratification)
static const bool AllowZSwaps = false;

static const int XYRadius = 8;
static const int BaseSwappingIterations = 100000;

static const float EMA_Alpha = 0.08f;
static const float HistoryRejection = 0.05f;

static const int TestFunctionsCount = 256;

template <int SampleCount, bool Sign>
Ren::Vec2i splat_pixel_energy(const int ox, const int oy, const int oz,
                              const std::bitset<TileRes> bitmap[SampleCount][TileRes], const bool strided_access,
                              const float z_filter[SampleCount], float energy[SampleCount][TileRes][TileRes]) {
    float min_value = FLT_MAX, max_value = -1.0f;
    int imin = -1, imax = -1;
    for (int z = 0; z < SampleCount; ++z) {
        const float proximity_z = z_filter[(SampleCount + z - oz) % SampleCount];

        for (int y = 0; y < TileRes; ++y) {
            int dy = std::abs(y - oy);
            dy = std::min(dy, TileRes - dy);
            for (int x = 0; x < TileRes; ++x) {
                int dx = std::abs(x - ox);
                dx = std::min(dx, TileRes - dx);

                float proximity = 0.0f;
                if (z == oz) {
                    proximity = std::exp(-(dx * dx + dy * dy) / GaussOmegaI);
                    if (strided_access) {
                        if ((dx % 2) == 0 && (dy % 2) == 0) {
                            proximity += std::exp(-((dx / 2) * (dx / 2) + (dy / 2) * (dy / 2)) / GaussOmegaI);
                        }
                        if ((dx % 3) == 0 && (dy % 3) == 0) {
                            proximity += std::exp(-((dx / 3) * (dx / 3) + (dy / 3) * (dy / 3)) / GaussOmegaI);
                        }
                        if ((dx % 4) == 0 && (dy % 4) == 0) {
                            proximity += std::exp(-((dx / 4) * (dx / 4) + (dy / 4) * (dy / 4)) / GaussOmegaI);
                        }
                    }
                } else if (x == ox && y == oy) {
                    proximity = proximity_z;
                    if (strided_access) {
                        proximity *= 4.0f;
                    }
                }

                if constexpr (Sign) {
                    energy[z][y][x] += proximity;
                } else {
                    energy[z][y][x] -= proximity;
                }

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

std::array<int, 3> xyz_from_index(const int index) {
    return std::array{index % TileRes, (index / TileRes) % TileRes, (index / TileRes) / TileRes};
}

float truncated_ema(const float alpha, const int i, const int j, const int m) {
    if (i < j + m - 1) {
        return alpha * powf(1.0f - alpha, float(i - j));
    } else if (i == j + m - 1) {
        return powf(1.0f - alpha, float(m - 1));
    }
    return 0.0f;
}

float test_function_1D(float x, float min, float max) {
    if (x > min && x < max) {
        return 1.0f;
    }
    return 0.0f;
}
float test_function_1D_integral(float min, float max) { return max - min; }

template <int SampleCount>
float calc_pixel_proximity(const int ox, const int oy, const int oz,
                           const std::vector<float> errors[SampleCount][TileRes][TileRes],
                           const float xy_filter[XYRadius + 1][XYRadius + 1], const float z_filter[SampleCount]) {
    const std::vector<float> &oerr = errors[oz][oy][ox];

    double total_error = 0.0;
    for (int z = 0; z < SampleCount; ++z) {
        const float proximity_z = z_filter[(SampleCount + z - oz) % SampleCount];

        for (int y = oy - XYRadius; y <= oy + XYRadius; ++y) {
            const int wrapped_y = (y + TileRes) % TileRes;
            const int dy = std::abs(y - oy);
            for (int x = ox - XYRadius; x <= ox + XYRadius; ++x) {
                const int wrapped_x = (x + TileRes) % TileRes;
                const int dx = std::abs(x - ox);
                if (wrapped_x == ox && wrapped_y == oy && z == oz) {
                    continue;
                }
                float proximity = 0.0f;
                if ((wrapped_x == ox && wrapped_y == oy) || z == oz) {
                    proximity = xy_filter[dy][dx] * proximity_z;
                }

                float error = 0.0f;
                for (int i = 0; i < TestFunctionsCount && proximity > 0.0f; ++i) {
                    const float diff = oerr[i] - errors[z][wrapped_y][wrapped_x][i];
                    error += diff * diff;
                }
                error *= (proximity / TestFunctionsCount);

                total_error += error;
            }
        }
    }
    return float(total_error);
}

template <int SampleCount, bool Sign>
void splat_pixel_proximity(const int ox, const int oy, const int oz,
                           const std::vector<float> errors[SampleCount][TileRes][TileRes],
                           float energy[SampleCount][TileRes][TileRes],
                           const float xy_filter[XYRadius + 1][XYRadius + 1], const float z_filter[SampleCount]) {
    const std::vector<float> &oerr = errors[oz][oy][ox];
    for (int z = 0; z < SampleCount; ++z) {
        const float proximity_z = z_filter[(SampleCount - z + oz) % SampleCount];

        // const int dz = std::abs(z - oz);
        for (int y = oy - XYRadius; y <= oy + XYRadius; ++y) {
            const int wrapped_y = (y + TileRes) % TileRes;
            const int dy = std::abs(y - oy);
            for (int x = ox - XYRadius; x <= ox + XYRadius; ++x) {
                const int wrapped_x = (x + TileRes) % TileRes;
                const int dx = std::abs(x - ox);
                if (wrapped_x == ox && wrapped_y == oy && z == oz) {
                    continue;
                }

                float proximity = 0.0f;
                if ((wrapped_x == ox && wrapped_y == oy) || z == oz) {
                    proximity = xy_filter[dy][dx] * proximity_z;
                }

                float error = 0.0f;
                for (int i = 0; i < TestFunctionsCount && proximity > 0.0f; ++i) {
                    const float diff = oerr[i] - errors[z][wrapped_y][wrapped_x][i];
                    error += diff * diff;
                }
                error *= (proximity / TestFunctionsCount);

                if constexpr (Sign) {
                    energy[z][wrapped_y][wrapped_x] += error;
                } else {
                    energy[z][wrapped_y][wrapped_x] -= error;
                }
            }
        }
    }
}

template <int SampleCount>
float calc_pixel_proximity(const int ox, const int oy, const int oz,
                           const std::vector<Ren::Vec2f> values[TileRes][TileRes]) {
    const Ren::Vec2f oval = values[oy][ox][oz];

    double total_proximity = 0.0;
    for (int z = oz - XYRadius; z <= oz + XYRadius; ++z) {
        const int wrapped_z = (z + SampleCount) % SampleCount;
        const int dz = std::abs(z - oz);
        for (int y = oy - XYRadius; y <= oy + XYRadius; ++y) {
            const int wrapped_y = (y + TileRes) % TileRes;
            const int dy = std::abs(y - oy);
            for (int x = ox - XYRadius; x <= ox + XYRadius; ++x) {
                const int wrapped_x = (x + TileRes) % TileRes;
                const int dx = std::abs(x - ox);
                if (wrapped_x == ox && wrapped_y == oy && wrapped_z == oz) {
                    continue;
                }
                float proximity = 0.0f;
                if ((wrapped_x == ox && wrapped_y == oy) || wrapped_z == oz) {
                    proximity += (dx * dx + dy * dy + dz * dz) / GaussOmegaI;
                    // calc distance ^ (2.0 / 3.0)
                    proximity += cbrtf(Ren::Distance2(values[wrapped_y][wrapped_x][wrapped_z], oval));
                    proximity = std::exp(-proximity);
                }
                total_proximity += proximity;
            }
        }
    }
    return float(total_proximity);
}

template <int SampleCount, bool Sign>
void splat_pixel_proximity(const int ox, const int oy, const int oz,
                           const std::vector<Ren::Vec2f> values[TileRes][TileRes],
                           float energy[SampleCount][TileRes][TileRes]) {
    const Ren::Vec2f oval = values[oy][ox][oz];
    for (int z = oz - XYRadius; z <= oz + XYRadius; ++z) {
        const int wrapped_z = (z + SampleCount) % SampleCount;
        const int dz = std::abs(z - oz);
        for (int y = oy - XYRadius; y <= oy + XYRadius; ++y) {
            const int wrapped_y = (y + TileRes) % TileRes;
            const int dy = std::abs(y - oy);
            for (int x = ox - XYRadius; x <= ox + XYRadius; ++x) {
                const int wrapped_x = (x + TileRes) % TileRes;
                const int dx = std::abs(x - ox);
                if (wrapped_x == ox && wrapped_y == oy && wrapped_z == oz) {
                    continue;
                }

                float proximity = 0.0f;
                if ((wrapped_x == ox && wrapped_y == oy) || wrapped_z == oz) {
                    proximity += (dx * dx + dy * dy + dz * dz) / GaussOmegaI;
                    // calc distance ^ (2.0 / 3.0)
                    proximity += cbrtf(Ren::Distance2(values[wrapped_y][wrapped_x][wrapped_z], oval));
                    proximity = std::exp(-proximity);
                }
                if constexpr (Sign) {
                    energy[wrapped_z][wrapped_y][wrapped_x] += proximity;
                } else {
                    energy[wrapped_z][wrapped_y][wrapped_x] -= proximity;
                }
            }
        }
    }
}

void dft_2d(const float data[TileRes][TileRes], std::complex<float> output[TileRes][TileRes]) {
    for (int v = 0; v < TileRes; ++v) {
        for (int u = 0; u < TileRes; ++u) {

            std::complex<float> sum = {};

            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {

                    const float angle = -2.0f * Ren::Pi<float>() * (float(u) * x / TileRes + float(v) * y / TileRes);
                    const float shift = ((x + y) % 2 == 0) ? 1.0f : -1.0f;

                    sum += std::complex<float>{data[y][x] * shift, 0.0f} *
                           std::complex<float>{std::cos(angle), std::sin(angle)};
                }
            }

            output[v][u] = sum;
        }
    }
}

} // namespace TCBN

void WriteDDS(const float data[], int w, int h, int d, const char *name);
void WriteDDS(const Ren::Vec2f data[], int w, int h, int d, const char *name);
} // namespace Eng::BNInternal

template <int Log2SampleCount, Eng::eSpatialFilter sf, Eng::eTemporalFilter tf>
void Eng::Generate1D_TCBN_VC(const unsigned int seed, const bool strided_access) {
    using namespace BNInternal;
    using namespace TCBN;

    static const int SampleCount = (1 << Log2SampleCount);
    static const int InitialPointsPercentage = 10;

    // Dynamic allocation is used to avoid stack overflow
    struct bn_data_t {
        std::bitset<TileRes> bitmap[SampleCount][TileRes];
        float energy[SampleCount][TileRes][TileRes] = {};
        int points_count = 0;
        Ren::Vec2i iminmax;

        float noise[SampleCount][TileRes][TileRes] = {};

        // filter LUT
        float z_filter[SampleCount] = {};

        // temp data
        float debug_values[SampleCount][TileRes][TileRes] = {};
        float debug_values2[SampleCount][TileRes / 2][TileRes / 2] = {};
        float debug_values3[SampleCount][(TileRes + 2) / 3][(TileRes + 2) / 3] = {};
        float debug_values4[SampleCount][TileRes / 4][TileRes / 4] = {};
        float debug_values5[TileRes][SampleCount][TileRes] = {};
        float debug_values6[TileRes][TileRes] = {};
        std::complex<float> debug_dft[TileRes][TileRes] = {};
    };
    auto data = std::make_unique<bn_data_t>();

    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> uniform_index(0, SampleCount * TileRes * TileRes - 1);
    std::uniform_real_distribution<float> uniform_unorm_float(0.0f, 1.0f);

    // Init temporal filter
    if constexpr (tf == eTemporalFilter::Gauss) {
        for (int z = 0; z < SampleCount; ++z) {
            const int dz = std::min(z, SampleCount - z);
            data->z_filter[z] = std::exp(-(dz * dz) / GaussOmegaI);
        }
    } else if constexpr (tf == eTemporalFilter::TruncatedEMA) {
        // Stochastically sample the EMA filter.
        // This totally can be solved analytically, but I'm too lazy to do it.
        const int IterationsCount = 1000000;
        for (int iter = 0; iter < IterationsCount; ++iter) {
            // Generate history length based on the rejection probability
            int history_len = 0;
            while (history_len++ < SampleCount) {
                const float u = uniform_unorm_float(gen);
                if (u < HistoryRejection) {
                    break;
                }
            }
            for (int i = 0; i < SampleCount; ++i) {
                data->z_filter[i] += truncated_ema(EMA_Alpha, 0, std::max(-i, i - SampleCount), history_len);
            }
        }
        // Normalize
        for (int i = SampleCount - 1; i >= 0; --i) {
            data->z_filter[i] /= data->z_filter[0];
        }
    }

    // Void and cluster algorithm extended to include time dimension
    // See: https://blog.demofox.org/2019/06/25/generating-blue-noise-textures-with-void-and-cluster/

    // Set initial points
    while (data->points_count < (SampleCount * TileRes * TileRes * InitialPointsPercentage / 100)) {
        const int index = uniform_index(gen);
        const auto [ox, oy, oz] = xyz_from_index(index);
        assert(index == oz * (TileRes * TileRes) + oy * TileRes + ox);

        if (!data->bitmap[oz][oy][ox]) {
            data->bitmap[oz][oy][ox] = true;
            data->iminmax = splat_pixel_energy<SampleCount, true>(ox, oy, oz, data->bitmap, strided_access,
                                                                  data->z_filter, data->energy);
            ++data->points_count;
        }
    }

    char name_buf[128];

    auto debug_dft = [&]() {
        float min_val = FLT_MAX, max_val = 0.0f;
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values6[y][x] = data->noise[z][y][x];
                }
            }

            dft_2d(data->debug_values6, data->debug_dft);

            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values[z][y][x] = std::abs(data->debug_dft[y][x]);
                    min_val = std::min(min_val, data->debug_values[z][y][x]);
                    max_val = std::max(max_val, data->debug_values[z][y][x]);
                }
            }
        }

        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            // e = (e - min_val) / (max_val - min_val);
            e /= max_val;
        }

        snprintf(name_buf, sizeof(name_buf), "debug_dft_%i.dds", SampleCount);
        WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);
    };

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
            data->iminmax = splat_pixel_energy<SampleCount, false>(ox, oy, oz, data->bitmap, strided_access,
                                                                   data->z_filter, data->energy);
            last_point = data->iminmax[0];
        }
        { // Add min
            const auto [ox, oy, oz] = xyz_from_index(data->iminmax[0]);

            data->bitmap[oz][oy][ox] = true;
            data->iminmax = splat_pixel_energy<SampleCount, true>(ox, oy, oz, data->bitmap, strided_access,
                                                                  data->z_filter, data->energy);
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
        temp->iminmax = splat_pixel_energy<SampleCount, false>(ox, oy, oz, temp->bitmap, strided_access, data->z_filter,
                                                               temp->energy);
    }

    // Phase II - Order First Half of Pixels
    while (data->points_count < (SampleCount * TileRes * TileRes / 2)) {
        const auto [ox, oy, oz] = xyz_from_index(data->iminmax[0]);

        data->noise[oz][oy][ox] = float(data->points_count++) / (SampleCount * TileRes * TileRes);
        data->bitmap[oz][oy][ox] = true;
        data->iminmax = splat_pixel_energy<SampleCount, true>(ox, oy, oz, data->bitmap, strided_access, data->z_filter,
                                                              data->energy);
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
            data->iminmax = splat_pixel_energy<SampleCount, true>(ox, oy, oz, data->bitmap, strided_access,
                                                                  data->z_filter, data->energy);
        }
    }

    // Phase III - Order Second Half of Pixels
    while (data->points_count < SampleCount * TileRes * TileRes) {
        const auto [ox, oy, oz] = xyz_from_index(data->iminmax[1]);

        data->noise[oz][oy][ox] = float(data->points_count++) / (SampleCount * TileRes * TileRes);
        data->bitmap[oz][oy][ox] = false;
        data->iminmax = splat_pixel_energy<SampleCount, false>(ox, oy, oz, data->bitmap, strided_access, data->z_filter,
                                                               data->energy);
    }

    { // Debug noise values
        snprintf(name_buf, sizeof(name_buf), "debug_samples_%i.dds", SampleCount);
        WriteDDS(&data->noise[0][0][0], TileRes, TileRes, SampleCount, name_buf);

        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values5[y][z][x] = data->noise[z][y][x];
                }
            }
        }

        snprintf(name_buf, sizeof(name_buf), "debug_samples_XZY_%i.dds", SampleCount);
        WriteDDS(&data->debug_values5[0][0][0], TileRes, SampleCount, TileRes, name_buf);

        for (int i = 0; i < 4; ++i) {
            for (int z = 0; z < SampleCount; ++z) {
                for (int y = 0; y < TileRes; y += 2) {
                    for (int x = 0; x < TileRes; x += 2) {
                        data->debug_values2[z][y / 2][x / 2] = data->debug_values[z][y + (i / 2)][x + (i % 2)];
                    }
                }
            }
            snprintf(name_buf, sizeof(name_buf), "debug_samples_%i_strided2_%i.dds", SampleCount, i);
            WriteDDS(&data->debug_values2[0][0][0], TileRes / 2, TileRes / 2, SampleCount, name_buf);
        }

        for (int i = 0; i < 9; ++i) {
            for (int z = 0; z < SampleCount; ++z) {
                for (int y = 0; y < TileRes; y += 3) {
                    for (int x = 0; x < TileRes; x += 3) {
                        data->debug_values3[z][y / 3][x / 3] = data->debug_values[z][y + (i / 3)][x + (i % 3)];
                    }
                }
            }
            snprintf(name_buf, sizeof(name_buf), "debug_samples_%i_strided3_%i.dds", SampleCount, i);
            WriteDDS(&data->debug_values3[0][0][0], TileRes / 3, TileRes / 3, SampleCount, name_buf);
        }

        for (int i = 0; i < 16; ++i) {
            for (int z = 0; z < SampleCount; ++z) {
                for (int y = 0; y < TileRes; y += 4) {
                    for (int x = 0; x < TileRes; x += 4) {
                        data->debug_values4[z][y / 4][x / 4] = data->debug_values[z][y + (i / 4)][x + (i % 4)];
                    }
                }
            }
            snprintf(name_buf, sizeof(name_buf), "debug_samples_%i_strided4_%i.dds", SampleCount, i);
            WriteDDS(&data->debug_values4[0][0][0], TileRes / 4, TileRes / 4, SampleCount, name_buf);
        }
    }

    debug_dft();

    { // dump C array
        if (strided_access) {
            snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/__tcbn_sampler_1D_%ispp_stride.inl",
                     SampleCount);
        } else {
            snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/__tcbn_sampler_1D_%ispp.inl",
                     SampleCount);
        }
        std::ofstream out_file(name_buf, std::ios::binary);
        out_file << "const int w = " << TileRes << ";\n";
        out_file << "const int h = " << TileRes << ";\n";
        out_file << "const int d = " << SampleCount << ";\n";
        out_file << "const uint8_t tcbn_samples[" << SampleCount * TileRes * TileRes << "] = {\n    ";
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

template <int Log2SampleCount, Eng::eSpatialFilter sf, Eng::eTemporalFilter tf>
void Eng::Generate1D_TCBN_Swap(const unsigned int seed) {
    using namespace BNInternal;
    using namespace TCBN;

    static const int SampleCount = (1 << Log2SampleCount);

    // Dynamic allocation is used to avoid stack overflow
    struct bn_data_t {
        float samples[SampleCount][TileRes][TileRes];
        std::vector<float> errors[SampleCount][TileRes][TileRes];
        float proximity[SampleCount][TileRes][TileRes] = {};

        // filter LUTs
        float xy_filter[XYRadius + 1][XYRadius + 1] = {};
        float z_filter[SampleCount] = {};

        // temp data
        float debug_values[SampleCount][TileRes][TileRes] = {};
        float debug_values2[TileRes][TileRes] = {};
        float debug_values3[TileRes][SampleCount][TileRes] = {};
        std::complex<float> debug_dft[TileRes][TileRes] = {};
    };
    auto data = std::make_unique<bn_data_t>();

    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> uniform_index(0, SampleCount * TileRes * TileRes - 1);
    std::uniform_real_distribution<float> uniform_unorm_float(0.0f, 1.0f);

    char name_buf[128];

    // Init spatial filter
    if constexpr (sf == eSpatialFilter::Gauss) {
        for (int dy = 0; dy <= XYRadius; ++dy) {
            for (int dx = 0; dx <= XYRadius; ++dx) {
                data->xy_filter[dy][dx] = std::exp(-(dx * dx + dy * dy) / GaussOmegaI);
            }
        }
    }

    // Init temporal filter
    if constexpr (tf == eTemporalFilter::Gauss) {
    } else if constexpr (tf == eTemporalFilter::TruncatedEMA) {
        // Stochastically sample the EMA filter.
        // This totally can be solved analytically, but I'm too lazy to do it.
        const int IterationsCount = 1000000;
        for (int iter = 0; iter < IterationsCount; ++iter) {
            // Generate history length based on the rejection probability
            int history_len = 0;
            while (history_len++ < SampleCount) {
                const float u = uniform_unorm_float(gen);
                if (u < HistoryRejection) {
                    break;
                }
            }
            for (int i = 0; i < SampleCount; ++i) {
                data->z_filter[i] += truncated_ema(EMA_Alpha, 0, std::max(-i, i - SampleCount), history_len);
            }
        }
        // Normalize
        for (int i = SampleCount - 1; i >= 0; --i) {
            data->z_filter[i] /= data->z_filter[0];
        }
    }

    // Generate initial samples
    for (int z = 0; z < SampleCount; ++z) {
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                const int k = (TileRes * TileRes) / SampleCount;
                // NOTE: Slightly shift towards center to avoid numerical issues in validation (doesn't matter)
                const float off = 0.0001f + float((y * TileRes + x + k * z) % (TileRes * TileRes));
                data->samples[z][y][x] = (off + 0.9998f * uniform_unorm_float(gen)) / float(TileRes * TileRes);
            }
        }
    }
    { // Randomize slices order
        int slice_order[SampleCount];
        std::iota(std::begin(slice_order), std::end(slice_order), 0);
        std::shuffle(std::begin(slice_order), std::end(slice_order), gen);
        for (int i = 0; i < SampleCount; ++i) {
            while (slice_order[i] != i) {
                for (int y = 0; y < TileRes; ++y) {
                    for (int x = 0; x < TileRes; ++x) {
                        std::swap(data->samples[i][y][x], data->samples[slice_order[i]][y][x]);
                    }
                }
                std::swap(slice_order[i], slice_order[slice_order[i]]);
            }
        }
    }

    auto validate_stratification = [&](const bool validate_z) {
        for (int z = 0; z < SampleCount; ++z) {
            bool xy_strata[TileRes * TileRes] = {};
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    const int s = int(data->samples[z][y][x] * (TileRes * TileRes));
                    assert(xy_strata[s] == false);
                    xy_strata[s] = true;
                }
            }
        }
        // Stratification by z is only valid for initial state (before swaps)
        if (!validate_z) {
            return;
        }
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                bool z_strata[SampleCount] = {};
                for (int z = 0; z < SampleCount; ++z) {
                    const int s = int(data->samples[z][y][x] * SampleCount);

                    assert(z_strata[s] == false);
                    z_strata[s] = true;
                }
            }
        }
    };
    validate_stratification(true);

    // Randomize sequences order
    for (int z = 0; z < SampleCount; ++z) {
        std::vector<Ren::Vec2i> seq_order(TileRes * TileRes);
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                seq_order[y * TileRes + x] = Ren::Vec2i{x, y};
            }
        }
        std::shuffle(std::begin(seq_order), std::end(seq_order), gen);
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                while (seq_order[y * TileRes + x] != Ren::Vec2i{x, y}) {
                    const Ren::Vec2i s = seq_order[y * TileRes + x];

                    std::swap(data->samples[z][y][x], data->samples[z][s[1]][s[0]]);
                    std::swap(seq_order[y * TileRes + x], seq_order[s[1] * TileRes + s[0]]);
                }
            }
        }
    }

    { // load checkpoint (if exists)
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/tcbn_samples_1D_%ispp.bin", SampleCount);
        std::ifstream in_file(name_buf, std::ios::binary);
        if (in_file.good()) {
            in_file >> gen;
            in_file.read((char *)data->samples, sizeof(data->samples));
            validate_stratification(false);
        }
    }

    // sample test functions
    for (int z = 0; z < SampleCount; ++z) {
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                for (int i = 0; i < TestFunctionsCount; ++i) {
                    const float u = (i + 0.5f) / TestFunctionsCount;

                    // NOTE: In 1D case optimizing for step function is equivalent of optimizing for sample distance.
                    // It doesn't make sense, we do this for experimentation.
                    const float val = test_function_1D(data->samples[z][y][x], -0.001f, u);
                    const float res = test_function_1D_integral(0.0f, u);

                    data->errors[z][y][x].push_back(res - val);
                }
            }
        }
    }

    auto debug_dft = [&]() {
        float min_val = FLT_MAX, max_val = 0.0f;
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values2[y][x] = data->samples[z][y][x];
                }
            }

            dft_2d(data->debug_values2, data->debug_dft);

            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values[z][y][x] = std::abs(data->debug_dft[y][x]);
                    min_val = std::min(min_val, data->debug_values[z][y][x]);
                    max_val = std::max(max_val, data->debug_values[z][y][x]);
                }
            }
        }

        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            e = (e - min_val) / (max_val - min_val);
        }

        snprintf(name_buf, sizeof(name_buf), "debug_dft_%i.dds", SampleCount);
        WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);
    };

    debug_dft();

    float best_total_proximity = 0.0f;
    for (int z = 0; z < SampleCount; ++z) {
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                data->proximity[z][y][x] =
                    calc_pixel_proximity<SampleCount>(x, y, z, data->errors, data->xy_filter, data->z_filter);
                best_total_proximity += data->proximity[z][y][x];
            }
        }
    }
    float prev_best_total_proximity = best_total_proximity;

    float T = 0.0f;
    for (int iter = 0; iter <= SampleCount * BaseSwappingIterations; ++iter) {
        if ((iter % 10000) == 0) {
            const float delta = best_total_proximity - prev_best_total_proximity;
            printf("Swapping Iteration %i (%f | %+f) T = %f\n", iter, best_total_proximity, delta, T);
            prev_best_total_proximity = best_total_proximity;

            // const float goal_delta = Ren::Mix(100.0f, 0.0f, float(iter) / (SampleCount * BaseSwappingIterations));
            // T = std::clamp(T + 0.00001f * (goal_delta - delta), 0.0f, 0.001f);
            if (delta > 0.0f && delta < 1.0f) {
                T += 0.00005f;
            } else {
                T = std::max(T - 0.0001f, 0.0f);
            }

            if (!AllowZSwaps) {
                validate_stratification(false);
            }

            { // save current state
                snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/tcbn_samples_1D_%ispp.bin",
                         SampleCount);
                std::ofstream out_file(name_buf, std::ios::binary);
                out_file << gen;
                out_file.write((const char *)data->samples, sizeof(data->samples));
            }

            float min_proximity = FLT_MAX, max_proximity = 0.0f;
            for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
                const auto [x, y, z] = xyz_from_index(j);

                data->debug_values[z][y][x] = data->proximity[z][y][x];
                min_proximity = std::min(min_proximity, data->debug_values[z][y][x]);
                max_proximity = std::max(max_proximity, data->debug_values[z][y][x]);
            }
            // normalize errors (for easier debugging)
            for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
                const auto [x, y, z] = xyz_from_index(j);

                float &e = data->debug_values[z][y][x];
                e = (e - min_proximity) / (max_proximity - min_proximity);
            }

            snprintf(name_buf, sizeof(name_buf), "debug_energy_%i.dds", SampleCount);
            WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);

            snprintf(name_buf, sizeof(name_buf), "debug_samples_%i.dds", SampleCount);
            WriteDDS(&data->samples[0][0][0], TileRes, TileRes, SampleCount, name_buf);

            for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
                const auto [x, y, z] = xyz_from_index(j);

                data->debug_values3[y][z][x] = data->samples[z][y][x];
            }

            snprintf(name_buf, sizeof(name_buf), "debug_samples_XZY_%i.dds", SampleCount);
            WriteDDS(&data->debug_values3[0][0][0], TileRes, SampleCount, TileRes, name_buf);
        }

        const int index1 = uniform_index(gen);
        const auto [ox1, oy1, oz1] = xyz_from_index(index1);

        const int CandidatesCount = 16;

        int swap_candidates[CandidatesCount];
        for (int i = 0; i < CandidatesCount; ++i) {
            swap_candidates[i] = uniform_index(gen);
            if (!AllowZSwaps) {
                // Force same Z plane
                swap_candidates[i] = oz1 * TileRes * TileRes + (swap_candidates[i] % (TileRes * TileRes));
            }
        }

        float proximity_cdf[CandidatesCount + 1] = {};
        for (int i = 0; i < CandidatesCount; ++i) {
            const auto [x2, y2, z2] = xyz_from_index(swap_candidates[i]);
            proximity_cdf[i + 1] =
                proximity_cdf[i] + std::abs(data->samples[oz1][oy1][ox1] - data->samples[z2][y2][x2]);
        }
        for (int i = 0; i < CandidatesCount; ++i) {
            proximity_cdf[i + 1] /= proximity_cdf[16];
        }

        const float r = uniform_unorm_float(gen);
        int idx = 0;
        while (idx < CandidatesCount - 1 && r > proximity_cdf[idx + 1]) {
            ++idx;
        }

        // Randomly swap samples
        const int index2 = swap_candidates[idx];
        const auto [ox2, oy2, oz2] = xyz_from_index(index2);

        assert(oz2 == oz1);

        // Subtract swapped pixels contribution
        splat_pixel_proximity<SampleCount, false>(ox1, oy1, oz1, data->errors, data->proximity, data->xy_filter,
                                                  data->z_filter);
        splat_pixel_proximity<SampleCount, false>(ox2, oy2, oz2, data->errors, data->proximity, data->xy_filter,
                                                  data->z_filter);

        std::swap(data->samples[oz1][oy1][ox1], data->samples[oz2][oy2][ox2]);
        std::swap(data->errors[oz1][oy1][ox1], data->errors[oz2][oy2][ox2]);

        // Add swapped pixels contribution
        splat_pixel_proximity<SampleCount, true>(ox1, oy1, oz1, data->errors, data->proximity, data->xy_filter,
                                                 data->z_filter);
        splat_pixel_proximity<SampleCount, true>(ox2, oy2, oz2, data->errors, data->proximity, data->xy_filter,
                                                 data->z_filter);

        // Recalc proximity of changed pixels
        data->proximity[oz1][oy1][ox1] =
            calc_pixel_proximity<SampleCount>(ox1, oy1, oz1, data->errors, data->xy_filter, data->z_filter);
        data->proximity[oz2][oy2][ox2] =
            calc_pixel_proximity<SampleCount>(ox2, oy2, oz2, data->errors, data->xy_filter, data->z_filter);

        float total_proximity = 0.0f;
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    total_proximity += data->proximity[z][y][x];
                }
            }
        }

        if (total_proximity > best_total_proximity || uniform_unorm_float(gen) < T) {
            // Accept this iteration
            best_total_proximity = total_proximity;
        } else {
            // Revert swap
            splat_pixel_proximity<SampleCount, false>(ox1, oy1, oz1, data->errors, data->proximity, data->xy_filter,
                                                      data->z_filter);
            splat_pixel_proximity<SampleCount, false>(ox2, oy2, oz2, data->errors, data->proximity, data->xy_filter,
                                                      data->z_filter);

            std::swap(data->samples[oz1][oy1][ox1], data->samples[oz2][oy2][ox2]);
            std::swap(data->errors[oz1][oy1][ox1], data->errors[oz2][oy2][ox2]);

            splat_pixel_proximity<SampleCount, true>(ox1, oy1, oz1, data->errors, data->proximity, data->xy_filter,
                                                     data->z_filter);
            splat_pixel_proximity<SampleCount, true>(ox2, oy2, oz2, data->errors, data->proximity, data->xy_filter,
                                                     data->z_filter);

            data->proximity[oz1][oy1][ox1] =
                calc_pixel_proximity<SampleCount>(ox1, oy1, oz1, data->errors, data->xy_filter, data->z_filter);
            data->proximity[oz2][oy2][ox2] =
                calc_pixel_proximity<SampleCount>(ox2, oy2, oz2, data->errors, data->xy_filter, data->z_filter);
        }
    }

    debug_dft();

    { // dump C array
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/__tcbn_sampler_1D_%ispp.inl", SampleCount);

        std::ofstream out_file(name_buf, std::ios::binary);
        out_file << "const int w = " << TileRes << ";\n";
        out_file << "const int h = " << TileRes << ";\n";
        out_file << "const int d = " << SampleCount << ";\n";
        out_file << "// best_total_proximity = " << best_total_proximity << "\n";
        out_file << "const uint8_t tcbn_samples[" << SampleCount * TileRes * TileRes << "] = {\n    ";
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    out_file << std::clamp(int(data->samples[z][y][x] * 255.0f), 0, 255);
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

template <int Log2SampleCount> void Eng::Generate2D_TCBN(const unsigned int seed) {
    using namespace BNInternal;
    using namespace TCBN;

    static const int SampleCount = (1 << Log2SampleCount);
    static const int SampleCountSqrt = (1 << (Log2SampleCount / 2));

    // Dynamic allocation is used to avoid stack overflow
    struct bn_data_t {
        std::vector<Ren::Vec2f> samples[TileRes][TileRes];
        float proximity[SampleCount][TileRes][TileRes] = {};

        // temp data
        float debug_values[SampleCount][TileRes][TileRes] = {};
        Ren::Vec2f debug_values2[SampleCount][TileRes][TileRes] = {};
        float debug_values3[TileRes][TileRes] = {};
        std::complex<float> debug_dft[TileRes][TileRes] = {};
    };
    auto data = std::make_unique<bn_data_t>();

    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> uniform_index(0, SampleCount * TileRes * TileRes - 1);
    std::uniform_real_distribution<float> uniform_unorm_float(0.0f, 1.0f);

    char name_buf[128];

    // Generate initial samples
#if 0   // z tratification
    for (int y = 0; y < TileRes; ++y) {
        for (int x = 0; x < TileRes; ++x) {
            for (int z = 0; z < SampleCount; ++z) {
                const float off_x = float(z % SampleCountSqrt);
                const float off_y = float(z / SampleCountSqrt);

                data->samples[y][x].emplace_back(
                    Ren::Vec2f(off_x + uniform_unorm_float(gen), off_y + uniform_unorm_float(gen)) /
                    float(SampleCountSqrt));
            }
            // Randomize the initial order
            std::shuffle(std::begin(data->samples[y][x]), std::end(data->samples[y][x]), gen);
        }
    }
#elif 1 // xyz stratification
    for (int z = 0; z < SampleCount; ++z) {
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                const int k = TileRes / SampleCountSqrt;
                const float off_x = float((x + k * (z % SampleCountSqrt)) % TileRes);
                const float off_y = float((y + k * (z / SampleCountSqrt)) % TileRes);

                data->samples[y][x].emplace_back(
                    Ren::Vec2f(off_x + uniform_unorm_float(gen), off_y + uniform_unorm_float(gen)) / float(TileRes));
            }
        }
    }
    // Randomize slices order
    int slice_order[SampleCount];
    std::iota(std::begin(slice_order), std::end(slice_order), 0);
    std::shuffle(std::begin(slice_order), std::end(slice_order), gen);
    for (int i = 0; i < SampleCount; ++i) {
        while (slice_order[i] != i) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    std::swap(data->samples[y][x][i], data->samples[y][x][slice_order[i]]);
                }
            }
            std::swap(slice_order[i], slice_order[slice_order[i]]);
        }
    }
    // Randomize sequences order
    std::shuffle(&data->samples[0][0], &data->samples[0][0] + TileRes * TileRes, gen);
#endif

    auto validate_stratification = [&]() {
        for (int z = 0; z < SampleCount; ++z) {
            bool xy_strata[TileRes][TileRes] = {};
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    const auto s = Ren::Vec2i(data->samples[y][x][z] * TileRes);

                    assert(xy_strata[s[1]][s[0]] == false);
                    xy_strata[s[1]][s[0]] = true;
                }
            }
        }
        /*for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                bool z_strata[SampleCountSqrt][SampleCountSqrt] = {};
                for (int z = 0; z < SampleCount; ++z) {
                    const auto s = Ren::Vec2i(data->samples[y][x][z] * SampleCountSqrt);

                    assert(z_strata[s[1]][s[0]] == false);
                    z_strata[s[1]][s[0]] = true;
                }
            }
        }*/
    };

    auto debug_dft = [&]() {
        float min_val = FLT_MAX, max_val = 0.0f;
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values3[y][x] = data->samples[y][x][z][0];
                }
            }

            dft_2d(data->debug_values3, data->debug_dft);

            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->debug_values[z][y][x] = std::abs(data->debug_dft[y][x]);
                    min_val = std::min(min_val, data->debug_values[z][y][x]);
                    max_val = std::max(max_val, data->debug_values[z][y][x]);
                }
            }
        }

        for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
            const auto [x, y, z] = xyz_from_index(j);

            float &e = data->debug_values[z][y][x];
            // e = (e - min_val) / (max_val - min_val);
            e /= max_val;
        }

        snprintf(name_buf, sizeof(name_buf), "debug_dft_%i.dds", SampleCount);
        WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);
    };

    debug_dft();

    for (int i = 0; i < SampleCount * TileRes * TileRes; ++i) {
        const auto [x, y, z] = xyz_from_index(i);

        data->debug_values2[z][y][x] = data->samples[y][x][z];
    }

    snprintf(name_buf, sizeof(name_buf), "debug_samples_%i.dds", SampleCount);
    WriteDDS(&data->debug_values2[0][0][0], TileRes, TileRes, SampleCount, name_buf);

    float best_total_proximity = 0.0f;
    for (int z = 0; z < SampleCount; ++z) {
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                data->proximity[z][y][x] = calc_pixel_proximity<SampleCount>(x, y, z, data->samples);
                best_total_proximity += data->proximity[z][y][x];
            }
        }
    }

    for (int iter = 0; iter <= SampleCount * BaseSwappingIterations; ++iter) {
        if ((iter % 10000) == 0) {
            printf("Swapping Iteration %i (%f)\n", iter, best_total_proximity);

            validate_stratification();
            debug_dft();

            float min_proximity = FLT_MAX, max_proximity = 0.0f;
            for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
                const auto [x, y, z] = xyz_from_index(j);

                data->debug_values[z][y][x] = data->proximity[z][y][x];
                min_proximity = std::min(min_proximity, data->debug_values[z][y][x]);
                max_proximity = std::max(max_proximity, data->debug_values[z][y][x]);
            }
            // normalize errors (for easier debugging)
            for (int j = 0; j < SampleCount * TileRes * TileRes; ++j) {
                const auto [x, y, z] = xyz_from_index(j);

                float &e = data->debug_values[z][y][x];
                e = (e - min_proximity) / (max_proximity - min_proximity);

                data->debug_values2[z][y][x] = data->samples[y][x][z];
            }

            snprintf(name_buf, sizeof(name_buf), "debug_energy_%i.dds", SampleCount);
            WriteDDS(&data->debug_values[0][0][0], TileRes, TileRes, SampleCount, name_buf);

            snprintf(name_buf, sizeof(name_buf), "debug_samples_%i.dds", SampleCount);
            WriteDDS(&data->debug_values2[0][0][0], TileRes, TileRes, SampleCount, name_buf);
        }

        // Randomly swap samples
        int index1 = uniform_index(gen), index2 = uniform_index(gen);
        const auto [ox1, oy1, oz1] = xyz_from_index(index1);
        auto [ox2, oy2, oz2] = xyz_from_index(index2);

        if ((iter % 100) < 99 || oz1 == oz2) {
            // Swap samples within the same XY slice
            oz2 = oz1;

            // Make sure the samples are from the same strata in Z dimension
            /*const Ren::Vec2i strata1 = Ren::Vec2i(data->samples[oy1][ox1][oz1] * SampleCountSqrt);
            Ren::Vec2i strata2 = Ren::Vec2i(data->samples[oy2][ox2][oz2] * SampleCountSqrt);
            while (strata2 != strata1) {
                index2 = uniform_index(gen);
                const auto [_ox2, _oy2, _oz2] = xyz_from_index(index2);
                ox2 = _ox2, oy2 = _oy2;
                strata2 = Ren::Vec2i(data->samples[oy2][ox2][oz2] * SampleCountSqrt);
            }*/
        } else {
            // Swap the whole sequence
        }

        if (oz1 == oz2) {
            //
            // Swap single sample within the sequence
            //
            const int oz = oz1;

            // Subtract swapped pixels contribution
            splat_pixel_proximity<SampleCount, false>(ox1, oy1, oz, data->samples, data->proximity);
            splat_pixel_proximity<SampleCount, false>(ox2, oy2, oz, data->samples, data->proximity);

            const Ren::Vec2i strata1 = Ren::Vec2i(data->samples[oy1][ox1][oz1] * SampleCountSqrt);
            const Ren::Vec2i strata2 = Ren::Vec2i(data->samples[oy2][ox2][oz2] * SampleCountSqrt);

            std::swap(data->samples[oy1][ox1][oz], data->samples[oy2][ox2][oz]);

            // Add swapped pixels contribution
            splat_pixel_proximity<SampleCount, true>(ox1, oy1, oz, data->samples, data->proximity);
            splat_pixel_proximity<SampleCount, true>(ox2, oy2, oz, data->samples, data->proximity);

            // Recalc proximity of changed pixels
            data->proximity[oz][oy1][ox1] = calc_pixel_proximity<SampleCount>(ox1, oy1, oz, data->samples);
            data->proximity[oz][oy2][ox2] = calc_pixel_proximity<SampleCount>(ox2, oy2, oz, data->samples);
        } else {
            //
            // Swap the whole sequence
            //

            // Subtract swapped pixels contribution
            for (int oz = 0; oz < SampleCount; ++oz) {
                splat_pixel_proximity<SampleCount, false>(ox1, oy1, oz, data->samples, data->proximity);
                splat_pixel_proximity<SampleCount, false>(ox2, oy2, oz, data->samples, data->proximity);
            }

            std::swap(data->samples[oy1][ox1], data->samples[oy2][ox2]);

            // Add swapped pixels contribution
            for (int oz = 0; oz < SampleCount; ++oz) {
                splat_pixel_proximity<SampleCount, true>(ox1, oy1, oz, data->samples, data->proximity);
                splat_pixel_proximity<SampleCount, true>(ox2, oy2, oz, data->samples, data->proximity);
            }

            // Recalc proximity of changed pixels
            for (int oz = 0; oz < SampleCount; ++oz) {
                data->proximity[oz][oy1][ox1] = calc_pixel_proximity<SampleCount>(ox1, oy1, oz, data->samples);
                data->proximity[oz][oy2][ox2] = calc_pixel_proximity<SampleCount>(ox2, oy2, oz, data->samples);
            }
        }

        float total_proximity = 0.0f;
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    total_proximity += data->proximity[z][y][x];
                }
            }
        }

        const float acceptance_prob = fminf(powf(0.95f, 0.001f * iter), 0.001f);
        if (total_proximity < best_total_proximity || uniform_unorm_float(gen) < acceptance_prob) {
            if (total_proximity > best_total_proximity) {
                printf("Suboptimal iteration accepted!\n");
            }
            // Accept this iteration
            best_total_proximity = total_proximity;
            if (oz1 != oz2) {
                printf("Sequence swap accepted!\n");
            }
        } else {
            // Revert swap
            if (oz1 == oz2) {
                const int oz = oz1;

                splat_pixel_proximity<SampleCount, false>(ox1, oy1, oz, data->samples, data->proximity);
                splat_pixel_proximity<SampleCount, false>(ox2, oy2, oz, data->samples, data->proximity);

                std::swap(data->samples[oy1][ox1][oz], data->samples[oy2][ox2][oz]);

                splat_pixel_proximity<SampleCount, true>(ox1, oy1, oz, data->samples, data->proximity);
                splat_pixel_proximity<SampleCount, true>(ox2, oy2, oz, data->samples, data->proximity);

                data->proximity[oz][oy1][ox1] = calc_pixel_proximity<SampleCount>(ox1, oy1, oz, data->samples);
                data->proximity[oz][oy2][ox2] = calc_pixel_proximity<SampleCount>(ox2, oy2, oz, data->samples);
            } else {
                for (int oz = 0; oz < SampleCount; ++oz) {
                    splat_pixel_proximity<SampleCount, false>(ox1, oy1, oz, data->samples, data->proximity);
                    splat_pixel_proximity<SampleCount, false>(ox2, oy2, oz, data->samples, data->proximity);
                }

                std::swap(data->samples[oy1][ox1], data->samples[oy2][ox2]);

                for (int oz = 0; oz < SampleCount; ++oz) {
                    splat_pixel_proximity<SampleCount, true>(ox1, oy1, oz, data->samples, data->proximity);
                    splat_pixel_proximity<SampleCount, true>(ox2, oy2, oz, data->samples, data->proximity);
                }

                for (int oz = 0; oz < SampleCount; ++oz) {
                    data->proximity[oz][oy1][ox1] = calc_pixel_proximity<SampleCount>(ox1, oy1, oz, data->samples);
                    data->proximity[oz][oy2][ox2] = calc_pixel_proximity<SampleCount>(ox2, oy2, oz, data->samples);
                }
            }
        }
    }

    { // dump C array
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/__tcbn_sampler_2D_%ispp.inl", SampleCount);

        std::ofstream out_file(name_buf, std::ios::binary);
        out_file << "const int w = " << TileRes << ";\n";
        out_file << "const int h = " << TileRes << ";\n";
        out_file << "const int d = " << SampleCount << ";\n";
        out_file << "// best_total_proximity = " << best_total_proximity << "\n";
        out_file << "const uint8_t tcbn_samples[" << 2 * SampleCount * TileRes * TileRes << "] = {\n    ";
        for (int z = 0; z < SampleCount; ++z) {
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    out_file << std::clamp(int(data->samples[y][x][z][0] * 255.0f), 0, 255);
                    out_file << "u, ";
                    out_file << std::clamp(int(data->samples[y][x][z][1] * 255.0f), 0, 255);
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

template void Eng::Generate1D_TCBN_VC<4, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::Gauss>(unsigned int seed,
                                                                                                  bool strided_access);
template void
Eng::Generate1D_TCBN_VC<4, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::TruncatedEMA>(unsigned int seed,
                                                                                           bool strided_access);
template void Eng::Generate1D_TCBN_VC<6, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::Gauss>(unsigned int seed,
                                                                                                  bool strided_access);
template void
Eng::Generate1D_TCBN_VC<6, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::TruncatedEMA>(unsigned int seed,
                                                                                           bool strided_access);

template void Eng::Generate1D_TCBN_Swap<4, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::Gauss>(unsigned int seed);
template void
Eng::Generate1D_TCBN_Swap<4, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::TruncatedEMA>(unsigned int seed);
template void Eng::Generate1D_TCBN_Swap<6, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::Gauss>(unsigned int seed);
template void
Eng::Generate1D_TCBN_Swap<6, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::TruncatedEMA>(unsigned int seed);

// template void Eng::Generate2D_TCBN<0>(unsigned int seed);
// template void Eng::Generate2D_TCBN<1>(unsigned int seed);
// template void Eng::Generate2D_TCBN<2>(unsigned int seed);
// template void Eng::Generate2D_TCBN<3>(unsigned int seed);
template void Eng::Generate2D_TCBN<4>(unsigned int seed);
// template void Eng::Generate2D_TCBN<5>(unsigned int seed);
template void Eng::Generate2D_TCBN<6>(unsigned int seed);
// template void Eng::Generate2D_TCBN<7>(unsigned int seed);
// template void Eng::Generate2D_TCBN<8>(unsigned int seed);