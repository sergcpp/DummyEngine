#include "Utils.h"

#include <deque>

#include "CPUFeatures.h"
#include "Texture.h"

namespace Ren {
uint16_t f32_to_f16(const float value) {
    int32_t i;
    memcpy(&i, &value, sizeof(float));

    int32_t s = (i >> 16) & 0x00008000;
    int32_t e = ((i >> 23) & 0x000000ff) - (127 - 15);
    int32_t m = i & 0x007fffff;
    if (e <= 0) {
        if (e < -10) {
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }

        m = (m | 0x00800000) >> (1 - e);

        if (m & 0x00001000)
            m += 0x00002000;

        s = s | (m >> 13);
        uint16_t ret;
        memcpy(&ret, &s, sizeof(uint16_t));
        return ret;
    } else if (e == 0xff - (127 - 15)) {
        if (m == 0) {
            s = s | 0x7c00;
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        } else {
            m >>= 13;

            s = s | 0x7c00 | m | (m == 0);
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }
    } else {
        if (m & 0x00001000) {
            m += 0x00002000;

            if (m & 0x00800000) {
                m = 0;  // overflow in significand,
                e += 1; // adjust exponent
            }
        }

        if (e > 30) {
            s = s | 0x7c00;
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }

        s = s | (e << 10) | (m >> 13);
        uint16_t ret;
        memcpy(&ret, &s, sizeof(uint16_t));
        return ret;
    }
}

int16_t f32_to_s16(float value) { return int16_t(value * 32767); }

uint16_t f32_to_u16(float value) { return uint16_t(value * 65535); }

const uint8_t _blank_DXT5_block_4x4[] = {0x00, 0x00, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const int _blank_DXT5_block_4x4_len = sizeof(_blank_DXT5_block_4x4);

const uint8_t _blank_ASTC_block_4x4[] = {0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const int _blank_ASTC_block_4x4_len = sizeof(_blank_ASTC_block_4x4);

Ren::Vec4f permute(const Ren::Vec4f &x) {
    return Ren::Mod(((x * 34.0) + Ren::Vec4f{1.0}) * x, Ren::Vec4f{289.0});
}

Ren::Vec4f taylor_inv_sqrt(const Ren::Vec4f &r) {
    return Ren::Vec4f{1.79284291400159f} - 0.85373472095314f * r;
}

Ren::Vec4f fade(const Ren::Vec4f &t) {
    return t * t * t * (t * (t * 6.0f - Ren::Vec4f{15.0f}) + Ren::Vec4f{10.0f});
}
} // namespace Ren

#define _MIN(x, y) ((x) < (y) ? (x) : (y))
#define _MAX(x, y) ((x) < (y) ? (y) : (x))
#define _CLAMP(x, lo, hi) (_MIN(_MAX((x), (lo)), (hi)))

#define _MIN3(x, y, z) _MIN((x), _MIN((y), (z)))
#define _MAX3(x, y, z) _MAX((x), _MAX((y), (z)))

#define _MIN4(x, y, z, w) _MIN(_MIN((x), (y)), _MIN((z), (w)))
#define _MAX4(x, y, z, w) _MAX(_MAX((x), (y)), _MAX((z), (w)))

std::unique_ptr<uint8_t[]> Ren::ReadTGAFile(const void *data, int &w, int &h,
                                            eTexFormat &format) {
    const uint8_t tga_header[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto *tga_compare = (const uint8_t *)data;
    const uint8_t *img_header = (const uint8_t *)data + sizeof(tga_header);
    uint32_t img_size;
    bool compressed = false;

    if (memcmp(tga_header, tga_compare, sizeof(tga_header)) != 0) {
        if (tga_compare[2] == 1) {
            fprintf(stderr, "Image cannot be indexed color.");
        }
        if (tga_compare[2] == 3) {
            fprintf(stderr, "Image cannot be greyscale color.");
        }
        if (tga_compare[2] == 9 || tga_compare[2] == 10) {
            // fprintf(stderr, "Image cannot be compressed.");
            compressed = true;
        }
    }

    w = int(img_header[1] * 256u + img_header[0]);
    h = int(img_header[3] * 256u + img_header[2]);

    if (w <= 0 || h <= 0 || (img_header[4] != 24 && img_header[4] != 32)) {
        if (w <= 0 || h <= 0) {
            fprintf(stderr, "Image must have a width and height greater than 0");
        }
        if (img_header[4] != 24 && img_header[4] != 32) {
            fprintf(stderr, "Image must be 24 or 32 bit");
        }
        return nullptr;
    }

    const uint32_t bpp = img_header[4];
    const uint32_t bytes_per_pixel = bpp / 8;
    img_size = w * h * bytes_per_pixel;
    const uint8_t *image_data = (const uint8_t *)data + 18;

    std::unique_ptr<uint8_t[]> image_ret(new uint8_t[img_size]);
    uint8_t *_image_ret = &image_ret[0];

    if (!compressed) {
        for (unsigned i = 0; i < img_size; i += bytes_per_pixel) {
            _image_ret[i] = image_data[i + 2];
            _image_ret[i + 1] = image_data[i + 1];
            _image_ret[i + 2] = image_data[i];
            if (bytes_per_pixel == 4) {
                _image_ret[i + 3] = image_data[i + 3];
            }
        }
    } else {
        for (unsigned num = 0; num < img_size;) {
            uint8_t packet_header = *image_data++;
            if (packet_header & (1u << 7u)) {
                uint8_t color[4];
                unsigned size = (packet_header & ~(1u << 7u)) + 1;
                size *= bytes_per_pixel;
                for (unsigned i = 0; i < bytes_per_pixel; i++) {
                    color[i] = *image_data++;
                }
                for (unsigned i = 0; i < size;
                     i += bytes_per_pixel, num += bytes_per_pixel) {
                    _image_ret[num] = color[2];
                    _image_ret[num + 1] = color[1];
                    _image_ret[num + 2] = color[0];
                    if (bytes_per_pixel == 4) {
                        _image_ret[num + 3] = color[3];
                    }
                }
            } else {
                unsigned size = (packet_header & ~(1u << 7u)) + 1;
                size *= bytes_per_pixel;
                for (unsigned i = 0; i < size;
                     i += bytes_per_pixel, num += bytes_per_pixel) {
                    _image_ret[num] = image_data[i + 2];
                    _image_ret[num + 1] = image_data[i + 1];
                    _image_ret[num + 2] = image_data[i];
                    if (bytes_per_pixel == 4) {
                        _image_ret[num + 3] = image_data[i + 3];
                    }
                }
                image_data += size;
            }
        }
    }

    if (bpp == 32) {
        format = eTexFormat::RawRGBA8888;
    } else if (bpp == 24) {
        format = eTexFormat::RawRGB888;
    }

    return image_ret;
}

void Ren::RGBMDecode(const uint8_t rgbm[4], float out_rgb[3]) {
    out_rgb[0] = 4.0f * (rgbm[0] / 255.0f) * (rgbm[3] / 255.0f);
    out_rgb[1] = 4.0f * (rgbm[1] / 255.0f) * (rgbm[3] / 255.0f);
    out_rgb[2] = 4.0f * (rgbm[2] / 255.0f) * (rgbm[3] / 255.0f);
}

void Ren::RGBMEncode(const float rgb[3], uint8_t out_rgbm[4]) {
    float fr = rgb[0] / 4.0f;
    float fg = rgb[1] / 4.0f;
    float fb = rgb[2] / 4.0f;
    float fa = std::max(std::max(fr, fg), std::max(fb, 1e-6f));
    if (fa > 1.0f)
        fa = 1.0f;

    fa = std::ceil(fa * 255.0f) / 255.0f;
    fr /= fa;
    fg /= fa;
    fb /= fa;

    out_rgbm[0] = (uint8_t)_CLAMP(int(fr * 255), 0, 255);
    out_rgbm[1] = (uint8_t)_CLAMP(int(fg * 255), 0, 255);
    out_rgbm[2] = (uint8_t)_CLAMP(int(fb * 255), 0, 255);
    out_rgbm[3] = (uint8_t)_CLAMP(int(fa * 255), 0, 255);
}

std::unique_ptr<float[]> Ren::ConvertRGBE_to_RGB32F(const uint8_t *image_data,
                                                    const int w, const int h) {
    std::unique_ptr<float[]> fp_data(new float[w * h * 3]);

    for (int i = 0; i < w * h; i++) {
        const uint8_t r = image_data[4 * i + 0], g = image_data[4 * i + 1],
                      b = image_data[4 * i + 2], a = image_data[4 * i + 3];

        const float f = std::exp2(float(a) - 128.0f);
        const float k = 1.0f / 255;

        fp_data[3 * i + 0] = k * float(r) * f;
        fp_data[3 * i + 1] = k * float(g) * f;
        fp_data[3 * i + 2] = k * float(b) * f;
    }

    return fp_data;
}

std::unique_ptr<uint16_t[]> Ren::ConvertRGBE_to_RGB16F(const uint8_t *image_data,
                                                       const int w, const int h) {
    std::unique_ptr<uint16_t[]> fp16_data(new uint16_t[w * h * 3]);

    for (int i = 0; i < w * h; i++) {
        const uint8_t r = image_data[4 * i + 0], g = image_data[4 * i + 1],
                      b = image_data[4 * i + 2], a = image_data[4 * i + 3];

        const float f = std::exp2(float(a) - 128.0f);
        const float k = 1.0f / 255;

        fp16_data[3 * i + 0] = f32_to_f16(k * float(r) * f);
        fp16_data[3 * i + 1] = f32_to_f16(k * float(g) * f);
        fp16_data[3 * i + 2] = f32_to_f16(k * float(b) * f);
    }

    return fp16_data;
}

std::unique_ptr<uint8_t[]> Ren::ConvertRGB32F_to_RGBE(const float *image_data,
                                                      const int w, const int h,
                                                      const int channels) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 4]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            auto val = Vec3f{Uninitialize};

            if (channels == 3) {
                val[0] = image_data[3 * (y * w + x) + 0];
                val[1] = image_data[3 * (y * w + x) + 1];
                val[2] = image_data[3 * (y * w + x) + 2];
            } else if (channels == 4) {
                val[0] = image_data[4 * (y * w + x) + 0];
                val[1] = image_data[4 * (y * w + x) + 1];
                val[2] = image_data[4 * (y * w + x) + 2];
            }

            auto exp = Vec3f{std::log2(val[0]), std::log2(val[1]), std::log2(val[2])};
            for (int i = 0; i < 3; i++) {
                exp[i] = std::ceil(exp[i]);
                if (exp[i] < -128.0f) {
                    exp[i] = -128.0f;
                } else if (exp[i] > 127.0f) {
                    exp[i] = 127.0f;
                }
            }

            const float common_exp = std::max(exp[0], std::max(exp[1], exp[2]));
            const float range = std::exp2(common_exp);

            Ren::Vec3f mantissa = val / range;
            for (int i = 0; i < 3; i++) {
                if (mantissa[i] < 0.0f)
                    mantissa[i] = 0.0f;
                else if (mantissa[i] > 1.0f)
                    mantissa[i] = 1.0f;
            }

            const auto res =
                Ren::Vec4f{mantissa[0], mantissa[1], mantissa[2], common_exp + 128.0f};

            u8_data[(y * w + x) * 4 + 0] = (uint8_t)_CLAMP(int(res[0] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 1] = (uint8_t)_CLAMP(int(res[1] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 2] = (uint8_t)_CLAMP(int(res[2] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 3] = (uint8_t)_CLAMP(int(res[3]), 0, 255);
        }
    }

    return u8_data;
}

std::unique_ptr<uint8_t[]> Ren::ConvertRGB32F_to_RGBM(const float *image_data,
                                                      const int w, const int h,
                                                      const int channels) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 4]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            RGBMEncode(&image_data[channels * (y * w + x)], &u8_data[(y * w + x) * 4]);
        }
    }

    return u8_data;
}

