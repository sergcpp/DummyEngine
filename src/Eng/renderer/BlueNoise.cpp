#include "BlueNoise.h"

#include <cassert>
#include <fstream>
#include <random>

namespace Eng {
namespace BNInternal {
static const int SampleCountPow = 4;
static const int SampleCount = (1 << SampleCountPow);
static const int TileRes = 128;
static const int TotalFunctionsCount = 1024;
// Ideally this should be equal to tile res, but this would be too slow due to quadratic complexity
static const int ProximityRadius = 4;
static const float ProximityGoal = 6500.0f;
static const int MaxScramblingIterations = 100000;
static const int MaxSortingIterations = 1000000;

static const float GaussOmega = 2.1f;
static const float GaussTable[] = {
    std::exp(-0 / GaussOmega),  std::exp(-1 / GaussOmega),  std::exp(-2 / GaussOmega),  std::exp(-3 / GaussOmega),
    std::exp(-4 / GaussOmega),  std::exp(-5 / GaussOmega),  std::exp(-6 / GaussOmega),  std::exp(-7 / GaussOmega),
    std::exp(-8 / GaussOmega),  std::exp(-9 / GaussOmega),  std::exp(-10 / GaussOmega), std::exp(-11 / GaussOmega),
    std::exp(-12 / GaussOmega), std::exp(-13 / GaussOmega), std::exp(-14 / GaussOmega), std::exp(-15 / GaussOmega),
    std::exp(-16 / GaussOmega), std::exp(-17 / GaussOmega), std::exp(-18 / GaussOmega), std::exp(-19 / GaussOmega),
    std::exp(-20 / GaussOmega), std::exp(-21 / GaussOmega), std::exp(-22 / GaussOmega), std::exp(-23 / GaussOmega),
    std::exp(-24 / GaussOmega), std::exp(-25 / GaussOmega), std::exp(-26 / GaussOmega), std::exp(-27 / GaussOmega),
    std::exp(-28 / GaussOmega), std::exp(-29 / GaussOmega), std::exp(-30 / GaussOmega), std::exp(-31 / GaussOmega),
    std::exp(-32 / GaussOmega), std::exp(-33 / GaussOmega), std::exp(-34 / GaussOmega), std::exp(-35 / GaussOmega),
    std::exp(-36 / GaussOmega), std::exp(-37 / GaussOmega), std::exp(-38 / GaussOmega), std::exp(-39 / GaussOmega),
    std::exp(-40 / GaussOmega), std::exp(-41 / GaussOmega), std::exp(-42 / GaussOmega), std::exp(-43 / GaussOmega),
    std::exp(-44 / GaussOmega), std::exp(-45 / GaussOmega), std::exp(-46 / GaussOmega), std::exp(-47 / GaussOmega),
    std::exp(-48 / GaussOmega), std::exp(-49 / GaussOmega), std::exp(-50 / GaussOmega), std::exp(-51 / GaussOmega),
    std::exp(-52 / GaussOmega), std::exp(-53 / GaussOmega), std::exp(-54 / GaussOmega), std::exp(-55 / GaussOmega),
    std::exp(-56 / GaussOmega), std::exp(-57 / GaussOmega), std::exp(-58 / GaussOmega), std::exp(-59 / GaussOmega),
    std::exp(-60 / GaussOmega), std::exp(-61 / GaussOmega), std::exp(-62 / GaussOmega), std::exp(-63 / GaussOmega),
    std::exp(-64 / GaussOmega), std::exp(-65 / GaussOmega), std::exp(-66 / GaussOmega), std::exp(-67 / GaussOmega),
    std::exp(-68 / GaussOmega), std::exp(-69 / GaussOmega), std::exp(-70 / GaussOmega), std::exp(-71 / GaussOmega),
    std::exp(-72 / GaussOmega), std::exp(-73 / GaussOmega), std::exp(-74 / GaussOmega), std::exp(-75 / GaussOmega),
    std::exp(-76 / GaussOmega), std::exp(-77 / GaussOmega), std::exp(-78 / GaussOmega), std::exp(-79 / GaussOmega),
    std::exp(-80 / GaussOmega), std::exp(-81 / GaussOmega), std::exp(-82 / GaussOmega), std::exp(-83 / GaussOmega),
    std::exp(-84 / GaussOmega), std::exp(-85 / GaussOmega), std::exp(-86 / GaussOmega), std::exp(-87 / GaussOmega)};

// Simple step
float test_function(float x, float min, float max) {
    if (x > min && x < max) {
        return 1.0f;
    }
    return 0.0f;
}
float integral_val(float min, float max) { return max - min; }

void evaluate_integrals(const float samples[], const uint32_t sample_count, const uint32_t sample_sorting_key,
                        float out_errors[]) {
    for (uint32_t j = 0; j < TotalFunctionsCount; ++j) {
        const float point = float(j) / float(TotalFunctionsCount - 1);

        float result = 0.0f;
        for (uint32_t k = 0; k < sample_count; ++k) {
            result += test_function(samples[k ^ sample_sorting_key], 0.0f - 0.1f, point);
        }
        result /= sample_count;

        const float integral = integral_val(0.0f, point);
        out_errors[j] = std::abs(result - integral);
    }
}

float calc_pixel_proximity(const int ox, const int oy, const std::vector<float> values[TileRes][TileRes]) {
    const float *v = values[oy][ox].data();

    double total_proximity = 0.0;
    for (int y = oy - ProximityRadius; y <= oy + ProximityRadius; ++y) {
        const int wrapped_y = (y + TileRes) % TileRes;
        const std::vector<float> *y_vals = values[wrapped_y];
        for (int x = ox - ProximityRadius; x <= ox + ProximityRadius; ++x) {
            const int wrapped_x = (x + TileRes) % TileRes;
            if (wrapped_x == ox && wrapped_y == oy) {
                continue;
            }

            const int i = (x - ox) * (x - ox) + (y - oy) * (y - oy);
            // total_proximity += std::exp(-((x - ox) * (x - ox) + (y - oy) * (y - oy)) / GaussOmega) *
            //                    (values[wrapped_y][wrapped_x] - v) * (values[wrapped_y][wrapped_x] - v);
            assert(i < std::size(GaussTable));

            const float *vals = y_vals[wrapped_x].data();

            float proximity = 0.0f;
            for (int j = 0; j < TotalFunctionsCount; ++j) {
                proximity += (vals[j] - v[j]) * (vals[j] - v[j]);
            }
            total_proximity += GaussTable[i] * proximity;
        }
    }
    return float(total_proximity / TotalFunctionsCount);
}

float calc_pixel_proximity(const int ox, const int oy, const std::vector<float> values_first[TileRes][TileRes],
                           const std::vector<float> values_last[TileRes][TileRes]) {
    const std::vector<float> &v_first = values_first[oy][ox], &v_last = values_last[oy][ox];

    double total_proximity = 0.0f;
    for (int y = oy - ProximityRadius; y <= oy + ProximityRadius; ++y) {
        const int wrapped_y = (y + TileRes) % TileRes;
        for (int x = ox - ProximityRadius; x <= ox + ProximityRadius; ++x) {
            const int wrapped_x = (x + TileRes) % TileRes;
            if (wrapped_x == ox && wrapped_y == oy) {
                continue;
            }

            const int i = (x - ox) * (x - ox) + (y - oy) * (y - oy);
            assert(i < std::size(GaussTable));

            float proximity = 0.0f;
            for (int j = 0; j < TotalFunctionsCount; ++j) {
                proximity += (values_first[wrapped_y][wrapped_x][j] - v_first[j]) *
                                 (values_first[wrapped_y][wrapped_x][j] - v_first[j]) +
                             (values_last[wrapped_y][wrapped_x][j] - v_last[j]) *
                                 (values_last[wrapped_y][wrapped_x][j] - v_last[j]);
            }
            total_proximity += GaussTable[i] * proximity;
        }
    }
    return float(0.5 * total_proximity / TotalFunctionsCount);
}

uint32_t hash(uint32_t x) {
    // finalizer from murmurhash3
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

void WriteTGA(const float *data, int pitch, const int w, const int h, int px_stride, const int bpp, const char *name);

} // namespace BNInternal
} // namespace Eng

void Eng::Generate1D_BlueNoiseTiles_StepFunction(const uint32_t initial_samples[]) {
    using namespace BNInternal;

    // Dynamic allocation is used to avoid stack overflow
    struct bn_data_t {
        std::vector<float> samples[TileRes][TileRes];
        uint32_t scrambling_keys[TileRes][TileRes] = {};
        uint32_t sorting_keys[TileRes][TileRes] = {};

        // temp data
        float proximity[TileRes][TileRes] = {};
        std::vector<float> errors[TileRes][TileRes];
        std::vector<float> errors_first[TileRes][TileRes], errors_last[TileRes][TileRes];
        float debug_errors[TileRes][TileRes];
    };
    std::unique_ptr<bn_data_t> data = std::make_unique<bn_data_t>();

    for (int y = 0; y < TileRes; ++y) {
        for (int x = 0; x < TileRes; ++x) {
            data->scrambling_keys[y][x] = hash(y * TileRes + x) & 0xffffff00;
        }
    }

    char name_buf[128];
    { // load scrambling keys
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/scrambling_keys_1D_%ispp.bin", SampleCount);
        std::ifstream in_file(name_buf, std::ios::binary | std::ios::ate);
        const size_t in_file_size = size_t(in_file.tellg());
        if (in_file_size == sizeof(data->scrambling_keys)) {
            in_file.seekg(0, std::ios::beg);
            in_file.read((char *)data->scrambling_keys, sizeof(data->scrambling_keys));
        }
    }
    { // load sorting keys
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/sorting_keys_1D_%ispp.bin", SampleCount);
        std::ifstream in_file(name_buf, std::ios::binary | std::ios::ate);
        const size_t in_file_size = size_t(in_file.tellg());
        if (in_file_size == sizeof(data->sorting_keys)) {
            in_file.seekg(0, std::ios::beg);
            in_file.read((char *)data->sorting_keys, sizeof(data->sorting_keys));
        }
    }

    // Calculate samples based on current scrambling
    for (int y = 0; y < TileRes; ++y) {
        for (int x = 0; x < TileRes; ++x) {
            data->samples[y][x].resize(SampleCount);
            for (int i = 0; i < SampleCount; ++i) {
                const uint32_t scrambled_val = initial_samples[i] ^ data->scrambling_keys[y][x];
                data->samples[y][x][i] = float(scrambled_val >> 8) / 16777216.0f;
            }
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> uniform_index(0, TileRes * TileRes - 1);
    std::uniform_int_distribution<uint32_t> uniform_uint32(0, 0xffffffff);
    std::uniform_real_distribution<float> uniform_unorm_float(0.0f, 1.0f);

    {     // Scrambling optimization
        { // Sample test functions
            float min_error = FLT_MAX, max_error = 0.0f;
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->errors[y][x].resize(TotalFunctionsCount);
                    evaluate_integrals(data->samples[y][x].data(), SampleCount, data->sorting_keys[y][x],
                                       data->errors[y][x].data());
                    for (const float e : data->errors[y][x]) {
                        min_error = std::min(min_error, e);
                        max_error = std::max(max_error, e);
                    }
                }
            }
            // normalize errors (for easier debugging)
            for (int j = 0; j < TileRes * TileRes; ++j) {
                for (float &e : data->errors[j / TileRes][j % TileRes]) {
                    e = (e - min_error) / (max_error - min_error);
                }
            }
        }

        float best_proximity = 0.0f, last_proximity = 0.0f;
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                data->proximity[y][x] = calc_pixel_proximity(x, y, data->errors);
                best_proximity += data->proximity[y][x];
            }
        }
        last_proximity = best_proximity;
        printf("Best proximity = %f\n", best_proximity);

