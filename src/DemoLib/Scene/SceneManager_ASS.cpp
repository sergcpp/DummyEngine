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

#include "../Renderer/Renderer_GL_Defines.inl"

namespace SceneManagerInternal {
extern const char *MODELS_PATH;
extern const char *TEXTURES_PATH;
extern const char *MATERIALS_PATH;
extern const char *SHADERS_PATH;

void WriteImage(const uint8_t *out_data, int w, int h, int channels, const char *name);

void Write_RGBE(const Ray::pixel_color_t *out_data, int w, int h, const char *name) {
    auto u8_data = Ren::ConvertRGB32F_to_RGBE(&out_data[0].r, w, h, 4);
    WriteImage(&u8_data[0], w, h, 4, name);
}

void Write_RGB(const Ray::pixel_color_t *out_data, int w, int h, const char *name) {
    std::vector<uint8_t> u8_data(w * h * 3);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const auto &p = out_data[y * w + x];

            u8_data[(y * w + x) * 3 + 0] = uint8_t(std::min(int(p.r * 255), 255));
            u8_data[(y * w + x) * 3 + 1] = uint8_t(std::min(int(p.g * 255), 255));
            u8_data[(y * w + x) * 3 + 2] = uint8_t(std::min(int(p.b * 255), 255));
        }
    }

    WriteImage(&u8_data[0], w, h, 3, name);
}

void Write_RGBM(const float *out_data, int w, int h, int channels, const char *name) {
    auto u8_data = Ren::ConvertRGB32F_to_RGBM(out_data, w, h, channels);
    WriteImage(&u8_data[0], w, h, 4, name);
}

void Write_DDS(const uint8_t *image_data, int w, int h, int channels, const char *out_file) {
    // Check if power of two
    bool store_mipmaps = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {},
        heights[16] = {};

    mipmaps[0].reset(new uint8_t[w * h * channels]);
    // mirror by y (????)
    for (int j = 0; j < h; j++) {
        memcpy(&mipmaps[0][j * w * channels], &image_data[(h - j - 1) * w * channels], w * channels);
    }
    widths[0] = w;
    heights[0] = h;
    int mip_count;

    if (store_mipmaps) {
        mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels);
    } else {
        mip_count = 1;
    }

    {
        uint8_t *dxt_data[16] = {};
        int dxt_size[16] = {};
        int dxt_size_total = 0;

        for (int i = 0; i < mip_count; i++) {
            if (channels == 3) {
                dxt_data[i] = convert_image_to_DXT1(mipmaps[i].get(), widths[i], heights[i], channels, &dxt_size[i]);
            } else if (channels == 4) {
                dxt_data[i] = convert_image_to_DXT5(mipmaps[i].get(), widths[i], heights[i], channels, &dxt_size[i]);
            }
            dxt_size_total += dxt_size[i];
        }

        DDS_header header = {};
        header.dwMagic = ('D' << 0) | ('D' << 8) | ('S' << 16) | (' ' << 24);
        header.dwSize = 124;
        header.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT;
        header.dwWidth = w;
        header.dwHeight = h;
        header.dwPitchOrLinearSize = dxt_size_total;
        header.dwMipMapCount = mip_count;
        header.sPixelFormat.dwSize = 32;
        header.sPixelFormat.dwFlags = DDPF_FOURCC;

        if (channels == 3) {
            header.sPixelFormat.dwFourCC = ('D' << 0) | ('X' << 8) | ('T' << 16) | ('1' << 24);
        } else {
            header.sPixelFormat.dwFourCC = ('D' << 0) | ('X' << 8) | ('T' << 16) | ('5' << 24);
        }

        header.sCaps.dwCaps1 = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;

        std::ofstream out_stream(out_file, std::ios::binary);
        out_stream.write((char *)&header, sizeof(header));

        for (int i = 0; i < mip_count; i++) {
            out_stream.write((char *)dxt_data[i], dxt_size[i]);
            SOIL_free_image_data(dxt_data[i]);
            dxt_data[i] = nullptr;
        }
    }
}