int Ren::InitMipMaps(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16],
                     int heights[16], const int channels, const eMipOp op[4]) {
    int mip_count = 1;

    int _w = widths[0], _h = heights[0];
    while (_w > 1 || _h > 1) {
        int _prev_w = _w, _prev_h = _h;
        _w = std::max(_w / 2, 1);
        _h = std::max(_h / 2, 1);
        if (!mipmaps[mip_count]) {
            mipmaps[mip_count].reset(new uint8_t[_w * _h * channels]);
        }
        widths[mip_count] = _w;
        heights[mip_count] = _h;
        const uint8_t *tex = mipmaps[mip_count - 1].get();

        int count = 0;

        for (int j = 0; j < _prev_h; j += 2) {
            for (int i = 0; i < _prev_w; i += 2) {
                for (int k = 0; k < channels; k++) {
                    if (op[k] == eMipOp::Skip) {
                        continue;
                    } else if (op[k] == eMipOp::Zero) {
                        mipmaps[mip_count][count * channels + k] = 0;
                    }

                    // 4x4 pixel neighbourhood
                    int c[4][4];

                    // fetch inner quad
                    c[1][1] = tex[((j + 0) * _prev_w + i + 0) * channels + k];
                    c[1][2] = tex[((j + 0) * _prev_w + i + 1) * channels + k];
                    c[2][1] = tex[((j + 1) * _prev_w + i + 0) * channels + k];
                    c[2][2] = tex[((j + 1) * _prev_w + i + 1) * channels + k];

                    if (op[k] == eMipOp::Avg) {
                        mipmaps[mip_count][count * channels + k] =
                            uint8_t((c[1][1] + c[1][2] + c[2][1] + c[2][2]) / 4);
                    } else if (op[k] == eMipOp::Min) {
                        mipmaps[mip_count][count * channels + k] =
                            uint8_t(_MIN4(c[1][1], c[1][2], c[2][1], c[2][2]));
                    } else if (op[k] == eMipOp::Max) {
                        mipmaps[mip_count][count * channels + k] =
                            uint8_t(_MAX4(c[1][1], c[1][2], c[2][1], c[2][2]));
                    } else if (op[k] == eMipOp::MinBilinear ||
                               op[k] == eMipOp::MaxBilinear) {

                        // fetch outer quad
                        for (int dy = -1; dy < 3; dy++) {
                            for (int dx = -1; dx < 3; dx++) {
                                if ((dx == 0 || dx == 1) && (dy == 0 || dy == 1)) {
                                    continue;
                                }

                                const int i0 = (i + dx + _prev_w) % _prev_w;
                                const int j0 = (j + dy + _prev_h) % _prev_h;

                                c[dy + 1][dx + 1] =
                                    tex[(j0 * _prev_w + i0) * channels + k];
                            }
                        }

                        static const int quadrants[2][2][2] = {{{-1, -1}, {+1, -1}},
                                                               {{-1, +1}, {+1, +1}}};

                        int test_val = c[1][1];

                        for (int dj = 1; dj < 3; dj++) {
                            for (int di = 1; di < 3; di++) {
                                const int i0 = di + quadrants[dj - 1][di - 1][0];
                                const int j0 = dj + quadrants[dj - 1][di - 1][1];

                                if (op[k] == eMipOp::MinBilinear) {
                                    test_val =
                                        _MIN(test_val, (c[dj][di] + c[dj][i0]) / 2);
                                    test_val =
                                        _MIN(test_val, (c[dj][di] + c[j0][di]) / 2);
                                } else if (op[k] == eMipOp::MaxBilinear) {
                                    test_val =
                                        _MAX(test_val, (c[dj][di] + c[dj][i0]) / 2);
                                    test_val =
                                        _MAX(test_val, (c[dj][di] + c[j0][di]) / 2);
                                }
                            }
                        }

                        for (int dj = 0; dj < 3; dj++) {
                            for (int di = 0; di < 3; di++) {
                                if (di == 1 && dj == 1) {
                                    continue;
                                }

                                if (op[k] == eMipOp::MinBilinear) {
                                    test_val =
                                        _MIN(test_val,
                                             (c[dj + 0][di + 0] + c[dj + 0][di + 1] +
                                              c[dj + 1][di + 0] + c[dj + 1][di + 1]) /
                                                 4);
                                } else if (op[k] == eMipOp::MaxBilinear) {
                                    test_val =
                                        _MAX(test_val,
                                             (c[dj + 0][di + 0] + c[dj + 0][di + 1] +
                                              c[dj + 1][di + 0] + c[dj + 1][di + 1]) /
                                                 4);
                                }
                            }
                        }

                        c[1][1] = test_val;

                        if (op[k] == eMipOp::MinBilinear) {
                            mipmaps[mip_count][count * channels + k] =
                                uint8_t(_MIN4(c[1][1], c[1][2], c[2][1], c[2][2]));
                        } else if (op[k] == eMipOp::MaxBilinear) {
                            mipmaps[mip_count][count * channels + k] =
                                uint8_t(_MAX4(c[1][1], c[1][2], c[2][1], c[2][2]));
                        }
                    }
                }

                count++;
            }
        }

        mip_count++;
    }

    return mip_count;
}