        for (int iter = 0; iter < MaxScramblingIterations && best_proximity < ProximityGoal; ++iter) {
            if ((iter % 1000) == 0) {
                printf("Annealing Iteration %i\n", iter);
            }

            // Randomly swap two scrambling keys
            const int index1 = uniform_index(gen), index2 = uniform_index(gen);
            const int oy1 = index1 / TileRes, ox1 = index1 % TileRes;
            const int oy2 = index2 / TileRes, ox2 = index2 % TileRes;

            std::swap(data->scrambling_keys[oy1][ox1], data->scrambling_keys[oy2][ox2]);
            std::swap(data->samples[oy1][ox1], data->samples[oy2][ox2]);
            std::swap(data->errors[oy1][ox1], data->errors[oy2][ox2]);

            { // Recalc proximity around changed pixels
                for (int y = oy1 - ProximityRadius; y <= oy1 + ProximityRadius; ++y) {
                    const int wrapped_y = (y + TileRes) % TileRes;
                    for (int x = ox1 - ProximityRadius; x <= ox1 + ProximityRadius; ++x) {
                        const int wrapped_x = (x + TileRes) % TileRes;
                        data->proximity[wrapped_y][wrapped_x] =
                            calc_pixel_proximity(wrapped_x, wrapped_y, data->errors);
                    }
                }
                for (int y = oy2 - ProximityRadius; y <= oy2 + ProximityRadius; ++y) {
                    const int wrapped_y = (y + TileRes) % TileRes;
                    for (int x = ox2 - ProximityRadius; x <= ox2 + ProximityRadius; ++x) {
                        const int wrapped_x = (x + TileRes) % TileRes;
                        data->proximity[wrapped_y][wrapped_x] =
                            calc_pixel_proximity(wrapped_x, wrapped_y, data->errors);
                    }
                }
            }
            float total_proximity = 0.0f;
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    total_proximity += data->proximity[y][x];
                }
            }

            // In simulated annealing we still accept worse permutation sometimes to avoid being stuck in local minimum
            // (not really sure if this helps)
            const float acceptance_prob = std::exp((total_proximity - last_proximity) * best_proximity);
            if (total_proximity > last_proximity || uniform_unorm_float(gen) < acceptance_prob) {
                // Accept this iteration (don't swap back)
                last_proximity = total_proximity;
                if (total_proximity > best_proximity) {
                    best_proximity = total_proximity;
                    printf("Best proximity = %f\n", best_proximity);

                    { // save current state
                        snprintf(name_buf, sizeof(name_buf),
                                 "src/Eng/renderer/precomputed/scrambling_keys_1D_%ispp.bin", SampleCount);
                        std::ofstream out_file(name_buf, std::ios::binary);
                        out_file.write((const char *)data->scrambling_keys, sizeof(data->scrambling_keys));
                    }

                    float min_error = FLT_MAX, max_error = 0.0f;
                    for (int j = 0; j < TileRes * TileRes; ++j) {
                        data->debug_errors[j / TileRes][j % TileRes] +=
                            data->errors[j / TileRes][j % TileRes][7 * TotalFunctionsCount / 11];
                        min_error = std::min(min_error, data->debug_errors[j / TileRes][j % TileRes]);
                        max_error = std::max(max_error, data->debug_errors[j / TileRes][j % TileRes]);
                    }
                    // normalize errors (for easier debugging)
                    for (int j = 0; j < TileRes * TileRes; ++j) {
                        float &e = data->debug_errors[j / TileRes][j % TileRes];
                        e = (e - min_error) / (max_error - min_error);
                    }
                    snprintf(name_buf, sizeof(name_buf), "debug_errors_%i_%i.tga", SampleCount, SampleCount);
                    WriteTGA(&data->debug_errors[0][0], TileRes, TileRes, TileRes, 1, 3, name_buf);
                }
            } else {
                // Discard this iteration (swap values back)
                std::swap(data->scrambling_keys[oy1][ox1], data->scrambling_keys[oy2][ox2]);
                std::swap(data->samples[oy1][ox1], data->samples[oy2][ox2]);
                std::swap(data->errors[oy1][ox1], data->errors[oy2][ox2]);

                { // Recalc proximity around changed pixels
                    for (int y = oy1 - ProximityRadius; y <= oy1 + ProximityRadius; ++y) {
                        const int wrapped_y = (y + TileRes) % TileRes;
                        for (int x = ox1 - ProximityRadius; x <= ox1 + ProximityRadius; ++x) {
                            const int wrapped_x = (x + TileRes) % TileRes;
                            data->proximity[wrapped_y][wrapped_x] =
                                calc_pixel_proximity(wrapped_x, wrapped_y, data->errors);
                        }
                    }
                    for (int y = oy2 - ProximityRadius; y <= oy2 + ProximityRadius; ++y) {
                        const int wrapped_y = (y + TileRes) % TileRes;
                        for (int x = ox2 - ProximityRadius; x <= ox2 + ProximityRadius; ++x) {
                            const int wrapped_x = (x + TileRes) % TileRes;
                            data->proximity[wrapped_y][wrapped_x] =
                                calc_pixel_proximity(wrapped_x, wrapped_y, data->errors);
                        }
                    }
                }
            }
        }
    }

    //
    // At this point we have our scrambling keys optimized for SampleCount samples taken,
    // now we need to find sorting keys which make it work for any power of 2 samples subset
    //

    { // Sorting optimization
        int subset_count_pow = SampleCountPow - 1;
        while (subset_count_pow >= 0) {
            const uint32_t subset_sample_count = (1u << subset_count_pow);

            float min_error = FLT_MAX, max_error = 0.0f;
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->errors_first[y][x].resize(TotalFunctionsCount);
                    evaluate_integrals(data->samples[y][x].data(), subset_sample_count, data->sorting_keys[y][x],
                                       data->errors_first[y][x].data());
                    data->errors_last[y][x].resize(TotalFunctionsCount);
                    evaluate_integrals(data->samples[y][x].data(), subset_sample_count,
                                       data->sorting_keys[y][x] ^ subset_sample_count, data->errors_last[y][x].data());
                    for (const float e : data->errors_first[y][x]) {
                        min_error = std::min(min_error, e);
                        max_error = std::max(max_error, e);
                    }
                    for (const float e : data->errors_last[y][x]) {
                        min_error = std::min(min_error, e);
                        max_error = std::max(max_error, e);
                    }
                }
            }
            // normalize (for easier debugging)
            for (int j = 0; j < TileRes * TileRes; ++j) {
                for (float &e : data->errors_first[j / TileRes][j % TileRes]) {
                    e = (e - min_error) / (max_error - min_error);
                }
                for (float &e : data->errors_last[j / TileRes][j % TileRes]) {
                    e = (e - min_error) / (max_error - min_error);
                }
            }

            float best_proximity = 0.0f, last_proximity = 0.0f;
            for (int y = 0; y < TileRes; ++y) {
                for (int x = 0; x < TileRes; ++x) {
                    data->proximity[y][x] = calc_pixel_proximity(x, y, data->errors_first, data->errors_last);
                    best_proximity += data->proximity[y][x];
                }
            }
            last_proximity = best_proximity;
            printf("Best proximity (%i) = %f\n", subset_sample_count, best_proximity);

            for (int iter = 0; iter < MaxSortingIterations && best_proximity < ProximityGoal; ++iter) {
                if ((iter % 1000) == 0) {
                    printf("Iteration %i\n", iter);
                }
                // Randomly flip one pixel
                const int index = uniform_index(gen);
                const int py = index / TileRes, px = index % TileRes;

                data->sorting_keys[py][px] ^= subset_sample_count;
                std::swap(data->errors_first[py][px], data->errors_last[py][px]);

                // Recalc proximity around changed pixel
                for (int y = py - ProximityRadius; y <= py + ProximityRadius; ++y) {
                    const int wrapped_y = (y + TileRes) % TileRes;
                    for (int x = px - ProximityRadius; x <= px + ProximityRadius; ++x) {
                        const int wrapped_x = (x + TileRes) % TileRes;
                        data->proximity[wrapped_y][wrapped_x] =
                            calc_pixel_proximity(wrapped_x, wrapped_y, data->errors_first, data->errors_last);
                    }
                }

                float total_proximity = 0.0f;
                for (int y = 0; y < TileRes; ++y) {
                    for (int x = 0; x < TileRes; ++x) {
                        total_proximity += data->proximity[y][x];
                    }
                }

                const float acceptance_prob = std::exp((total_proximity - last_proximity) * best_proximity);
                if (total_proximity > last_proximity || uniform_unorm_float(gen) < acceptance_prob) {
                    // Accept this iteration (don't swap back)
                    last_proximity = total_proximity;
                    if (total_proximity > best_proximity) {
                        best_proximity = total_proximity;
                        printf("Best proximity (%i) = %f\n", subset_sample_count, best_proximity);

                        { // save current state
                            snprintf(name_buf, sizeof(name_buf),
                                     "src/Eng/renderer/precomputed/sorting_keys_1D_%ispp.bin", SampleCount);
                            std::ofstream out_file(name_buf, std::ios::binary);
                            out_file.write((const char *)data->sorting_keys, sizeof(data->sorting_keys));
                        }

                        float min_error = FLT_MAX, max_error = 0.0f;
                        for (int j = 0; j < TileRes * TileRes; ++j) {
                            data->debug_errors[j / TileRes][j % TileRes] +=
                                data->errors_first[j / TileRes][j % TileRes][7 * TotalFunctionsCount / 11];
                            min_error = std::min(min_error, data->debug_errors[j / TileRes][j % TileRes]);
                            max_error = std::max(max_error, data->debug_errors[j / TileRes][j % TileRes]);
                        }
                        // normalize errors (for easier debugging)
                        for (int j = 0; j < TileRes * TileRes; ++j) {
                            float &e = data->debug_errors[j / TileRes][j % TileRes];
                            e = (e - min_error) / (max_error - min_error);
                        }

                        snprintf(name_buf, sizeof(name_buf), "debug_errors_%i_%i.tga", SampleCount,
                                 subset_sample_count);
                        WriteTGA(&data->debug_errors[0][0], TileRes, TileRes, TileRes, 1, 3, name_buf);
                    }
                } else {
                    // Discard this iteration (swap values back)
                    data->sorting_keys[py][px] ^= subset_sample_count;
                    std::swap(data->errors_first[py][px], data->errors_last[py][px]);

                    // Recalc proximity around changed pixel
                    for (int y = py - ProximityRadius; y <= py + ProximityRadius; ++y) {
                        const int wrapped_y = (y + TileRes) % TileRes;
                        for (int x = px - ProximityRadius; x <= px + ProximityRadius; ++x) {
                            const int wrapped_x = (x + TileRes) % TileRes;
                            data->proximity[wrapped_y][wrapped_x] =
                                calc_pixel_proximity(wrapped_x, wrapped_y, data->errors_first, data->errors_last);
                        }
                    }
                }
            }

            --subset_count_pow;
        }
    }

    { // dump C array
        snprintf(name_buf, sizeof(name_buf), "src/Eng/renderer/precomputed/__bn_sampler_1D_%ispp.inl", SampleCount);
        std::ofstream out_file(name_buf, std::ios::binary);
        out_file << "const int SampleCount = " << SampleCount << ";\n";
        out_file << "const int TileRes = " << TileRes << ";\n";
        out_file << "// Data consists of <(SampleCount) number of samples><(TileRes * TileRes) number of scrambling "
                    "keys><(TileRes * TileRes) number of sorting keys>\n";
        out_file << "const uint32_t bn_pmj_data[" << SampleCount + 2 * TileRes * TileRes << "] = {\n    ";
        for (int i = 0; i < SampleCount; ++i) {
            out_file << initial_samples[i] << "u, ";
        }
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                out_file << data->scrambling_keys[y][x] << "u, ";
            }
        }
        for (int i = 0; i < TileRes; ++i) {
            for (int j = 0; j < TileRes; ++j) {
                out_file << data->sorting_keys[i][j];
                if (i != TileRes - 1 || j != TileRes - 1) {
                    out_file << "u, ";
                } else {
                    out_file << "u\n";
                }
            }
        }
        out_file << "};\n";
    }
}

