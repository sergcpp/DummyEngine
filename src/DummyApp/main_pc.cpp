#include "DummyApp.h"

#include <fstream>
#include <random>

#include <Ray/internal/PMJ.h>
#include <Ren/Span.h>

const int SampleCountPow = 8;
const int SampleCount = (1 << SampleCountPow);
const int TileRes = 128;
const int TestIntegralsCount = 1000;

float test_function(float x, float min, float max) {
    if (x > min && x < max) {
        return 1.0f;
    }
    return 0.0f;
}

float integral_val(float min, float max) { return max - min; }

float evaluate_integrals(const float samples[], const int sample_count) {
    double total_error = 0.0;
    for (int j = 0; j < TestIntegralsCount; ++j) {
        const float point = float(j) / float(TestIntegralsCount - 1);

        float results[2] = {};
        for (int k = 0; k < sample_count; ++k) {
            const float f = test_function(samples[k], 0.0f - 0.1f, point);
            // white region first, black region second
            results[0] += f;
            // black region first, white region second
            results[1] += 1.0f - f;
        }
        results[0] /= sample_count;
        results[1] /= sample_count;

        const float integral = integral_val(0.0f, point);
        total_error += std::abs(results[0] - integral) + std::abs(results[1] - 1.0f + integral);
    }
    return float(total_error / TestIntegralsCount);
}