void Write_KTX_DXT(const uint8_t *image_data, int w, int h, int channels, const char *out_file) {
    // Check if power of two
    bool store_mipmaps = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {},
        heights[16] = {};

    mipmaps[0].reset(new uint8_t[w * h * channels]);
    // mirror by y (????)
    for (int j = 0; j < h; j++) {
        memcpy(&mipmaps[0][j * w * channels], &image_data[(h - j - 1) * w * channels], w * channels);
    }
    widths[0] = w;
    heights[0] = h;
    int mip_count;

    if (store_mipmaps) {
        mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels);
    } else {
        mip_count = 1;
    }

    {   // Write file
        uint8_t *dxt_data[16] = {};
        int dxt_size[16] = {};
        int dxt_size_total = 0;

        for (int i = 0; i < mip_count; i++) {
            if (channels == 3) {
                dxt_data[i] = convert_image_to_DXT1(mipmaps[i].get(), widths[i], heights[i], channels, &dxt_size[i]);
            } else if (channels == 4) {
                dxt_data[i] = convert_image_to_DXT5(mipmaps[i].get(), widths[i], heights[i], channels, &dxt_size[i]);
            }
            dxt_size_total += dxt_size[i];
        }

        const uint32_t gl_rgb = 0x1907;
        const uint32_t gl_rgba = 0x1908;

        const uint32_t RGB_S3TC_DXT1 = 0x83F0;
        const uint32_t RGBA_S3TC_DXT5 = 0x83F3;

        Ren::KTXHeader header = {};
        header.gl_type = 0;
        header.gl_type_size = 1;
        header.gl_format = 0; // should be zero for compressed texture
        if (channels == 4) {
            header.gl_internal_format = RGBA_S3TC_DXT5;
            header.gl_base_internal_format = gl_rgba;
        } else {
            header.gl_internal_format = RGB_S3TC_DXT1;
            header.gl_base_internal_format = gl_rgb;
        }
        header.pixel_width = w;
        header.pixel_height = h;
        header.pixel_depth = 0;

        header.array_elements_count = 0;
        header.faces_count = 1;
        header.mipmap_levels_count = mip_count;
        
        header.key_value_data_size = 0;

        uint32_t file_offset = 0;
        std::ofstream out_stream(out_file, std::ios::binary);
        out_stream.write((char *)&header, sizeof(header));
        file_offset += sizeof(header);

        for (int i = 0; i < mip_count; i++) {
            assert((file_offset % 4) == 0);
            uint32_t size = (uint32_t)dxt_size[i];
            out_stream.write((char *)&size, sizeof(uint32_t));
            file_offset += sizeof(uint32_t);
            out_stream.write((char *)dxt_data[i], size);
            file_offset += size;

            uint32_t pad = (file_offset % 4) ? (4 - (file_offset % 4)) : 0;
            while (pad) {
                const uint8_t zero_byte = 0;
                out_stream.write((char *)&zero_byte, 1);
                pad--;
            }

            SOIL_free_image_data(dxt_data[i]);
            dxt_data[i] = nullptr;
        }
    }
}

int ConvertToASTC(const uint8_t *image_data, int width, int height, int channels, float bitrate, std::unique_ptr<uint8_t[]> &out_buf);
std::unique_ptr<uint8_t[]> DecodeASTC(const uint8_t *image_data, int data_size, int xdim, int ydim, int width, int height);
std::unique_ptr<uint8_t[]> Decode_KTX_ASTC(const uint8_t *image_data, int data_size, int &width, int &height);