#define float_to_byte(val)                                                                                             \
    (((val) <= 0.0f) ? 0 : (((val) > (1.0f - 0.5f / 255.0f)) ? 255 : uint8_t((255.0f * (val)) + 0.5f)))

void Eng::BNInternal::WriteTGA(const float *data, int pitch, const int w, const int h, int px_stride, const int bpp,
                               const char *name) {
    std::ofstream file(name, std::ios::binary);

    unsigned char header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    header[12] = w & 0xFF;
    header[13] = (w >> 8) & 0xFF;
    header[14] = (h) & 0xFF;
    header[15] = (h >> 8) & 0xFF;
    header[16] = bpp * 8;
    header[17] |= (1 << 5); // set origin to upper left corner

    file.write((char *)&header[0], sizeof(header));

    auto out_data = std::unique_ptr<uint8_t[]>{new uint8_t[size_t(w) * h * bpp]};
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            out_data[(j * w + i) * bpp + 0] = float_to_byte(data[px_stride * (j * pitch + i)]);
            out_data[(j * w + i) * bpp + 1] = float_to_byte(data[px_stride * (j * pitch + i)]);
            out_data[(j * w + i) * bpp + 2] = float_to_byte(data[px_stride * (j * pitch + i)]);
            if (bpp == 4) {
                out_data[i * 4 + 3] = float_to_byte(data[px_stride * (j * pitch + i)]);
            }
        }
    }

    file.write((const char *)&out_data[0], size_t(w) * h * bpp);

    static const char footer[26] = "\0\0\0\0"         // no extension area
                                   "\0\0\0\0"         // no developer directory
                                   "TRUEVISION-XFILE" // yep, this is a TGA file
                                   ".";
    file.write(footer, sizeof(footer));
}

#undef float_to_byte