int Ren::InitMipMapsRGBM(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16],
                         int heights[16]) {
    int mip_count = 1;

    int _w = widths[0], _h = heights[0];
    while (_w > 1 || _h > 1) {
        int _prev_w = _w, _prev_h = _h;
        _w = std::max(_w / 2, 1);
        _h = std::max(_h / 2, 1);
        mipmaps[mip_count].reset(new uint8_t[_w * _h * 4]);
        widths[mip_count] = _w;
        heights[mip_count] = _h;
        const uint8_t *tex = mipmaps[mip_count - 1].get();

        int count = 0;

        for (int j = 0; j < _prev_h; j += 2) {
            for (int i = 0; i < _prev_w; i += 2) {
                float rgb_sum[3];
                RGBMDecode(&tex[((j + 0) * _prev_w + i) * 4], rgb_sum);

                float temp[3];
                RGBMDecode(&tex[((j + 0) * _prev_w + i + 1) * 4], temp);
                rgb_sum[0] += temp[0];
                rgb_sum[1] += temp[1];
                rgb_sum[2] += temp[2];

                RGBMDecode(&tex[((j + 1) * _prev_w + i) * 4], temp);
                rgb_sum[0] += temp[0];
                rgb_sum[1] += temp[1];
                rgb_sum[2] += temp[2];

                RGBMDecode(&tex[((j + 1) * _prev_w + i + 1) * 4], temp);
                rgb_sum[0] += temp[0];
                rgb_sum[1] += temp[1];
                rgb_sum[2] += temp[2];

                rgb_sum[0] /= 4.0f;
                rgb_sum[1] /= 4.0f;
                rgb_sum[2] /= 4.0f;

                RGBMEncode(rgb_sum, &mipmaps[mip_count][count * 4]);
                count++;
            }
        }

        mip_count++;
    }

    return mip_count;
}