void Write_KTX_ASTC(const uint8_t *image_data, int w, int h, int channels, const char *out_file) {
    // Check if power of two
    bool store_mipmaps = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {},
        heights[16] = {};

    mipmaps[0].reset(new uint8_t[w * h * channels]);
    // mirror by y (????)
    for (int j = 0; j < h; j++) {
        memcpy(&mipmaps[0][j * w * channels], &image_data[(h - j - 1) * w * channels], w * channels);
    }
    widths[0] = w;
    heights[0] = h;
    int mip_count;

    if (store_mipmaps) {
        mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels);
    } else {
        mip_count = 1;
    }

    bool high_quality = strstr(out_file, "lightmaps") != 0;

    {   // Write file
        std::unique_ptr<uint8_t[]> astc_data[16];
        int astc_size[16] = {};
        int astc_size_total = 0;

        for (int i = 0; i < mip_count; i++) {
            astc_size[i] = ConvertToASTC(mipmaps[i].get(), widths[i], heights[i], channels, high_quality ? 8.0f : 2.0f, astc_data[i]);
            astc_size_total += astc_size[i];
        }

        const uint32_t gl_rgb = 0x1907;
        const uint32_t gl_rgba = 0x1908;

        const uint32_t gl_compressed_rgba_astc_4x4_khr = 0x93B0;
        const uint32_t gl_compressed_rgba_astc_6x6_khr = 0x93B4;
        const uint32_t gl_compressed_rgba_astc_8x8_khr = 0x93B7;

        Ren::KTXHeader header = {};
        header.gl_type = 0;
        header.gl_type_size = 1;
        header.gl_format = 0; // should be zero for compressed texture
        if (high_quality) {
            header.gl_internal_format = gl_compressed_rgba_astc_4x4_khr;
        } else {
            header.gl_internal_format = gl_compressed_rgba_astc_8x8_khr;
        }
        if (channels == 4) {
            header.gl_base_internal_format = gl_rgba;
        } else {
            header.gl_base_internal_format = gl_rgb;
        }
        header.pixel_width = w;
        header.pixel_height = h;
        header.pixel_depth = 0;

        header.array_elements_count = 0;
        header.faces_count = 1;
        header.mipmap_levels_count = mip_count;

        header.key_value_data_size = 0;

        uint32_t file_offset = 0;
        std::ofstream out_stream(out_file, std::ios::binary);
        out_stream.write((char *)&header, sizeof(header));
        file_offset += sizeof(header);

        for (int i = 0; i < mip_count; i++) {
            assert((file_offset % 4) == 0);
            uint32_t size = (uint32_t)astc_size[i];
            out_stream.write((char *)&size, sizeof(uint32_t));
            file_offset += sizeof(uint32_t);
            out_stream.write((char *)astc_data[i].get(), size);
            file_offset += size;

            uint32_t pad = (file_offset % 4) ? (4 - (file_offset % 4)) : 0;
            while (pad) {
                const uint8_t zero_byte = 0;
                out_stream.write((char *)&zero_byte, 1);
                pad--;
            }
        }
    }
}

