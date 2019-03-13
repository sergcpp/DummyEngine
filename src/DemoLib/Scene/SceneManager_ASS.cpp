#include "SceneManager.h"

#include <fstream>
#include <functional>
#include <iterator>
#include <map>

#include <dirent.h>

extern "C" {
#include <Ren/SOIL2/image_DXT.h>
}
#include <Ren/SOIL2/SOIL2.h>

#undef max
#undef min

#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/ThreadPool.h>

namespace SceneManagerInternal {
void WriteImage(const std::vector<uint8_t> &out_data, int w, int h, int channels, const std::string &name) {
    int res = 0;
    if (strstr(name.c_str(), ".tga")) {
        res = SOIL_save_image(name.c_str(), SOIL_SAVE_TYPE_TGA, w, h, channels, out_data.data());
    } else if (strstr(name.c_str(), ".png")) {
        res = SOIL_save_image(name.c_str(), SOIL_SAVE_TYPE_PNG, w, h, channels, out_data.data());
    }

    if (!res) {
        LOGE("Failed to save image %s", name.c_str());
    }
}

void Write_RGBE(const std::vector<Ray::pixel_color_t> &out_data, int w, int h, const std::string &name) {
    std::vector<uint8_t> u8_data(w * h * 4);

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

            uint8_t r = (uint8_t)std::max(std::min(int(res[0] * 255), 255), 0);
            uint8_t g = (uint8_t)std::max(std::min(int(res[1] * 255), 255), 0);
            uint8_t b = (uint8_t)std::max(std::min(int(res[2] * 255), 255), 0);
            uint8_t a = (uint8_t)std::max(std::min(int(res[3]), 255), 0);

            u8_data[(y * w + x) * 4 + 0] = r;
            u8_data[(y * w + x) * 4 + 1] = g;
            u8_data[(y * w + x) * 4 + 2] = b;
            u8_data[(y * w + x) * 4 + 3] = a;
        }
    }

    WriteImage(u8_data, w, h, 4, name);
}

void Write_RGB(const std::vector<Ray::pixel_color_t> &out_data, int w, int h, const std::string &name) {
    std::vector<uint8_t> u8_data(w * h * 3);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const auto &p = out_data[y * w + x];

            u8_data[(y * w + x) * 3 + 0] = uint8_t(std::min(int(p.r * 255), 255));
            u8_data[(y * w + x) * 3 + 1] = uint8_t(std::min(int(p.g * 255), 255));
            u8_data[(y * w + x) * 3 + 2] = uint8_t(std::min(int(p.b * 255), 255));
        }
    }

    WriteImage(u8_data, w, h, 3, name);
}