void Ren::ReorderTriangleIndices(const uint32_t *indices, const uint32_t indices_count,
                                 const uint32_t vtx_count, uint32_t *out_indices) {
    // From https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html

    uint32_t prim_count = indices_count / 3;

    struct vtx_data_t {
        int32_t cache_pos = -1;
        float score = 0.0f;
        uint32_t ref_count = 0;
        uint32_t active_tris_count = 0;
        std::unique_ptr<int32_t[]> tris;
    };

    const int MaxSizeVertexCache = 32;

    auto get_vertex_score = [MaxSizeVertexCache](int32_t cache_pos,
                                                 uint32_t active_tris_count) -> float {
        const float CacheDecayPower = 1.5f;
        const float LastTriScore = 0.75f;
        const float ValenceBoostScale = 2.0f;
        const float ValenceBoostPower = 0.5f;

        if (active_tris_count == 0) {
            // No tri needs this vertex!
            return -1.0f;
        }

        float score = 0.0f;

        if (cache_pos < 0) {
            // Vertex is not in FIFO cache - no score.
        } else if (cache_pos < 3) {
            // This vertex was used in the last triangle,
            // so it has a fixed score, whichever of the three
            // it's in. Otherwise, you can get very different
            // answers depending on whether you add
            // the triangle 1,2,3 or 3,1,2 - which is silly.
            score = LastTriScore;
        } else {
            assert(cache_pos < MaxSizeVertexCache);
            // Points for being high in the cache.
            const float scaler = 1.0f / (MaxSizeVertexCache - 3);
            score = 1.0f - float(cache_pos - 3) * scaler;
            score = std::pow(score, CacheDecayPower);
        }

        // Bonus points for having a low number of tris still to
        // use the vert, so we get rid of lone verts quickly.

        const float valence_boost =
            std::pow((float)active_tris_count, -ValenceBoostPower);
        score += ValenceBoostScale * valence_boost;
        return score;
    };

    struct tri_data_t {
        bool is_in_list = false;
        float score = 0.0f;
        uint32_t indices[3] = {};
    };

    std::unique_ptr<vtx_data_t[]> _vertices(new vtx_data_t[vtx_count]);
    std::unique_ptr<tri_data_t[]> _triangles(new tri_data_t[prim_count]);

    // avoid operator[] call overhead in debug
    vtx_data_t *vertices = _vertices.get();
    tri_data_t *triangles = _triangles.get();

    for (uint32_t i = 0; i < indices_count; i += 3) {
        tri_data_t &tri = triangles[i / 3];

        tri.indices[0] = indices[i + 0];
        tri.indices[1] = indices[i + 1];
        tri.indices[2] = indices[i + 2];

        vertices[indices[i + 0]].active_tris_count++;
        vertices[indices[i + 1]].active_tris_count++;
        vertices[indices[i + 2]].active_tris_count++;
    }

    for (uint32_t i = 0; i < vtx_count; i++) {
        vtx_data_t &v = vertices[i];
        v.tris.reset(new int32_t[v.active_tris_count]);
        v.score = get_vertex_score(v.cache_pos, v.active_tris_count);
    }

    int32_t next_best_index = -1, next_next_best_index = -1;
    float next_best_score = -1.0f, next_next_best_score = -1.0f;

    for (uint32_t i = 0; i < indices_count; i += 3) {
        tri_data_t &tri = triangles[i / 3];

        vtx_data_t &v0 = vertices[indices[i + 0]];
        vtx_data_t &v1 = vertices[indices[i + 1]];
        vtx_data_t &v2 = vertices[indices[i + 2]];

        v0.tris[v0.ref_count++] = i / 3;
        v1.tris[v1.ref_count++] = i / 3;
        v2.tris[v2.ref_count++] = i / 3;

        tri.score = v0.score + v1.score + v2.score;

        if (tri.score > next_best_score) {
            if (next_best_score > next_next_best_score) {
                next_next_best_index = next_best_index;
                next_next_best_score = next_best_score;
            }
            next_best_index = i / 3;
            next_best_score = tri.score;
        }

        if (tri.score > next_next_best_score) {
            next_next_best_index = i / 3;
            next_next_best_score = tri.score;
        }
    }

    std::deque<uint32_t> lru_cache;

    auto use_vertex = [](std::deque<uint32_t> &lru_cache, uint32_t vtx_index) {
        auto it = std::find(std::begin(lru_cache), std::end(lru_cache), vtx_index);

        if (it == std::end(lru_cache)) {
            lru_cache.push_back(vtx_index);
            it = std::begin(lru_cache);
        }

        if (it != std::begin(lru_cache)) {
            lru_cache.erase(it);
            lru_cache.push_front(vtx_index);
        }
    };

    auto enforce_size = [&get_vertex_score](std::deque<uint32_t> &lru_cache,
                                            vtx_data_t *vertices, uint32_t max_size,
                                            std::vector<uint32_t> &out_tris_to_update) {
        out_tris_to_update.clear();

        if (lru_cache.size() > max_size) {
            lru_cache.resize(max_size);
        }

        for (size_t i = 0; i < lru_cache.size(); i++) {
            vtx_data_t &v = vertices[lru_cache[i]];

            v.cache_pos = (int32_t)i;
            v.score = get_vertex_score(v.cache_pos, v.active_tris_count);

            for (uint32_t j = 0; j < v.ref_count; j++) {
                int tri_index = v.tris[j];
                if (tri_index != -1) {
                    auto it = std::find(std::begin(out_tris_to_update),
                                        std::end(out_tris_to_update), tri_index);
                    if (it == std::end(out_tris_to_update)) {
                        out_tris_to_update.push_back(tri_index);
                    }
                }
            }
        }
    };

    for (int32_t out_index = 0; out_index < (int32_t)indices_count;) {
        if (next_best_index < 0) {
            next_best_score = next_next_best_score = -1.0f;
            next_best_index = next_next_best_index = -1;

            for (int32_t i = 0; i < (int32_t)prim_count; i++) {
                const tri_data_t &tri = triangles[i];
                if (!tri.is_in_list) {
                    if (tri.score > next_best_score) {
                        if (next_best_score > next_next_best_score) {
                            next_next_best_index = next_best_index;
                            next_next_best_score = next_best_score;
                        }
                        next_best_index = i;
                        next_best_score = tri.score;
                    }

                    if (tri.score > next_next_best_score) {
                        next_next_best_index = i;
                        next_next_best_score = tri.score;
                    }
                }
            }
        }

        tri_data_t &next_best_tri = triangles[next_best_index];

        for (unsigned int indice : next_best_tri.indices) {
            out_indices[out_index++] = indice;

            vtx_data_t &v = vertices[indice];
            v.active_tris_count--;
            for (uint32_t k = 0; k < v.ref_count; k++) {
                if (v.tris[k] == next_best_index) {
                    v.tris[k] = -1;
                    break;
                }
            }

            use_vertex(lru_cache, indice);
        }

        next_best_tri.is_in_list = true;

        std::vector<uint32_t> tris_to_update;
        enforce_size(lru_cache, &vertices[0], MaxSizeVertexCache, tris_to_update);

        next_best_score = -1.0f;
        next_best_index = -1;

        for (const uint32_t ti : tris_to_update) {
            tri_data_t &tri = triangles[ti];

            if (!tri.is_in_list) {
                tri.score = vertices[tri.indices[0]].score +
                            vertices[tri.indices[1]].score +
                            vertices[tri.indices[2]].score;

                if (tri.score > next_best_score) {
                    if (next_best_score > next_next_best_score) {
                        next_next_best_index = next_best_index;
                        next_next_best_score = next_best_score;
                    }
                    next_best_index = ti;
                    next_best_score = tri.score;
                }

                if (tri.score > next_next_best_score) {
                    next_next_best_index = ti;
                    next_next_best_score = tri.score;
                }
            }
        }

        if (next_best_index == -1 && next_next_best_index != -1) {
            if (!triangles[next_next_best_index].is_in_list) {
                next_best_index = next_next_best_index;
                next_best_score = next_next_best_score;
            }

            next_next_best_index = -1;
            next_next_best_score = -1.0f;
        }
    }
}