void WriteImage(const uint8_t *out_data, int w, int h, int channels, const char *name) {
    int res = 0;
    if (strstr(name, ".tga")) {
        res = SOIL_save_image(name, SOIL_SAVE_TYPE_TGA, w, h, channels, out_data);
    } else if (strstr(name, ".png")) {
        res = SOIL_save_image(name, SOIL_SAVE_TYPE_PNG, w, h, channels, out_data);
    } else if (strstr(name, ".dds")) {
        res = 1;
        Write_DDS(out_data, w, h, channels, name);
    } else if (strstr(name, ".ktx")) {
        res = 1;
        Write_KTX_ASTC(out_data, w, h, channels, name);
    }

    if (!res) {
        LOGE("Failed to save image %s", name);
    }
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

std::vector<Ray::pixel_color_t> FlushSeams(const Ray::pixel_color_t *pixels, int width, int height, float invalid_threshold, int filter_size) {
    std::vector<Ray::pixel_color_t> temp_pixels1{ pixels, pixels + width * height },
                                    temp_pixels2{ (size_t)width * height };

    // Avoid bound checks in debug
    Ray::pixel_color_t *_temp_pixels1 = temp_pixels1.data(),
                       *_temp_pixels2 = temp_pixels2.data();

    // apply dilation filter
    for (int i = 0; i < filter_size; i++) {
        bool has_invalid = false;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                auto in_p = _temp_pixels1[y * width + x];
                auto &out_p = _temp_pixels2[y * width + x];

                float mul = 1.0f;
                if (in_p.a < invalid_threshold) {
                    has_invalid = true;

                    Ray::pixel_color_t new_p = { 0 };
                    int count = 0;
                    for (int _y : { y - 1, y, y + 1 }) {
                        for (int _x : { x - 1, x, x + 1 }) {
                            if (_x < 0 || _y < 0 || _x > width - 1 || _y > height - 1) continue;

                            const auto &p = _temp_pixels1[_y * width + _x];
                            if (p.a >= invalid_threshold) {
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
    while ((in_ent = readdir(in_dir))) {
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
    while ((in_ent = readdir(in_dir))) {
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
#warning "Not Implemented!"
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
#warning "Not Implemented!"
#endif
    return true;
}

}

// these are from astc codec
#undef IGNORE
#include <astc/astc_codec_internals.h>
#undef MAX

int astc_main(int argc, char **argv);

void test_inappropriate_extended_precision(void);
void prepare_angular_tables(void);
void build_quantization_mode_table(void);
void find_closest_blockdim_2d(float target_bitrate, int *x, int *y, int consider_illegal);

void encode_astc_image(const astc_codec_image *input_image,
                       astc_codec_image *output_image,
                       int xdim,
                       int ydim,
                       int zdim,
                       const error_weighting_params *ewp, astc_decode_mode decode_mode, swizzlepattern swz_encode, swizzlepattern swz_decode, uint8_t * buffer, int pack_and_unpack, int threadcount);

bool g_astc_initialized = false;

bool SceneManager::PrepareAssets(const char *in_folder, const char *out_folder, const char *platform, Sys::ThreadPool *p_threads) {
    using namespace SceneManagerInternal;

    // for astc codec
    if (!g_astc_initialized) {
        test_inappropriate_extended_precision();
        prepare_angular_tables();
        build_quantization_mode_table();
        g_astc_initialized = true;
    }

    auto replace_texture_extension = [platform](std::string &tex) {
        size_t n;
        if ((n = tex.find(".tga")) != std::string::npos) {
            if (strcmp(platform, "pc") == 0) {
                tex.replace(n + 1, n + 3, "dds");
            } else if (strcmp(platform, "android") == 0) {
                tex.replace(n + 1, n + 3, "ktx");
            }
        } else if ((n = tex.find(".png")) != std::string::npos) {
            if (strcmp(platform, "pc") == 0) {
                tex.replace(n + 1, n + 3, "dds");
            } else if (strcmp(platform, "android") == 0) {
                tex.replace(n + 1, n + 3, "ktx");
            }
        }
    };

    std::map<std::string, std::string> shader_constants;

    shader_constants.emplace("$ModifyWarning", "/***********************************************/\n"
                                               "/* This file was autogenerated, do not modify! */\n"
                                               "/***********************************************/");

    shader_constants.emplace("$FltEps",         "0.0000001");

    shader_constants.emplace("$ItemGridResX",   AS_STR(REN_GRID_RES_X));
    shader_constants.emplace("$ItemGridResY",   AS_STR(REN_GRID_RES_Y));
    shader_constants.emplace("$ItemGridResZ",   AS_STR(REN_GRID_RES_Z));

    // Vertex attributes
    shader_constants.emplace("$VtxPosLoc",      AS_STR(REN_VTX_POS_LOC));
    shader_constants.emplace("$VtxNorLoc",      AS_STR(REN_VTX_NOR_LOC));
    shader_constants.emplace("$VtxTanLoc",      AS_STR(REN_VTX_TAN_LOC));
    shader_constants.emplace("$VtxUV1Loc",      AS_STR(REN_VTX_UV1_LOC));
    shader_constants.emplace("$VtxUV2Loc",      AS_STR(REN_VTX_UV2_LOC));

    // Texture slots
    shader_constants.emplace("$DiffTexSlot",    AS_STR(REN_DIFF_TEX_SLOT));
    shader_constants.emplace("$NormTexSlot",    AS_STR(REN_NORM_TEX_SLOT));
    shader_constants.emplace("$SpecTexSlot",    AS_STR(REN_SPEC_TEX_SLOT));
    shader_constants.emplace("$ShadTexSlot",    AS_STR(REN_SHAD_TEX_SLOT));
    shader_constants.emplace("$LmapDirSlot",    AS_STR(REN_LMAP_DIR_SLOT));
    shader_constants.emplace("$LmapIndirSlot",  AS_STR(REN_LMAP_INDIR_SLOT));
    shader_constants.emplace("$LmapSHSlot",     AS_STR(REN_LMAP_SH_SLOT));
    shader_constants.emplace("$DecalTexSlot",   AS_STR(REN_DECAL_TEX_SLOT));
    shader_constants.emplace("$SSAOTexSlot",    AS_STR(REN_SSAO_TEX_SLOT));
    shader_constants.emplace("$LightBufSlot",   AS_STR(REN_LIGHT_BUF_SLOT));
    shader_constants.emplace("$DecalBufSlot",   AS_STR(REN_DECAL_BUF_SLOT));
    shader_constants.emplace("$CellsBufSlot",   AS_STR(REN_CELLS_BUF_SLOT));
    shader_constants.emplace("$ItemsBufSlot",   AS_STR(REN_ITEMS_BUF_SLOT));
    shader_constants.emplace("$InstanceBufSlot",AS_STR(REN_INSTANCE_BUF_SLOT));

    // Uniform locations
    shader_constants.emplace("$uMMatrixLoc",    AS_STR(REN_U_M_MATRIX_LOC));
    shader_constants.emplace("$uInstancesLoc",  AS_STR(REN_U_INSTANCES_LOC));

    // Uniform block locations
    shader_constants.emplace("$ubSharedDataLoc",AS_STR(REN_UB_SHARED_DATA_LOC));

    // Shader output channels
    shader_constants.emplace("$OutColorIndex",  AS_STR(REN_OUT_COLOR_INDEX));
    shader_constants.emplace("$OutNormIndex",   AS_STR(REN_OUT_NORM_INDEX));
    shader_constants.emplace("$OutSpecIndex",   AS_STR(REN_OUT_SPEC_INDEX));

    // Shadow properties
    if (strcmp(platform, "pc") == 0) {
        shader_constants.emplace("$ShadRes",    AS_STR(REN_SHAD_RES_PC));
    } else if (strcmp(platform, "android") == 0) {
        shader_constants.emplace("$ShadRes",    AS_STR(REN_SHAD_RES_ANDROID));
    }

    shader_constants.emplace("$ShadCasc0Dist",  AS_STR(REN_SHAD_CASCADE0_DIST));
    shader_constants.emplace("$ShadCasc0Samp",  AS_STR(REN_SHAD_CASCADE0_SAMPLES));
    shader_constants.emplace("$ShadCasc1Dist",  AS_STR(REN_SHAD_CASCADE1_DIST));
    shader_constants.emplace("$ShadCasc1Samp",  AS_STR(REN_SHAD_CASCADE1_SAMPLES));
    shader_constants.emplace("$ShadCasc2Dist",  AS_STR(REN_SHAD_CASCADE2_DIST));
    shader_constants.emplace("$ShadCasc2Samp",  AS_STR(REN_SHAD_CASCADE2_SAMPLES));
    shader_constants.emplace("$ShadCasc3Dist",  AS_STR(REN_SHAD_CASCADE3_DIST));
    shader_constants.emplace("$ShadCasc3Samp",  AS_STR(REN_SHAD_CASCADE3_SAMPLES));

    shader_constants.emplace("$SSAOBufResDiv",  AS_STR(REN_SSAO_BUF_RES_DIV));

    shader_constants.emplace("$MaxShadowMaps",  AS_STR(REN_MAX_SHADOWMAPS_TOTAL));

    auto inline_constants = [&shader_constants](std::string &line) {
        size_t n = 0;
        while ((n = line.find('$', n)) != std::string::npos) {
            size_t l = 1;

            const char punctuation_chars[] = ".,(); $*[]";
            while (std::find(std::begin(punctuation_chars), std::end(punctuation_chars), line[n + l]) == std::end(punctuation_chars)) {
                l++;
            }

            const std::string var = line.substr(n, l);

            const auto it = shader_constants.find(var);
            if (it != shader_constants.end()) {
                line.replace(n, l, it->second);
            } else {
                throw std::runtime_error("Unknow variable!");
            }
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
        auto src_size = (size_t)src_stream.tellg();
        src_stream.seekg(0, std::ios::beg);

        std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
        src_stream.read((char *)&src_buf[0], src_size);

        int width, height, channels;
        unsigned char *image_data = SOIL_load_image_from_memory(&src_buf[0], (int)src_size, &width, &height, &channels, 0);

        Write_DDS(image_data, width, height, channels, out_file);

        SOIL_free_image_data(image_data);
    };

    auto h_conv_to_astc = [](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Conv %s", out_file);

        std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
        auto src_size = (size_t)src_stream.tellg();
        src_stream.seekg(0, std::ios::beg);

        std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
        src_stream.read((char *)&src_buf[0], src_size);

        int width, height, channels;
        unsigned char *image_data = SOIL_load_image_from_memory(&src_buf[0], (int)src_size, &width, &height, &channels, 0);

        Write_KTX_ASTC(image_data, width, height, channels, out_file);

        SOIL_free_image_data(image_data);
    };

    auto h_conv_hdr_to_rgbm = [](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Conv %s", out_file);

        int width, height;
        auto image_rgbe = LoadHDR(in_file, width, height);
        auto image_f32 = Ren::ConvertRGBE_to_RGB32F(&image_rgbe[0], width, height);
        
        std::unique_ptr<float[]> temp(new float[width * 3]);
        for (int j = 0; j < height / 2; j++) {
            int j1 = j, j2 = height - j - 1;
            memcpy(&temp[0], &image_f32[j1 * width * 3], width * 3 * sizeof(float));
            memcpy(&image_f32[j1 * width * 3], &image_f32[j2 * width * 3], width * 3 * sizeof(float));
            memcpy(&image_f32[j2 * width * 3], &temp[0], width * 3 * sizeof(float));
        }

        Write_RGBM(&image_f32[0], width, height, 3, out_file);
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

    auto h_preprocess_shader = [&inline_constants](const char *in_file, const char *out_file) {
        LOGI("[PrepareAssets] Prep %s", out_file);

        std::ifstream src_stream(in_file, std::ios::binary);
        std::ofstream dst_stream(out_file, std::ios::binary);
        std::string line;

        while (std::getline(src_stream, line)) {
            inline_constants(line);
            dst_stream << line << "\r\n";
        }
    };

    struct Handler {
        const char *ext;
        std::function<void(const char *in_file, const char *out_file)> convert;
    };

    std::map<std::string, Handler> handlers;

    handlers["bff"]     = { "bff",  h_copy };
    handlers["mesh"]    = { "mesh", h_copy };
    handlers["anim"]    = { "anim", h_copy };
    handlers["vs.glsl"] = { "vs.glsl", h_preprocess_shader };
    handlers["fs.glsl"] = { "fs.glsl", h_preprocess_shader };

    if (strcmp(platform, "pc") == 0) {
        handlers["json"]    = { "json", h_preprocess_scene };
        handlers["txt"]     = { "txt", h_preprocess_material };
        handlers["tga"]     = { "dds", h_conv_to_dds };
        handlers["hdr"]     = { "dds", h_conv_hdr_to_rgbm };
        handlers["png"]     = { "dds", h_conv_to_dds };
    } else if (strcmp(platform, "android") == 0) {
        handlers["json"]    = { "json", h_preprocess_scene };
        handlers["txt"]     = { "txt", h_preprocess_material };
        handlers["tga"]     = { "ktx", h_conv_to_astc };
        handlers["hdr"]     = { "ktx", h_conv_hdr_to_rgbm };
        handlers["png"]     = { "ktx", h_conv_to_astc };
    }

    auto convert_file = [out_folder, &handlers](const char *in_file) {
        const char *base_path = strchr(in_file, '/');
        if (!base_path) return;
        const char *ext = strchr(in_file, '.');
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

    /*if (strcmp(platform, "android") == 0) {
        int argc = 6;
        char *argv[] = { "astc", "-c", "barrel_diffuse.png", "barrel_diffuse.astc", "2.0", "-medium" };

        //astc_main(argc, argv);

        h_conv_to_astc("barrel_diffuse.png", "barrel_diffuse1.astc");
    }*/

    /*{
        Sys::AssetFile in_file("D:\\repos\\occdemo\\assets_android\\textures\\lightmaps\\jap_house_lm_direct.ktx");
        size_t in_file_size = in_file.size();

        std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
        in_file.Read((char *)&in_file_data[0], in_file_size);

        int width, height;
        auto data = Decode_KTX_ASTC(&in_file_data[0], in_file_size, width, height);

        volatile int ii = 0;
    }*/

    return true;
}

int SceneManagerInternal::ConvertToASTC(const uint8_t *image_data, int width, int height, int channels, float bitrate, std::unique_ptr<uint8_t[]> &out_buf) {
    int padding = channels == 4 ? 1 : 0;
    
    astc_codec_image *src_image = allocate_image(8, width, height, 1, padding);

    if (channels == 4) {
        uint8_t *_img = &src_image->imagedata8[0][0][0];
        for (int j = 0; j < height; j++) {
            int y = j + padding;
            for (int i = 0; i < width; i++) {
                int x = i + padding;
                src_image->imagedata8[0][y][4 * x + 0] = image_data[4 * (j * width + i) + 0];
                src_image->imagedata8[0][y][4 * x + 1] = image_data[4 * (j * width + i) + 1];
                src_image->imagedata8[0][y][4 * x + 2] = image_data[4 * (j * width + i) + 2];
                src_image->imagedata8[0][y][4 * x + 3] = image_data[4 * (j * width + i) + 3];
            }
        }
    } else {
        uint8_t *_img = &src_image->imagedata8[0][0][0];
        for (int j = 0; j < height; j++) {
            int y = j + padding;
            for (int i = 0; i < width; i++) {
                int x = i + padding;
                _img[4 * (y * width + x) + 0] = image_data[3 * (j * width + i) + 0];
                _img[4 * (y * width + x) + 1] = image_data[3 * (j * width + i) + 1];
                _img[4 * (y * width + x) + 2] = image_data[3 * (j * width + i) + 2];
                _img[4 * (y * width + x) + 3] = 255;
            }
        }
    }

    int buf_size = 0;

    {
        const float target_bitrate = bitrate;
        int xdim, ydim;

        find_closest_blockdim_2d(target_bitrate, &xdim, &ydim, 0);

        float log10_texels_2d = (std::log((float)(xdim * ydim)) / std::log(10.0f));

        // 'medium' preset params
        int plimit_autoset = 25;
        float oplimit_autoset = 1.2f;
        float mincorrel_autoset = 0.75f;
        float dblimit_autoset_2d = std::max(95 - 35 * log10_texels_2d, 70 - 19 * log10_texels_2d);
        float bmc_autoset = 75;
        int maxiters_autoset = 2;

        int pcdiv;

        switch (ydim) {
        case 4:
            pcdiv = 25;
            break;
        case 5:
            pcdiv = 15;
            break;
        case 6:
            pcdiv = 15;
            break;
        case 8:
            pcdiv = 10;
            break;
        case 10:
            pcdiv = 8;
            break;
        case 12:
            pcdiv = 6;
            break;
        default:
            pcdiv = 6;
            break;
        };

        error_weighting_params ewp;

        ewp.rgb_power = 1.0f;
        ewp.alpha_power = 1.0f;
        ewp.rgb_base_weight = 1.0f;
        ewp.alpha_base_weight = 1.0f;
        ewp.rgb_mean_weight = 0.0f;
        ewp.rgb_stdev_weight = 0.0f;
        ewp.alpha_mean_weight = 0.0f;
        ewp.alpha_stdev_weight = 0.0f;

        ewp.rgb_mean_and_stdev_mixing = 0.0f;
        ewp.mean_stdev_radius = 0;
        ewp.enable_rgb_scale_with_alpha = 0;
        ewp.alpha_radius = 0;

        ewp.block_artifact_suppression = 0.0f;
        ewp.rgba_weights[0] = 1.0f;
        ewp.rgba_weights[1] = 1.0f;
        ewp.rgba_weights[2] = 1.0f;
        ewp.rgba_weights[3] = 1.0f;
        ewp.ra_normal_angular_scale = 0;

        int partitions_to_test = plimit_autoset;
        float dblimit_2d = dblimit_autoset_2d;
        float oplimit = oplimit_autoset;
        float mincorrel = mincorrel_autoset;

        int maxiters = maxiters_autoset;
        ewp.max_refinement_iters = maxiters;

        ewp.block_mode_cutoff = (bmc_autoset) / 100.0f;

        ewp.texel_avg_error_limit = 0.0f;

        ewp.partition_1_to_2_limit = oplimit;
        ewp.lowest_correlation_cutoff = mincorrel;

        if (partitions_to_test < 1) {
            partitions_to_test = 1;
        } else if (partitions_to_test > PARTITION_COUNT) {
            partitions_to_test = PARTITION_COUNT;
        }
        ewp.partition_search_limit = partitions_to_test;

        float max_color_component_weight = std::max(std::max(ewp.rgba_weights[0], ewp.rgba_weights[1]),
                                                    std::max(ewp.rgba_weights[2], ewp.rgba_weights[3]));
        ewp.rgba_weights[0] = std::max(ewp.rgba_weights[0], max_color_component_weight / 1000.0f);
        ewp.rgba_weights[1] = std::max(ewp.rgba_weights[1], max_color_component_weight / 1000.0f);
        ewp.rgba_weights[2] = std::max(ewp.rgba_weights[2], max_color_component_weight / 1000.0f);
        ewp.rgba_weights[3] = std::max(ewp.rgba_weights[3], max_color_component_weight / 1000.0f);

        if (channels == 4) {
            ewp.enable_rgb_scale_with_alpha = 1;
            ewp.alpha_radius = 1;
        }

        expand_block_artifact_suppression(xdim, ydim, 1, &ewp);

        swizzlepattern swz_encode = { 0, 1, 2, 3 };

        //int padding = std::max(ewp.mean_stdev_radius, ewp.alpha_radius);

        if (channels == 4 /*ewp.rgb_mean_weight != 0.0f || ewp.rgb_stdev_weight != 0.0f || ewp.alpha_mean_weight != 0.0f || ewp.alpha_stdev_weight != 0.0f*/) {
            
            compute_averages_and_variances(src_image, ewp.rgb_power, ewp.alpha_power, ewp.mean_stdev_radius, ewp.alpha_radius, swz_encode);
        }

        int xsize = src_image->xsize;
        int ysize = src_image->ysize;

        int xblocks = (xsize + xdim - 1) / xdim;
        int yblocks = (ysize + ydim - 1) / ydim;
        int zblocks = 1;

        buf_size = xblocks * yblocks * zblocks * 16;
        out_buf.reset(new uint8_t[buf_size]);

        encode_astc_image(src_image, nullptr, xdim, ydim, 1, &ewp, DECODE_LDR, swz_encode, swz_encode, &out_buf[0], 0, 8);
    }

    destroy_image(src_image);

    return buf_size;
}

std::unique_ptr<uint8_t[]> SceneManagerInternal::DecodeASTC(const uint8_t *image_data, int data_size, int xdim, int ydim, int width, int height) {
    int xsize = width;
    int ysize = height;
    int zsize = 1;

    int xblocks = (xsize + xdim - 1) / xdim;
    int yblocks = (ysize + ydim - 1) / ydim;
    int zblocks = 1;
    
    if (!g_astc_initialized) {
        test_inappropriate_extended_precision();
        prepare_angular_tables();
        build_quantization_mode_table();
        g_astc_initialized = true;
    }

    astc_codec_image *img = allocate_image(8, xsize, ysize, 1, 0);
    initialize_image(img);

    swizzlepattern swz_decode = { 0, 1, 2, 3 };

    imageblock pb;
    for (int z = 0; z < zblocks; z++) {
        for (int y = 0; y < yblocks; y++) {
            for (int x = 0; x < xblocks; x++) {
                int offset = (((z * yblocks + y) * xblocks) + x) * 16;
                const uint8_t *bp = image_data + offset;

                physical_compressed_block pcb;
                memcpy(&pcb, bp, sizeof(physical_compressed_block));

                symbolic_compressed_block scb;
                physical_to_symbolic(xdim, ydim, 1, pcb, &scb);
                decompress_symbolic_block(DECODE_LDR, xdim, ydim, 1, x * xdim, y * ydim, z * 1, &scb, &pb);
                write_imageblock(img, &pb, xdim, ydim, 1, x * xdim, y * ydim, z * 1, swz_decode);
            }
        }
    }

    std::unique_ptr<uint8_t[]> ret_data;
    ret_data.reset(new uint8_t[xsize * ysize * 4]);

    memcpy(&ret_data[0], &img->imagedata8[0][0][0], xsize * ysize * 4);

    destroy_image(img);

    return ret_data;
}

std::unique_ptr<uint8_t[]> SceneManagerInternal::Decode_KTX_ASTC(const uint8_t *image_data, int data_size, int &width, int &height) {
    Ren::KTXHeader header;
    memcpy(&header, &image_data[0], sizeof(Ren::KTXHeader));

    width = (int)header.pixel_width;
    height = (int)header.pixel_height;

    int data_offset = sizeof(Ren::KTXHeader);

    {   // Decode first mip level
        uint32_t img_size;
        memcpy(&img_size, &image_data[data_offset], sizeof(uint32_t));
        data_offset += sizeof(uint32_t);

        const uint32_t gl_compressed_rgba_astc_4x4_khr = 0x93B0;
        const uint32_t gl_compressed_rgba_astc_6x6_khr = 0x93B4;
        const uint32_t gl_compressed_rgba_astc_8x8_khr = 0x93B7;

        int xdim, ydim;

        if (header.gl_internal_format == gl_compressed_rgba_astc_4x4_khr) {
            xdim = 4;
            ydim = 4;
        } else if (header.gl_internal_format == gl_compressed_rgba_astc_6x6_khr) {
            xdim = 6;
            ydim = 6;
        } else if (header.gl_internal_format == gl_compressed_rgba_astc_8x8_khr) {
            xdim = 8;
            ydim = 8;
        } else {
            throw std::runtime_error("Unsupported block size!");
        }

        return DecodeASTC(&image_data[data_offset], img_size, xdim, ydim, width, height);
    }
}