void Write_RGBM(const std::vector<Ray::pixel_color_t> &out_data, int w, int h, const std::string &name) {
    std::vector<uint8_t> u8_data(w * h * 4);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            auto p = out_data[y * w + x];

            p.r *= 1.0f / 6.0f;
            p.g *= 1.0f / 6.0f;
            p.b *= 1.0f / 6.0f;

            p.a = std::max(std::max(p.r, p.g), std::max(p.b, 1e-6f));
            if (p.a > 1.0f) p.a = 1.0f;

            p.a = std::ceil(p.a * 255.0f) / 255.0f;
            p.r /= p.a;
            p.g /= p.a;
            p.b /= p.a;

            uint8_t r = (uint8_t)std::max(std::min(int(p.r * 255), 255), 0);
            uint8_t g = (uint8_t)std::max(std::min(int(p.g * 255), 255), 0);
            uint8_t b = (uint8_t)std::max(std::min(int(p.b * 255), 255), 0);
            uint8_t a = (uint8_t)std::max(std::min(int(p.a * 255), 255), 0);

            u8_data[(y * w + x) * 4 + 0] = r;
            u8_data[(y * w + x) * 4 + 1] = g;
            u8_data[(y * w + x) * 4 + 2] = b;
            u8_data[(y * w + x) * 4 + 3] = a;
        }
    }

    WriteImage(u8_data, w, h, 4, name);
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

    // Avoid bound checks in debug
    Ray::pixel_color_t *_temp_pixels1 = temp_pixels1.data(),
                       *_temp_pixels2 = temp_pixels2.data();

    // apply dilation filter
    for (int i = 0; i < FILTER_SIZE; i++) {
        bool has_invalid = false;

        for (int y = 0; y < res; y++) {
            for (int x = 0; x < res; x++) {
                auto in_p = _temp_pixels1[y * res + x];
                auto &out_p = _temp_pixels2[y * res + x];

                float mul = 1.0f;
                if (in_p.a < INVAL_THRES) {
                    has_invalid = true;

                    Ray::pixel_color_t new_p = { 0 };
                    int count = 0;
                    for (int _y : { y - 1, y, y + 1 }) {
                        for (int _x : { x - 1, x, x + 1 }) {
                            if (_x < 0 || _y < 0 || _x > res - 1 || _y > res - 1) continue;

                            const auto &p = _temp_pixels1[_y * res + _x];
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

        std::swap(_temp_pixels1, _temp_pixels2);
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

void ReadAllFiles_r(const char *in_folder, const std::function<void(const char *)> &callback) {
    DIR *in_dir = opendir(in_folder);
    if (!in_dir) {
        LOGE("Cannot open folder %s", in_folder);
        return;
    }

    struct dirent *in_ent = nullptr;
    while (in_ent = readdir(in_dir)) {
        if (in_ent->d_type == DT_DIR) {
            if (strcmp(in_ent->d_name, ".") == 0 || strcmp(in_ent->d_name, "..") == 0) {
                continue;
            }
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            ReadAllFiles_r(path.c_str(), callback);
        } else {
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            callback(path.c_str());
        }
    }

    closedir(in_dir);
}

void ReadAllFiles_MT_r(const char *in_folder, const std::function<void(const char *)> &callback, Sys::ThreadPool &threads, std::vector<std::future<void>> &events) {
    DIR *in_dir = opendir(in_folder);
    if (!in_dir) {
        LOGE("Cannot open folder %s", in_folder);
        return;
    }

    struct dirent *in_ent = nullptr;
    while (in_ent = readdir(in_dir)) {
        if (in_ent->d_type == DT_DIR) {
            if (strcmp(in_ent->d_name, ".") == 0 || strcmp(in_ent->d_name, "..") == 0) {
                continue;
            }
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            ReadAllFiles_r(path.c_str(), callback);
        } else {
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            events.push_back(threads.enqueue([path, &callback]() {
                callback(path.c_str());
            }));
        }
    }

    closedir(in_dir);
}

bool CheckCanSkipAsset(const char *in_file, const char *out_file) {
#ifdef _WIN32
    HANDLE in_h = CreateFile(in_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
    if (in_h == INVALID_HANDLE_VALUE) {
        LOGI("[PrepareAssets] Failed to open file!");
        CloseHandle(in_h);
        return true;
    }
    HANDLE out_h = CreateFile(out_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
    LARGE_INTEGER out_size = {};
    if (out_h != INVALID_HANDLE_VALUE && GetFileSizeEx(out_h, &out_size) && out_size.QuadPart) {
        FILETIME in_t, out_t;
        GetFileTime(in_h, NULL, NULL, &in_t);
        GetFileTime(out_h, NULL, NULL, &out_t);

        if (CompareFileTime(&in_t, &out_t) == -1) {
            CloseHandle(in_h);
            CloseHandle(out_h);
            return true;
        }
    }

    CloseHandle(in_h);
    CloseHandle(out_h);
#else
#error "Not Implemented!"
#endif
    return false;
}

bool CreateFolders(const char *out_file) {
#ifdef _WIN32
    const char *end = strchr(out_file, '/');
    while (end) {
        char folder[256] = {};
        strncpy(folder, out_file, end - out_file + 1);
        if (!CreateDirectory(folder, NULL)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                LOGI("[PrepareAssets] Failed to create directory!");
                return false;
            }
        }
        end = strchr(end + 1, '/');
    }
#else
#error "Not Implemented!"
#endif
    return true;
}

}

bool SceneManager::PrepareAssets(const char *in_folder, const char *out_folder, const char *platform, Sys::ThreadPool *p_threads) {
    using namespace SceneManagerInternal;

    auto replace_texture_extension = [](std::string &tex) {
        size_t n;
        if ((n = tex.find(".tga")) != std::string::npos) {
#if defined(__ANDROID__)
            // use astc textures later
#else
            tex.replace(n + 1, n + 3, "dds");
#endif
        } else if ((n = tex.find(".png")) != std::string::npos) {
#if defined(__ANDROID__)
            // use astc textures later
#else
            tex.replace(n + 1, n + 3, "dds");
#endif
        }
    };

    auto h_skip = [](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Skip %s", out_file);
    };

    auto h_copy = [](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Copy %s", out_file);

        std::ifstream src_stream(in_file, std::ios::binary);
        std::ofstream dst_stream(out_file, std::ios::binary);

        std::istreambuf_iterator<char> src_beg(src_stream);
        std::istreambuf_iterator<char> src_end;
        std::ostreambuf_iterator<char> dst_beg(dst_stream);
        std::copy(src_beg, src_end, dst_beg);
    };

    auto h_conv_to_dds = [](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Conv %s", out_file);

        std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
        auto src_size = src_stream.tellg();
        src_stream.seekg(0, std::ios::beg);

        std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
        src_stream.read((char *)&src_buf[0], src_size);

        int width, height, channels;
        unsigned char *image_data = SOIL_load_image_from_memory(&src_buf[0], (int)src_size, &width, &height, &channels, 0);

        // Check if power of two
        bool store_mipmaps = (width & (width - 1)) == 0 && (height & (height - 1)) == 0;

        std::unique_ptr<uint8_t[]> mipmaps[16] = {};
        int widths[16] = {},
            heights[16] = {};

        mipmaps[0].reset(new uint8_t[width * height * channels]);
        // mirror by y (????)
        for (int j = 0; j < height; j++) {
            memcpy(&mipmaps[0][j * width * channels], &image_data[(height - j - 1) * width * channels], width * channels);
        }
        widths[0] = width;
        heights[0] = height;
        int mip_count = 1;

        int _w = width, _h = height;
        while ((_w > 1 || _h > 1) && store_mipmaps) {
            int _prev_w = _w, _prev_h = _h;
            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);
            mipmaps[mip_count].reset(new uint8_t[_w * _h * channels]);
            widths[mip_count] = _w;
            heights[mip_count] = _h;
            const uint8_t *tex = mipmaps[mip_count - 1].get();

            int count = 0;

            if (channels == 4) {
                for (int j = 0; j < _prev_h; j += 2) {
                    for (int i = 0; i < _prev_w; i += 2) {
                        int r = tex[((j + 0) * _prev_w + i) * 4 + 0] + tex[((j + 0) * _prev_w + i + 1) * 4 + 0] +
                                tex[((j + 1) * _prev_w + i) * 4 + 0] + tex[((j + 1) * _prev_w + i + 1) * 4 + 0];
                        int g = tex[((j + 0) * _prev_w + i) * 4 + 1] + tex[((j + 0) * _prev_w + i + 1) * 4 + 1] +
                                tex[((j + 1) * _prev_w + i) * 4 + 1] + tex[((j + 1) * _prev_w + i + 1) * 4 + 1];
                        int b = tex[((j + 0) * _prev_w + i) * 4 + 2] + tex[((j + 0) * _prev_w + i + 1) * 4 + 2] +
                                tex[((j + 1) * _prev_w + i) * 4 + 2] + tex[((j + 1) * _prev_w + i + 1) * 4 + 2];
                        int a = tex[((j + 0) * _prev_w + i) * 4 + 3] + tex[((j + 0) * _prev_w + i + 1) * 4 + 3] +
                                tex[((j + 1) * _prev_w + i) * 4 + 3] + tex[((j + 1) * _prev_w + i + 1) * 4 + 3];

                        mipmaps[mip_count][count * 4 + 0] = uint8_t(r / 4);
                        mipmaps[mip_count][count * 4 + 1] = uint8_t(g / 4);
                        mipmaps[mip_count][count * 4 + 2] = uint8_t(b / 4);
                        mipmaps[mip_count][count * 4 + 3] = uint8_t(a / 4);
                        count++;
                    }
                }
            } else if (channels == 3) {
                for (int j = 0; j < _prev_h; j += 2) {
                    for (int i = 0; i < _prev_w; i += 2) {
                        int r = tex[((j + 0) * _prev_w + i) * 3 + 0] + tex[((j + 0) * _prev_w + i + 1) * 3 + 0] +
                                tex[((j + 1) * _prev_w + i) * 3 + 0] + tex[((j + 1) * _prev_w + i + 1) * 3 + 0];
                        int g = tex[((j + 0) * _prev_w + i) * 3 + 1] + tex[((j + 0) * _prev_w + i + 1) * 3 + 1] +
                                tex[((j + 1) * _prev_w + i) * 3 + 1] + tex[((j + 1) * _prev_w + i + 1) * 3 + 1];
                        int b = tex[((j + 0) * _prev_w + i) * 3 + 2] + tex[((j + 0) * _prev_w + i + 1) * 3 + 2] +
                                tex[((j + 1) * _prev_w + i) * 3 + 2] + tex[((j + 1) * _prev_w + i + 1) * 3 + 2];

                        mipmaps[mip_count][count * 3 + 0] = uint8_t(r / 4);
                        mipmaps[mip_count][count * 3 + 1] = uint8_t(g / 4);
                        mipmaps[mip_count][count * 3 + 2] = uint8_t(b / 4);
                        count++;
                    }
                }
            }

            mip_count++;
        }

        {
            uint8_t *dds_data[16] = {};
            int dds_size[16] = {};
            int dds_size_total = 0;

            for (int i = 0; i < mip_count; i++) {
                if (channels == 3) {
                    dds_data[i] = convert_image_to_DXT1(mipmaps[i].get(), widths[i], heights[i], channels, &dds_size[i]);
                } else if (channels == 4) {
                    dds_data[i] = convert_image_to_DXT5(mipmaps[i].get(), widths[i], heights[i], channels, &dds_size[i]);
                }
                dds_size_total += dds_size[i];
            }

            DDS_header header = {};
            header.dwMagic = ('D' << 0) | ('D' << 8) | ('S' << 16) | (' ' << 24);
            header.dwSize = 124;
            header.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT;
            header.dwWidth = width;
            header.dwHeight = height;
            header.dwPitchOrLinearSize = dds_size_total;
            header.dwMipMapCount = mip_count;
            header.sPixelFormat.dwSize = 32;
            header.sPixelFormat.dwFlags = DDPF_FOURCC;

            if (channels == 3) {
                header.sPixelFormat.dwFourCC = ('D' << 0) | ('X' << 8) | ('T' << 16) | ('1' << 24);
            } else {
                header.sPixelFormat.dwFourCC = ('D' << 0) | ('X' << 8) | ('T' << 16) | ('5' << 24);
            }

            header.sCaps.dwCaps1 = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;

            std::ofstream out_file(out_file, std::ios::binary);
            out_file.write((char *)&header, sizeof(header));

            for (int i = 0; i < mip_count; i++) {
                out_file.write((char *)dds_data[i], dds_size[i]);
                SOIL_free_image_data(dds_data[i]);
                dds_data[i] = nullptr;
            }
        }

        SOIL_free_image_data(image_data);
    };

    auto h_preprocess_material = [&replace_texture_extension](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Prep %s", out_file);

        std::ifstream src_stream(in_file, std::ios::binary);
        std::ofstream dst_stream(out_file, std::ios::binary);
        std::string line;

        while (std::getline(src_stream, line)) {
            replace_texture_extension(line);
            dst_stream << line << "\r\n";
        }
    };

    auto h_preprocess_scene = [&replace_texture_extension](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Prep %s", out_file);

        std::ifstream src_stream(in_file, std::ios::binary);
        std::ofstream dst_stream(out_file, std::ios::binary);

        JsObject js_scene;
        if (!js_scene.Read(src_stream)) {
            throw std::runtime_error("Cannot load scene!");
        }

        if (js_scene.Has("decals")) {
            JsObject &js_decals = (JsObject &)js_scene.at("decals");
            for (auto &d : js_decals.elements) {
                auto &js_decal = (JsObject &)d.second;
                if (js_decal.Has("diff")) {
                    auto &js_diff_tex = (JsString &)js_decal.at("diff");
                    replace_texture_extension(js_diff_tex.val);
                }
                if (js_decal.Has("norm")) {
                    auto &js_norm_tex = (JsString &)js_decal.at("norm");
                    replace_texture_extension(js_norm_tex.val);
                }
                if (js_decal.Has("spec")) {
                    auto &js_spec_tex = (JsString &)js_decal.at("spec");
                    replace_texture_extension(js_spec_tex.val);
                }
            }
        }

        js_scene.Write(dst_stream);
    };

    struct Handler {
        const char *ext;
        std::function<void(const char *in_file, const char *out_file)> convert;
    };

    std::map<std::string, Handler> handlers;

    handlers["bff"]     = { "bff",  h_copy };
    handlers["mesh"]    = { "mesh", h_copy };
    handlers["anim"]    = { "anim", h_copy };
    handlers["vs"]      = { "vs",   h_copy };
    handlers["fs"]      = { "fs",   h_copy };

    if (strcmp(platform, "pc_deb") == 0) {
        handlers["json"] = { "json", h_copy };
        handlers["txt"] = { "txt", h_copy };
        handlers["tga"] = { "tga", h_copy };
        handlers["hdr"] = { "hdr", h_copy };
        handlers["png"] = { "png", h_copy };
    } else if (strcmp(platform, "pc_rel") == 0) {
        handlers["json"] = { "json", h_preprocess_scene };
        handlers["txt"] = { "txt", h_preprocess_material };
        handlers["tga"] = { "dds", h_conv_to_dds };
        handlers["hdr"] = { "hdr", h_copy };
        handlers["png"] = { "dds", h_conv_to_dds };
    }

    auto convert_file = [out_folder, &handlers](const char *in_file) {
        const char *base_path = strchr(in_file, '/');
        if (!base_path) return;
        const char *ext = strrchr(in_file, '.');
        if (!ext) return;

        ext++;

        auto h_it = handlers.find(ext);
        if (h_it == handlers.end()) {
            LOGI("[PrepareAssets] No handler found for %s", in_file);
            return;
        }

        std::string out_file = out_folder;
        out_file += std::string(base_path, strlen(base_path) - strlen(ext));
        out_file += h_it->second.ext;

        if (CheckCanSkipAsset(in_file, out_file.c_str())) {
            return;
        }

        if (!CreateFolders(out_file.c_str())) {
            LOGI("[PrepareAssets] Failed to create directories for %s", out_file.c_str());
            return;
        }

        auto &conv_func = h_it->second.convert;
        conv_func(in_file, out_file.c_str());
    };

    if (p_threads) {
        std::vector<std::future<void>> events;
        ReadAllFiles_MT_r(in_folder, convert_file, *p_threads, events);

        for (auto &e : events) {
            e.wait();
        }
    } else {
        ReadAllFiles_r(in_folder, convert_file);
    }

    return true;
}