void Ren::ComputeTextureBasis(std::vector<vertex_t> &vertices,
                              std::vector<uint32_t> index_groups[],
                              const int groups_count) {
    const float flt_eps = 0.0000001f;

    std::vector<std::array<uint32_t, 3>> twin_verts(vertices.size(), {0, 0, 0});
    std::vector<Vec3f> binormals(vertices.size());
    for (int grp = 0; grp < groups_count; grp++) {
        std::vector<uint32_t> &indices = index_groups[grp];
        for (size_t i = 0; i < indices.size(); i += 3) {
            vertex_t *v0 = &vertices[indices[i + 0]];
            vertex_t *v1 = &vertices[indices[i + 1]];
            vertex_t *v2 = &vertices[indices[i + 2]];

            Vec3f &b0 = binormals[indices[i + 0]];
            Vec3f &b1 = binormals[indices[i + 1]];
            Vec3f &b2 = binormals[indices[i + 2]];

            const Vec3f dp1 = MakeVec3(v1->p) - MakeVec3(v0->p);
            const Vec3f dp2 = MakeVec3(v2->p) - MakeVec3(v0->p);

            const Vec2f dt1 = MakeVec2(v1->t[0]) - MakeVec2(v0->t[0]);
            const Vec2f dt2 = MakeVec2(v2->t[0]) - MakeVec2(v0->t[0]);

            Vec3f tangent, binormal;

            const float det = dt1[0] * dt2[1] - dt1[1] * dt2[0];
            if (std::abs(det) > flt_eps) {
                const float inv_det = 1.0f / det;
                tangent = (dp1 * dt2[1] - dp2 * dt1[1]) * inv_det;
                binormal = (dp2 * dt1[0] - dp1 * dt2[0]) * inv_det;
            } else {
                const Vec3f plane_N = Cross(dp1, dp2);
                tangent = Vec3f{0.0f, 1.0f, 0.0f};
                if (std::abs(plane_N[0]) <= std::abs(plane_N[1]) &&
                    std::abs(plane_N[0]) <= std::abs(plane_N[2])) {
                    tangent = Vec3f{1.0f, 0.0f, 0.0f};
                } else if (std::abs(plane_N[2]) <= std::abs(plane_N[0]) &&
                           std::abs(plane_N[2]) <= std::abs(plane_N[1])) {
                    tangent = Vec3f{0.0f, 0.0f, 1.0f};
                }

                binormal = Normalize(Cross(Vec3f(plane_N), tangent));
                tangent = Normalize(Cross(Vec3f(plane_N), binormal));
            }

            int i1 = (v0->b[0] * tangent[0] + v0->b[1] * tangent[1] +
                      v0->b[2] * tangent[2]) < 0;
            int i2 =
                2 * (b0[0] * binormal[0] + b0[1] * binormal[1] + b0[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 0]][i1 + i2 - 1];
                if (index == 0) {
                    index = (uint32_t)(vertices.size());
                    vertices.push_back(*v0);
                    memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                    twin_verts[indices[i + 0]][i1 + i2 - 1] = index;

                    v1 = &vertices[indices[i + 1]];
                    v2 = &vertices[indices[i + 2]];
                }
                indices[i] = index;
                v0 = &vertices[index];
            } else {
                b0 = binormal;
            }

            v0->b[0] += tangent[0];
            v0->b[1] += tangent[1];
            v0->b[2] += tangent[2];

            i1 =
                v1->b[0] * tangent[0] + v1->b[1] * tangent[1] + v1->b[2] * tangent[2] < 0;
            i2 =
                2 * (b1[0] * binormal[0] + b1[1] * binormal[1] + b1[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 1]][i1 + i2 - 1];
                if (index == 0) {
                    index = (uint32_t)(vertices.size());
                    vertices.push_back(*v1);
                    memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                    twin_verts[indices[i + 1]][i1 + i2 - 1] = index;

                    v0 = &vertices[indices[i + 0]];
                    v2 = &vertices[indices[i + 2]];
                }
                indices[i + 1] = index;
                v1 = &vertices[index];
            } else {
                b1 = binormal;
            }

            v1->b[0] += tangent[0];
            v1->b[1] += tangent[1];
            v1->b[2] += tangent[2];

            i1 =
                v2->b[0] * tangent[0] + v2->b[1] * tangent[1] + v2->b[2] * tangent[2] < 0;
            i2 =
                2 * (b2[0] * binormal[0] + b2[1] * binormal[1] + b2[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 2]][i1 + i2 - 1];
                if (index == 0) {
                    index = (uint32_t)(vertices.size());
                    vertices.push_back(*v2);
                    memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                    twin_verts[indices[i + 2]][i1 + i2 - 1] = index;

                    v0 = &vertices[indices[i + 0]];
                    v1 = &vertices[indices[i + 1]];
                }
                indices[i + 2] = index;
                v2 = &vertices[index];
            } else {
                b2 = binormal;
            }

            v2->b[0] += tangent[0];
            v2->b[1] += tangent[1];
            v2->b[2] += tangent[2];
        }
    }

    for (vertex_t &v : vertices) {
        if (std::abs(v.b[0]) > flt_eps || std::abs(v.b[1]) > flt_eps ||
            std::abs(v.b[2]) > flt_eps) {
            const Vec3f tangent = MakeVec3(v.b);
            Vec3f binormal = Cross(MakeVec3(v.n), tangent);
            float l = Length(binormal);
            if (l > flt_eps) {
                binormal /= l;
                memcpy(&v.b[0], &binormal[0], 3 * sizeof(float));
            }
        }
    }
}

