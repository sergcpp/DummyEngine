#include "SceneManager.h"

#include <fstream>

#include <Ren/Utils.h>
#include <Sys/AssetFile.h>

namespace SceneManagerInternal {
void WriteTGA(const std::vector<uint8_t> &out_data, int w, int h, const std::string &name) {
    int bpp = 4;

    std::ofstream file(name, std::ios::binary);

    unsigned char header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    header[12] = w & 0xFF;
    header[13] = (w >> 8) & 0xFF;
    header[14] = (h) & 0xFF;
    header[15] = (h >> 8) & 0xFF;
    header[16] = bpp * 8;

    file.write((char *)&header[0], sizeof(unsigned char) * 18);
    file.write((const char *)&out_data[0], w * h * bpp);

    static const char footer[26] = "\0\0\0\0" // no extension area
                                   "\0\0\0\0"// no developer directory
                                   "TRUEVISION-XFILE"// yep, this is a TGA file
                                   ".";
    file.write((const char *)&footer, sizeof(footer));
}

void WriteTGA(const std::vector<Ray::pixel_color_t> &out_data, int w, int h, const std::string &name) {
    int bpp = 4;

    std::ofstream file(name, std::ios::binary);

    unsigned char header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    header[12] = w & 0xFF;
    header[13] = (w >> 8) & 0xFF;
    header[14] = (h) & 0xFF;
    header[15] = (h >> 8) & 0xFF;
    header[16] = bpp * 8;

    file.write((char *)&header[0], sizeof(unsigned char) * 18);
    //file.write((const char *)&out_data[0], w * h * bpp);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const auto &p = out_data[y * w + x];

            Ren::Vec3f val = { p.r, p.g, p.b };

            Ren::Vec3f exp = { std::log2(val[0]), std::log2(val[1]), std::log2(val[2]) };
            for (int i = 0; i < 3; i++) {
                exp[i] = std::ceil(exp[i]);
                if (exp[i] < -128.0f) exp[i] = -128.0f;
                else if (exp[i] > 127.0f) exp[i] = 127.0f;
            }

            float common_exp = std::max(exp[0], std::max(exp[1], exp[2]));
            float range = std::exp2(common_exp);

            Ren::Vec3f mantissa = val / range;
            for (int i = 0; i < 3; i++) {
                if (mantissa[i] < 0.0f) mantissa[i] = 0.0f;
                else if (mantissa[i] > 1.0f) mantissa[i] = 1.0f;
            }

            Ren::Vec4f res = { mantissa[0], mantissa[1], mantissa[2], common_exp + 128.0f };

            uint8_t data[] = { uint8_t(res[2] * 255), uint8_t(res[1] * 255), uint8_t(res[0] * 255), uint8_t(res[3]) };
            file.write((const char *)&data[0], 4);
        }
    }

    static const char footer[26] = "\0\0\0\0" // no extension area
                                   "\0\0\0\0"// no developer directory
                                   "TRUEVISION-XFILE"// yep, this is a TGA file
                                   ".";
    file.write((const char *)&footer, sizeof(footer));
}

void LoadTGA(Sys::AssetFile &in_file, int w, int h, Ray::pixel_color8_t *out_data) {
    size_t in_file_size = (size_t)in_file.size();

    std::vector<char> in_file_data(in_file_size);
    in_file.Read(&in_file_data[0], in_file_size);

    Ren::eTexColorFormat format;
    int _w, _h;
    auto pixels = Ren::ReadTGAFile(&in_file_data[0], _w, _h, format);

    if (_w != w || _h != h) return;

    if (format == Ren::RawRGB888) {
        int i = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                out_data[i++] = { pixels[3 * (y * w + x)], pixels[3 * (y * w + x) + 1], pixels[3 * (y * w + x) + 2], 255 };
            }
        }
    } else if (format == Ren::RawRGBA8888) {
        int i = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                out_data[i++] = { pixels[4 * (y * w + x)], pixels[4 * (y * w + x) + 1], pixels[4 * (y * w + x) + 2], pixels[4 * (y * w + x) + 3] };
            }
        }
    } else {
        assert(false);
    }
}

std::vector<Ray::pixel_color_t> FlushSeams(const Ray::pixel_color_t *pixels, int res) {
    std::vector<Ray::pixel_color_t> temp_pixels1{ pixels, pixels + res * res },
                                    temp_pixels2{ (size_t)res * res };
    const int FILTER_SIZE = 16;
    const float INVAL_THRES = 0.5f;

    // apply dilation filter
    for (int i = 0; i < FILTER_SIZE; i++) {
        bool has_invalid = false;

        for (int y = 0; y < res; y++) {
            for (int x = 0; x < res; x++) {
                auto in_p = temp_pixels1[y * res + x];
                auto &out_p = temp_pixels2[y * res + x];

                float mul = 1.0f;
                if (in_p.a < INVAL_THRES) {
                    has_invalid = true;

                    Ray::pixel_color_t new_p = { 0 };
                    int count = 0;
                    for (int _y : { y - 1, y, y + 1 }) {
                        for (int _x : { x - 1, x, x + 1 }) {
                            if (_x < 0 || _y < 0 || _x > res - 1 || _y > res - 1) continue;

                            const auto &p = temp_pixels1[_y * res + _x];
                            if (p.a >= INVAL_THRES) {
                                new_p.r += p.r;
                                new_p.g += p.g;
                                new_p.b += p.b;
                                new_p.a += p.a;

                                count++;
                            }
                        }
                    }

                    if (count) {
                        float inv_c = 1.0f / count;
                        new_p.r *= inv_c;
                        new_p.g *= inv_c;
                        new_p.b *= inv_c;
                        new_p.a *= inv_c;

                        in_p = new_p;
                    }
                } else {
                    mul = 1.0f / in_p.a;
                }

                out_p.r = in_p.r * mul;
                out_p.g = in_p.g * mul;
                out_p.b = in_p.b * mul;
                out_p.a = in_p.a * mul;
            }
        }

        std::swap(temp_pixels1, temp_pixels2);
        if (!has_invalid) break;
    }

    return temp_pixels1;
}

std::unique_ptr<Ray::pixel_color8_t[]> GetTextureData(const Ren::Texture2DRef &tex_ref) {
    auto params = tex_ref->params();

    std::unique_ptr<Ray::pixel_color8_t[]> tex_data(new Ray::pixel_color8_t[params.w * params.h]);
#if defined(__ANDROID__)
    Sys::AssetFile in_file((std::string("assets/textures/") + tex_ref->name()).c_str());
    SceneManagerInternal::LoadTGA(in_file, params.w, params.h, &tex_data[0]);
#else
    tex_ref->ReadTextureData(Ren::RawRGBA8888, (void *)&tex_data[0]);
#endif

    return tex_data;
}

}

void SceneManager::PrepareAssets(const char *in_folder, const char *out_folder, const char *platform) {

}