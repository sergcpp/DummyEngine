#include "DummyApp.h"

#include <fstream>

#include <Ren/Span.h>
#include <Ray/internal/PMJ.h>

double test_function(double x, double min, double max) {
    if (x > min && x < max) {
        return 1.0f;
    }
    return 0.0f;
}

double integral_val(double min, double max) { return max - min; }

double evaluate_integrals(const double samples[256]) {
    double total_error = 0.0;
    for (int i = 0; i < 256; ++i) {
        const double min_val = double(i) / 255.0;
        for (int j = i; j < 256; ++j) {
            const double max_val = double(j) / 255.0;

            double result = 0.0;
            for (int k = 0; k < 256; ++k) {
                result += test_function(samples[k], min_val, max_val);
            }
            result /= 256.0;

            total_error += std::abs(result - integral_val(min_val, max_val));
        }
    }
    return total_error;
}

void WriteTGA(const double *data, int pitch, const int w, const int h, const int bpp, const char *name);

#undef main
int main(int argc, char *argv[]) {
#if 1
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0, 256, 1);

    double samples[256];
    for (int i = 0; i < 256; ++i) {
        samples[i] = pmj_samples[i].get<0>();
    }

    double error = evaluate_integrals(samples);

    WriteTGA(samples, 256, 256, 256, 3, "test.tga");

    return 0;
#else
    return DummyApp().Run(argc, argv);
#endif
}

#define double_to_byte(val)                                                                                             \
    (((val) <= 0.0f) ? 0 : (((val) > (1.0f - 0.5f / 255.0f)) ? 255 : uint8_t((255.0f * (val)) + 0.5f)))

void WriteTGA(const double *data, int pitch, const int w, const int h, const int bpp, const char *name) {
    if (!pitch) {
        pitch = w;
    }

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
            out_data[(j * w + i) * bpp + 0] = double_to_byte(data[j * pitch + i]);
            out_data[(j * w + i) * bpp + 1] = double_to_byte(data[j * pitch + i]);
            out_data[(j * w + i) * bpp + 2] = double_to_byte(data[j * pitch + i]);
            if (bpp == 4) {
                out_data[i * 4 + 3] = double_to_byte(data[j * pitch + i]);
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