//	Classic Perlin 3D Noise
//	by Stefan Gustavson
//
float Ren::PerlinNoise(const Ren::Vec4f &P) {
    Vec4f Pi0 = Floor(P);          // Integer part for indexing
    Vec4f Pi1 = Pi0 + Vec4f{1.0f}; // Integer part + 1
    Pi0 = Mod(Pi0, Vec4f{289.0f});
    Pi1 = Mod(Pi1, Vec4f{289.0f});
    const Vec4f Pf0 = Fract(P);         // Fractional part for interpolation
    const Vec4f Pf1 = Pf0 - Vec4f{1.0}; // Fractional part - 1.0
    const auto ix = Vec4f{Pi0[0], Pi1[0], Pi0[0], Pi1[0]};
    const auto iy = Vec4f{Pi0[1], Pi0[1], Pi1[1], Pi1[1]};
    const auto iz0 = Vec4f{Pi0[2]};
    const auto iz1 = Vec4f{Pi1[2]};
    const auto iw0 = Vec4f{Pi0[3]};
    const auto iw1 = Vec4f{Pi1[3]};

    const Vec4f ixy = permute(permute(ix) + iy);
    const Vec4f ixy0 = permute(ixy + iz0);
    const Vec4f ixy1 = permute(ixy + iz1);
    const Vec4f ixy00 = permute(ixy0 + iw0);
    const Vec4f ixy01 = permute(ixy0 + iw1);
    const Vec4f ixy10 = permute(ixy1 + iw0);
    const Vec4f ixy11 = permute(ixy1 + iw1);

    Vec4f gx00 = ixy00 / 7.0f;
    Vec4f gy00 = Floor(gx00) / 7.0;
    Vec4f gz00 = Floor(gy00) / 6.0;
    gx00 = Fract(gx00) - Vec4f{0.5f};
    gy00 = Fract(gy00) - Vec4f{0.5f};
    gz00 = Fract(gz00) - Vec4f{0.5f};
    Vec4f gw00 = Vec4f{0.75} - Abs(gx00) - Abs(gy00) - Abs(gz00);
    Vec4f sw00 = Step(gw00, Vec4f{0.0f});
    gx00 -= sw00 * (Step(Vec4f{0.0f}, gx00) - Vec4f{0.5f});
    gy00 -= sw00 * (Step(Vec4f{0.0f}, gy00) - Vec4f{0.5f});

    Vec4f gx01 = ixy01 / 7.0f;
    Vec4f gy01 = Floor(gx01) / 7.0f;
    Vec4f gz01 = Floor(gy01) / 6.0f;
    gx01 = Fract(gx01) - Vec4f{0.5};
    gy01 = Fract(gy01) - Vec4f{0.5};
    gz01 = Fract(gz01) - Vec4f{0.5};
    Vec4f gw01 = Vec4f{0.75f} - Abs(gx01) - Abs(gy01) - Abs(gz01);
    Vec4f sw01 = Step(gw01, Vec4f{0.0f});
    gx01 -= sw01 * (Step(Vec4f{0.0f}, gx01) - Vec4f{0.5f});
    gy01 -= sw01 * (Step(Vec4f{0.0f}, gy01) - Vec4f{0.5f});

    Vec4f gx10 = ixy10 / 7.0f;
    Vec4f gy10 = Floor(gx10) / 7.0f;
    Vec4f gz10 = Floor(gy10) / 6.0f;
    gx10 = Fract(gx10) - Vec4f{0.5};
    gy10 = Fract(gy10) - Vec4f{0.5};
    gz10 = Fract(gz10) - Vec4f{0.5};
    Vec4f gw10 = Vec4f{0.75f} - Abs(gx10) - Abs(gy10) - Abs(gz10);
    Vec4f sw10 = Step(gw10, Vec4f{0.0f});
    gx10 -= sw10 * (Step(Vec4f{0.0}, gx10) - Vec4f{0.5});
    gy10 -= sw10 * (Step(Vec4f{0.0}, gy10) - Vec4f{0.5});

    Vec4f gx11 = ixy11 / 7.0f;
    Vec4f gy11 = Floor(gx11) / 7.0f;
    Vec4f gz11 = Floor(gy11) / 6.0f;
    gx11 = Fract(gx11) - Vec4f{0.5f};
    gy11 = Fract(gy11) - Vec4f{0.5f};
    gz11 = Fract(gz11) - Vec4f{0.5f};
    Vec4f gw11 = Vec4f{0.75f} - Abs(gx11) - Abs(gy11) - Abs(gz11);
    Vec4f sw11 = Step(gw11, Vec4f{0.0f});
    gx11 -= sw11 * (Step(Vec4f{0.0f}, gx11) - Vec4f{0.5f});
    gy11 -= sw11 * (Step(Vec4f{0.0f}, gy11) - Vec4f{0.5f});

    auto g0000 = Vec4f{gx00[0], gy00[0], gz00[0], gw00[0]};
    auto g1000 = Vec4f{gx00[1], gy00[1], gz00[1], gw00[1]};
    auto g0100 = Vec4f{gx00[2], gy00[2], gz00[2], gw00[2]};
    auto g1100 = Vec4f{gx00[3], gy00[3], gz00[3], gw00[3]};
    auto g0010 = Vec4f{gx10[0], gy10[0], gz10[0], gw10[0]};
    auto g1010 = Vec4f{gx10[1], gy10[1], gz10[1], gw10[1]};
    auto g0110 = Vec4f{gx10[2], gy10[2], gz10[2], gw10[2]};
    auto g1110 = Vec4f{gx10[3], gy10[3], gz10[3], gw10[3]};
    auto g0001 = Vec4f{gx01[0], gy01[0], gz01[0], gw01[0]};
    auto g1001 = Vec4f{gx01[1], gy01[1], gz01[1], gw01[1]};
    auto g0101 = Vec4f{gx01[2], gy01[2], gz01[2], gw01[2]};
    auto g1101 = Vec4f{gx01[3], gy01[3], gz01[3], gw01[3]};
    auto g0011 = Vec4f{gx11[0], gy11[0], gz11[0], gw11[0]};
    auto g1011 = Vec4f{gx11[1], gy11[1], gz11[1], gw11[1]};
    auto g0111 = Vec4f{gx11[2], gy11[2], gz11[2], gw11[2]};
    auto g1111 = Vec4f{gx11[3], gy11[3], gz11[3], gw11[3]};

    const Vec4f norm00 = taylor_inv_sqrt(Vec4f{Dot(g0000, g0000), Dot(g0100, g0100),
                                               Dot(g1000, g1000), Dot(g1100, g1100)});
    g0000 *= norm00[0];
    g0100 *= norm00[1];
    g1000 *= norm00[2];
    g1100 *= norm00[3];

    const Vec4f norm01 = taylor_inv_sqrt(Vec4f{Dot(g0001, g0001), Dot(g0101, g0101),
                                               Dot(g1001, g1001), Dot(g1101, g1101)});
    g0001 *= norm01[0];
    g0101 *= norm01[1];
    g1001 *= norm01[2];
    g1101 *= norm01[3];

    const Vec4f norm10 = taylor_inv_sqrt(Vec4f{Dot(g0010, g0010), Dot(g0110, g0110),
                                               Dot(g1010, g1010), Dot(g1110, g1110)});
    g0010 *= norm10[0];
    g0110 *= norm10[1];
    g1010 *= norm10[2];
    g1110 *= norm10[3];

    const Vec4f norm11 = taylor_inv_sqrt(Vec4f{Dot(g0011, g0011), Dot(g0111, g0111),
                                               Dot(g1011, g1011), Dot(g1111, g1111)});
    g0011 *= norm11[0];
    g0111 *= norm11[1];
    g1011 *= norm11[2];
    g1111 *= norm11[3];

    const float n0000 = Dot(g0000, Pf0);
    const float n1000 = Dot(g1000, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf0[3]});
    const float n0100 = Dot(g0100, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n1100 = Dot(g1100, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n0010 = Dot(g0010, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n1010 = Dot(g1010, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n0110 = Dot(g0110, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n1110 = Dot(g1110, Vec4f{Pf1[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n0001 = Dot(g0001, Vec4f{Pf0[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n1001 = Dot(g1001, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n0101 = Dot(g0101, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n1101 = Dot(g1101, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n0011 = Dot(g0011, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n1011 = Dot(g1011, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n0111 = Dot(g0111, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf1[3]});
    const float n1111 = Dot(g1111, Pf1);

    const Vec4f fade_xyzw = fade(Pf0);
    const Vec4f n_0w = Mix(Vec4f{n0000, n1000, n0100, n1100},
                           Vec4f{n0001, n1001, n0101, n1101}, fade_xyzw[3]);
    const Vec4f n_1w = Mix(Vec4f{n0010, n1010, n0110, n1110},
                           Vec4f{n0011, n1011, n0111, n1111}, fade_xyzw[3]);
    const Vec4f n_zw = Mix(n_0w, n_1w, fade_xyzw[2]);
    const Vec2f n_yzw =
        Mix(Vec2f{n_zw[0], n_zw[1]}, Vec2f{n_zw[2], n_zw[3]}, fade_xyzw[1]);
    const float n_xyzw = Mix(n_yzw[0], n_yzw[1], fade_xyzw[0]);
    return 2.2f * n_xyzw;
}

// Classic Perlin noise, periodic version
float Ren::PerlinNoise(const Ren::Vec4f &P, const Ren::Vec4f &rep) {
    const Vec4f Pi0 = Mod(Floor(P), rep);          // Integer part modulo rep
    const Vec4f Pi1 = Mod(Pi0 + Vec4f{1.0f}, rep); // Integer part + 1 mod rep
    const Vec4f Pf0 = Fract(P);                    // Fractional part for interpolation
    const Vec4f Pf1 = Pf0 - Vec4f{1.0f};           // Fractional part - 1.0
    const Vec4f ix = Vec4f{Pi0[0], Pi1[0], Pi0[0], Pi1[0]};
    const Vec4f iy = Vec4f{Pi0[1], Pi0[1], Pi1[1], Pi1[1]};
    const Vec4f iz0 = Vec4f{Pi0[2]};
    const Vec4f iz1 = Vec4f{Pi1[2]};
    const Vec4f iw0 = Vec4f{Pi0[3]};
    const Vec4f iw1 = Vec4f{Pi1[3]};

    const Vec4f ixy = permute(permute(ix) + iy);
    const Vec4f ixy0 = permute(ixy + iz0);
    const Vec4f ixy1 = permute(ixy + iz1);
    const Vec4f ixy00 = permute(ixy0 + iw0);
    const Vec4f ixy01 = permute(ixy0 + iw1);
    const Vec4f ixy10 = permute(ixy1 + iw0);
    const Vec4f ixy11 = permute(ixy1 + iw1);

    Vec4f gx00 = ixy00 / 7.0f;
    Vec4f gy00 = Floor(gx00) / 7.0f;
    Vec4f gz00 = Floor(gy00) / 6.0f;
    gx00 = Fract(gx00) - Vec4f{0.5f};
    gy00 = Fract(gy00) - Vec4f{0.5f};
    gz00 = Fract(gz00) - Vec4f{0.5f};
    Vec4f gw00 = Vec4f{0.75f} - Abs(gx00) - Abs(gy00) - Abs(gz00);
    Vec4f sw00 = Step(gw00, Vec4f{0.0f});
    gx00 -= sw00 * (Step(Vec4f{0.0f}, gx00) - Vec4f{0.5f});
    gy00 -= sw00 * (Step(Vec4f{0.0f}, gy00) - Vec4f{0.5f});

    Vec4f gx01 = ixy01 / 7.0f;
    Vec4f gy01 = Floor(gx01) / 7.0f;
    Vec4f gz01 = Floor(gy01) / 6.0f;
    gx01 = Fract(gx01) - Vec4f{0.5f};
    gy01 = Fract(gy01) - Vec4f{0.5f};
    gz01 = Fract(gz01) - Vec4f{0.5f};
    Vec4f gw01 = Vec4f{0.75f} - Abs(gx01) - Abs(gy01) - Abs(gz01);
    Vec4f sw01 = Step(gw01, Vec4f{0.0f});
    gx01 -= sw01 * (Step(Vec4f{0.0f}, gx01) - Vec4f{0.5f});
    gy01 -= sw01 * (Step(Vec4f{0.0f}, gy01) - Vec4f{0.5f});

    Vec4f gx10 = ixy10 / 7.0f;
    Vec4f gy10 = Floor(gx10) / 7.0f;
    Vec4f gz10 = Floor(gy10) / 6.0f;
    gx10 = Fract(gx10) - Vec4f{0.5f};
    gy10 = Fract(gy10) - Vec4f{0.5f};
    gz10 = Fract(gz10) - Vec4f{0.5f};
    Vec4f gw10 = Vec4f{0.75f} - Abs(gx10) - Abs(gy10) - Abs(gz10);
    Vec4f sw10 = Step(gw10, Vec4f{0.0f});
    gx10 -= sw10 * (Step(Vec4f{0.0f}, gx10) - Vec4f{0.5});
    gy10 -= sw10 * (Step(Vec4f{0.0f}, gy10) - Vec4f{0.5});

    Vec4f gx11 = ixy11 / 7.0f;
    Vec4f gy11 = Floor(gx11) / 7.0f;
    Vec4f gz11 = Floor(gy11) / 6.0f;
    gx11 = Fract(gx11) - Vec4f{0.5f};
    gy11 = Fract(gy11) - Vec4f{0.5f};
    gz11 = Fract(gz11) - Vec4f{0.5f};
    Vec4f gw11 = Vec4f{0.75f} - Abs(gx11) - Abs(gy11) - Abs(gz11);
    Vec4f sw11 = Step(gw11, Vec4f{0.0f});
    gx11 -= sw11 * (Step(Vec4f{0.0f}, gx11) - Vec4f{0.5f});
    gy11 -= sw11 * (Step(Vec4f{0.0f}, gy11) - Vec4f{0.5f});

    auto g0000 = Vec4f(gx00[0], gy00[0], gz00[0], gw00[0]);
    auto g1000 = Vec4f(gx00[1], gy00[1], gz00[1], gw00[1]);
    auto g0100 = Vec4f(gx00[2], gy00[2], gz00[2], gw00[2]);
    auto g1100 = Vec4f(gx00[3], gy00[3], gz00[3], gw00[3]);
    auto g0010 = Vec4f(gx10[0], gy10[0], gz10[0], gw10[0]);
    auto g1010 = Vec4f(gx10[1], gy10[1], gz10[1], gw10[1]);
    auto g0110 = Vec4f(gx10[2], gy10[2], gz10[2], gw10[2]);
    auto g1110 = Vec4f(gx10[3], gy10[3], gz10[3], gw10[3]);
    auto g0001 = Vec4f(gx01[0], gy01[0], gz01[0], gw01[0]);
    auto g1001 = Vec4f(gx01[1], gy01[1], gz01[1], gw01[1]);
    auto g0101 = Vec4f(gx01[2], gy01[2], gz01[2], gw01[2]);
    auto g1101 = Vec4f(gx01[3], gy01[3], gz01[3], gw01[3]);
    auto g0011 = Vec4f(gx11[0], gy11[0], gz11[0], gw11[0]);
    auto g1011 = Vec4f(gx11[1], gy11[1], gz11[1], gw11[1]);
    auto g0111 = Vec4f(gx11[2], gy11[2], gz11[2], gw11[2]);
    auto g1111 = Vec4f(gx11[3], gy11[3], gz11[3], gw11[3]);

    const Vec4f norm00 = taylor_inv_sqrt(Vec4f{Dot(g0000, g0000), Dot(g0100, g0100),
                                               Dot(g1000, g1000), Dot(g1100, g1100)});
    g0000 *= norm00[0];
    g0100 *= norm00[1];
    g1000 *= norm00[2];
    g1100 *= norm00[3];

    const Vec4f norm01 = taylor_inv_sqrt(Vec4f{Dot(g0001, g0001), Dot(g0101, g0101),
                                               Dot(g1001, g1001), Dot(g1101, g1101)});
    g0001 *= norm01[0];
    g0101 *= norm01[1];
    g1001 *= norm01[2];
    g1101 *= norm01[3];

    const Vec4f norm10 = taylor_inv_sqrt(Vec4f{Dot(g0010, g0010), Dot(g0110, g0110),
                                               Dot(g1010, g1010), Dot(g1110, g1110)});
    g0010 *= norm10[0];
    g0110 *= norm10[1];
    g1010 *= norm10[2];
    g1110 *= norm10[3];

    const Vec4f norm11 = taylor_inv_sqrt(Vec4f{Dot(g0011, g0011), Dot(g0111, g0111),
                                               Dot(g1011, g1011), Dot(g1111, g1111)});
    g0011 *= norm11[0];
    g0111 *= norm11[1];
    g1011 *= norm11[2];
    g1111 *= norm11[3];

    const float n0000 = Dot(g0000, Pf0);
    const float n1000 = Dot(g1000, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf0[3]});
    const float n0100 = Dot(g0100, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n1100 = Dot(g1100, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n0010 = Dot(g0010, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n1010 = Dot(g1010, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n0110 = Dot(g0110, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n1110 = Dot(g1110, Vec4f{Pf1[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n0001 = Dot(g0001, Vec4f{Pf0[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n1001 = Dot(g1001, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n0101 = Dot(g0101, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n1101 = Dot(g1101, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n0011 = Dot(g0011, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n1011 = Dot(g1011, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n0111 = Dot(g0111, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf1[3]});
    const float n1111 = Dot(g1111, Pf1);

    const Vec4f fade_xyzw = fade(Pf0);
    const Vec4f n_0w = Mix(Vec4f{n0000, n1000, n0100, n1100},
                           Vec4f{n0001, n1001, n0101, n1101}, fade_xyzw[3]);
    const Vec4f n_1w = Mix(Vec4f{n0010, n1010, n0110, n1110},
                           Vec4f{n0011, n1011, n0111, n1111}, fade_xyzw[3]);
    const Vec4f n_zw = Mix(n_0w, n_1w, fade_xyzw[2]);
    const Vec2f n_yzw =
        Mix(Vec2f{n_zw[0], n_zw[1]}, Vec2f{n_zw[2], n_zw[3]}, fade_xyzw[1]);
    const float n_xyzw = Mix(n_yzw[0], n_yzw[1], fade_xyzw[0]);
    return 2.2f * n_xyzw;
}

#undef _MIN
#undef _MAX