float calc_pixel_proximity(const int ox, const int oy, const float values[TileRes][TileRes]) {
    const float v = values[oy][ox];

    // TODO: Is this really OK to limit the radius?
    const int Radius = 4;

    float total_proximity = 0.0f;
    for (int y = oy - Radius; y <= oy + Radius; ++y) {
        const int wrapped_y = (y + TileRes) % TileRes;
        for (int x = ox - Radius; x <= ox + Radius; ++x) {
            const int wrapped_x = (x + TileRes) % TileRes;
            if (wrapped_x == ox && wrapped_y == oy) {
                continue;
            }

            const float GaussOmega = 2.1f;

            total_proximity += std::exp(-((x - ox) * (x - ox) + (y - oy) * (y - oy)) / GaussOmega) *
                               (values[wrapped_y][wrapped_x] - v) * (values[wrapped_y][wrapped_x] - v);
        }
    }

    return total_proximity;
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

uint32_t hash_combine(uint32_t seed, uint32_t v) { return seed ^ (v + (seed << 6u) + (seed >> 2u)); }

void WriteTGA(const float *data, int pitch, int w, int h, int px_stride, int bpp, const char *name);

#undef main
int main(int argc, char *argv[]) {
#if 1
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(123456, SampleCount, 1);

    uint32_t initial_samples[SampleCount];
    for (int i = 0; i < SampleCount; ++i) {
        initial_samples[i] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
    }

    uint32_t scrambling_keys[TileRes][TileRes];
    for (int y = 0; y < TileRes; ++y) {
        for (int x = 0; x < TileRes; ++x) {
            scrambling_keys[y][x] = hash(y * TileRes + x) & 0xffffff00;
        }
    }

    { // try to load previous state
        std::ifstream in_file("scrambling_keys.bin", std::ios::binary | std::ios::ate);
        const size_t in_file_size = size_t(in_file.tellg());
        if (in_file_size == sizeof(scrambling_keys)) {
            in_file.seekg(0, std::ios::beg);
            in_file.read((char *)scrambling_keys, sizeof(scrambling_keys));
        }
    }

    std::vector<float> samples[TileRes][TileRes];
    for (int y = 0; y < TileRes; ++y) {
        for (int x = 0; x < TileRes; ++x) {
            samples[y][x].resize(SampleCount);
            for (int i = 0; i < SampleCount; ++i) {
                const uint32_t scrambled_val = initial_samples[i] ^ scrambling_keys[y][x];
                samples[y][x][i] = float(scrambled_val >> 8) / 16777216.0f;
            }
        }
    }

    static float errors[SampleCountPow + 1][TileRes][TileRes];
    for (int i = 0; i <= SampleCountPow; ++i) {
        float min_error = FLT_MAX, max_error = 0.0f;
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                errors[i][y][x] = evaluate_integrals(samples[y][x].data(), 1 << i);
                min_error = std::min(min_error, errors[i][y][x]);
                max_error = std::max(max_error, errors[i][y][x]);
            }
        }
        // normalize (for easier debugging)
        for (int j = 0; j < TileRes * TileRes; ++j) {
            float &e = errors[i][j / TileRes][j % TileRes];
            e = (e - min_error) / (max_error - min_error);
        }
    }

    const int AnnealingIterations = 10000000;

    float best_proximity = 0.0f, last_proximity = 0.0f;

    std::mt19937 gen(123456);
    std::uniform_int_distribution<int> uniform_index(0, TileRes * TileRes - 1);
    std::uniform_real_distribution<float> uniform_unorm_float(0.0f, 1.0f);

    float T = 1.0f, T_min = 0.00001f;
    for (int iter = 0; iter < AnnealingIterations; ++iter) {
        if ((iter % 1000) == 0) {
            printf("Annealing Iteration %i\n", iter);
        }

        // Randomly swap two scrambling keys
        int index1 = uniform_index(gen), index2;
        do {
            index2 = uniform_index(gen);
        } while (index1 == index2);

        std::swap(scrambling_keys[index1 / TileRes][index1 % TileRes],
                  scrambling_keys[index2 / TileRes][index2 % TileRes]);
        std::swap(samples[index1 / TileRes][index1 % TileRes], samples[index2 / TileRes][index2 % TileRes]);
        for (int i = 0; i <= SampleCountPow; ++i) {
            std::swap(errors[i][index1 / TileRes][index1 % TileRes], errors[i][index2 / TileRes][index2 % TileRes]);
        }

        float proximity[TileRes][TileRes];
        float total_proximity = 0.0f;
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
                const float prox = calc_pixel_proximity(x, y, errors[SampleCountPow]);
                proximity[y][x] = prox;
                total_proximity += prox;
            }
        }

        // We still accept worse permutation sometimes to avoid being stuck in local minimum
        const float acceptance_prob = std::exp((total_proximity - last_proximity) / T);
        if (total_proximity > last_proximity || uniform_unorm_float(gen) < acceptance_prob) {
            // Accept this iteration
            last_proximity = total_proximity;
            if (total_proximity > best_proximity) {
                best_proximity = total_proximity;
                printf("Best proximity = %f\n", best_proximity);

                // snprintf(temp_buf, sizeof(temp_buf), "best_samples_%i.tga", iter);
                // WriteTGA(&samples[0][0][0], TileRes, TileRes, TileRes, SampleCount, 3, "best_samples.tga");

                { // save current state
                    std::ofstream out_file("scrambling_keys.bin", std::ios::binary);
                    out_file.write((const char *)scrambling_keys, sizeof(scrambling_keys));
                }

                for (int i = 0; i <= SampleCountPow; ++i) {
                    char name_buf[128];
                    snprintf(name_buf, sizeof(name_buf), "best_errors_%i.tga", 1 << i);
                    WriteTGA(&errors[i][0][0], TileRes, TileRes, TileRes, 1, 3, name_buf);
                }
                WriteTGA(&proximity[0][0], TileRes, TileRes, TileRes, 1, 3, "best_proximity.tga");
            }
        } else {
            // Discard this iteration (swap values back)
            std::swap(scrambling_keys[index1 / TileRes][index1 % TileRes],
                      scrambling_keys[index2 / TileRes][index2 % TileRes]);
            std::swap(samples[index1 / TileRes][index1 % TileRes], samples[index2 / TileRes][index2 % TileRes]);
            for (int i = 0; i <= SampleCountPow; ++i) {
                std::swap(errors[i][index1 / TileRes][index1 % TileRes], errors[i][index2 / TileRes][index2 % TileRes]);
            }
        }

        T = std::max(T * 0.99f, T_min);
    }

    // At this point we have our scrambling keys optimized for SampleCount samples taken,
    // now we need to find ranking keys which make it work for any power of 2 samples subset
    uint32_t ranking_keys[TileRes][TileRes] = {};

    int samples_subset = SampleCount / 2;
    while (samples_subset) {
        for (int y = 0; y < TileRes; ++y) {
            for (int x = 0; x < TileRes; ++x) {
            }
        }
        samples_subset /= 2;
    }

    return 0;
#else
    return DummyApp().Run(argc, argv);
#endif
}

#define float_to_byte(val)                                                                                             \
    (((val) <= 0.0f) ? 0 : (((val) > (1.0f - 0.5f / 255.0f)) ? 255 : uint8_t((255.0f * (val)) + 0.5f)))

void WriteTGA(const float *data, int pitch, const int w, const int h, int px_stride, const int bpp, const char *name) {
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
