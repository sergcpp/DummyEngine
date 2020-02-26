#include "SceneManager.h"

#include <fstream>

#include <Eng/Utils/Load.h>
#include <Net/Compress.h>
extern "C" {
#include <Ren/SOIL2/image_DXT.h>
}
#include <Ren/SOIL2/SOIL2.h>
#include <Ren/Utils.h>
#include <Sys/Json.h>

namespace SceneManagerInternal {
int WriteImage(const uint8_t *out_data, int w, int h, int channels, const char *name);

void Write_RGBE(const Ray::pixel_color_t *out_data, int w, int h, const char *name) {
    std::unique_ptr<uint8_t[]> u8_data = Ren::ConvertRGB32F_to_RGBE(&out_data[0].r, w, h, 4);
    WriteImage(&u8_data[0], w, h, 4, name);
}

void Write_RGB(const Ray::pixel_color_t *out_data, int w, int h, const char *name) {
    std::vector<uint8_t> u8_data(w * h * 3);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const Ray::pixel_color_t &p = out_data[y * w + x];

            u8_data[(y * w + x) * 3 + 0] = uint8_t(std::min(int(p.r * 255), 255));
            u8_data[(y * w + x) * 3 + 1] = uint8_t(std::min(int(p.g * 255), 255));
            u8_data[(y * w + x) * 3 + 2] = uint8_t(std::min(int(p.b * 255), 255));
        }
    }

    WriteImage(&u8_data[0], w, h, 3, name);
}

void Write_RGBM(const float *out_data, int w, int h, int channels, const char *name) {
    std::unique_ptr<uint8_t[]> u8_data = Ren::ConvertRGB32F_to_RGBM(out_data, w, h, channels);
    WriteImage(&u8_data[0], w, h, 4, name);
}

void Write_DDS_Mips(
        const uint8_t * const * mipmaps, const int *widths, const int *heights, const int mip_count,
        const int channels, const char *out_file) {
    //
    // Compress mip images
    //
    uint8_t *dxt_data[16] = {};
    int dxt_size[16] = {};
    int dxt_size_total = 0;

    for (int i = 0; i < mip_count; i++) {
        if (channels == 3) {
            dxt_data[i] = convert_image_to_DXT1(mipmaps[i], widths[i], heights[i], channels, &dxt_size[i]);
        } else if (channels == 4) {
            dxt_data[i] = convert_image_to_DXT5(mipmaps[i], widths[i], heights[i], channels, &dxt_size[i]);
        }
        dxt_size_total += dxt_size[i];
    }

    //
    // Write out file
    //
    DDS_header header = {};
    header.dwMagic = (unsigned('D') << 0u) | (unsigned('D') << 8u) | (unsigned('S') << 16u) | (unsigned(' ') << 24u);
    header.dwSize = 124;
    header.dwFlags =
            unsigned(DDSD_CAPS) | unsigned(DDSD_HEIGHT) | unsigned(DDSD_WIDTH) |
            unsigned(DDSD_PIXELFORMAT) | unsigned(DDSD_LINEARSIZE) | unsigned(DDSD_MIPMAPCOUNT);
    header.dwWidth = widths[0];
    header.dwHeight = heights[0];
    header.dwPitchOrLinearSize = dxt_size_total;
    header.dwMipMapCount = mip_count;
    header.sPixelFormat.dwSize = 32;
    header.sPixelFormat.dwFlags = DDPF_FOURCC;

    if (channels == 3) {
        header.sPixelFormat.dwFourCC =
                (unsigned('D') << 0u) | (unsigned('X') << 8u) | (unsigned('T') << 16u) | (unsigned('1') << 24u);
    } else {
        header.sPixelFormat.dwFourCC =
                (unsigned('D') << 0u) | (unsigned('X') << 8u) | (unsigned('T') << 16u) | (unsigned('5') << 24u);
    }

    header.sCaps.dwCaps1 = unsigned(DDSCAPS_TEXTURE) | unsigned(DDSCAPS_MIPMAP);

    std::ofstream out_stream(out_file, std::ios::binary);
    out_stream.write((char *)&header, sizeof(header));

    for (int i = 0; i < mip_count; i++) {
        out_stream.write((char *)dxt_data[i], dxt_size[i]);
        SOIL_free_image_data(dxt_data[i]);
        dxt_data[i] = nullptr;
    }
}

void Write_DDS(const uint8_t *image_data, const int w, const int h, const int channels, const bool flip_y, const char *out_file) {
    // Check if power of two
    const bool store_mipmaps = (unsigned(w) & unsigned(w - 1)) == 0 && (unsigned(h) & unsigned(h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {},
            heights[16] = {};

    mipmaps[0].reset(new uint8_t[w * h * channels]);
    if (flip_y) {
        for (int j = 0; j < h; j++) {
            memcpy(&mipmaps[0][j * w * channels], &image_data[(h - j - 1) * w * channels], w * channels);
        }
    } else {
        memcpy(&mipmaps[0][0], &image_data[0], w * h * channels);
    }
    widths[0] = w;
    heights[0] = h;
    int mip_count;

    if (store_mipmaps) {
        mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels);
    } else {
        mip_count = 1;
    }

    uint8_t *_mipmaps[16];
    for (int i = 0; i < mip_count; i++) {
        _mipmaps[i] = mipmaps[i].get();
    }

    Write_DDS_Mips(_mipmaps, widths, heights, mip_count, channels, out_file);
}

void Write_KTX_DXT(const uint8_t *image_data, const int w, const int h, const int channels, const char *out_file) {
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

    //
    // Compress mip images
    //
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

    //
    // Write out file
    //
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
        auto size = (uint32_t)dxt_size[i];
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

int ConvertToASTC(const uint8_t *image_data, int width, int height, int channels, float bitrate, std::unique_ptr<uint8_t[]> &out_buf);
std::unique_ptr<uint8_t[]> DecodeASTC(const uint8_t *image_data, int data_size, int xdim, int ydim, int width, int height);
//std::unique_ptr<uint8_t[]> Decode_KTX_ASTC(const uint8_t *image_data, int data_size, int &width, int &height);

void Write_KTX_ASTC_Mips(
        const uint8_t * const * mipmaps, const int *widths, const int *heights, const int mip_count,
        const int channels, const char *out_file) {

    int quality = 0;
    if (strstr(out_file, "_norm") || strstr(out_file, "/env/")) {
        quality = 1;
    } else if (strstr(out_file, "lightmaps") || strstr(out_file, "probes_cache")) {
        quality = 2;
    }

    const float bits_per_pixel_sel[] = {
            2.0f, 3.56f, 8.0f
    };

    // Write file
    std::unique_ptr<uint8_t[]> astc_data[16];
    int astc_size[16] = {};
    int astc_size_total = 0;

    for (int i = 0; i < mip_count; i++) {
        astc_size[i] = ConvertToASTC(mipmaps[i], widths[i], heights[i], channels, bits_per_pixel_sel[quality], astc_data[i]);
        astc_size_total += astc_size[i];
    }

    const uint32_t gl_rgb = 0x1907;
    const uint32_t gl_rgba = 0x1908;

    const uint32_t gl_compressed_rgba_astc_4x4_khr = 0x93B0;
    const uint32_t gl_compressed_rgba_astc_6x6_khr = 0x93B4;
    const uint32_t gl_compressed_rgba_astc_8x8_khr = 0x93B7;

    const uint32_t gl_format_sel[] = {
            gl_compressed_rgba_astc_8x8_khr,
            gl_compressed_rgba_astc_6x6_khr,
            gl_compressed_rgba_astc_4x4_khr
    };

    Ren::KTXHeader header = {};
    header.gl_type = 0;
    header.gl_type_size = 1;
    header.gl_format = 0; // should be zero for compressed texture
    header.gl_internal_format = gl_format_sel[quality];

    if (channels == 4) {
        header.gl_base_internal_format = gl_rgba;
    } else {
        header.gl_base_internal_format = gl_rgb;
    }
    header.pixel_width = widths[0];
    header.pixel_height = heights[0];
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
        auto size = (uint32_t)astc_size[i];
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

void Write_KTX_ASTC(const uint8_t *image_data, const int w, const int h, const int channels, const bool flip_y, const char *out_file) {
    // Check if power of two
    const bool store_mipmaps = (unsigned(w) & unsigned(w - 1)) == 0 && (unsigned(h) & unsigned(h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {},
            heights[16] = {};

    mipmaps[0].reset(new uint8_t[w * h * channels]);
    if (flip_y) {
        for (int j = 0; j < h; j++) {
            memcpy(&mipmaps[0][j * w * channels], &image_data[(h - j - 1) * w * channels], w * channels);
        }
    } else {
        memcpy(&mipmaps[0][0], &image_data[0], w * h * channels);
    }
    widths[0] = w;
    heights[0] = h;
    int mip_count;

    if (store_mipmaps) {
        mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels);
    } else {
        mip_count = 1;
    }

    uint8_t *_mipmaps[16];
    for (int i = 0; i < mip_count; i++) {
        _mipmaps[i] = mipmaps[i].get();
    }

    Write_KTX_ASTC_Mips(_mipmaps, widths, heights, mip_count, channels, out_file);
}

int WriteImage(const uint8_t *out_data, int w, int h, int channels, const char *name) {
    int res = 0;
    if (strstr(name, ".tga")) {
        res = SOIL_save_image(name, SOIL_SAVE_TYPE_TGA, w, h, channels, out_data);
    } else if (strstr(name, ".png")) {
        res = SOIL_save_image(name, SOIL_SAVE_TYPE_PNG, w, h, channels, out_data);
    } else if (strstr(name, ".dds")) {
        res = 1;
        Write_DDS(out_data, w, h, channels, true /* flip_y */, name);
    } else if (strstr(name, ".ktx")) {
        res = 1;
        Write_KTX_ASTC(out_data, w, h, channels, true /* flip_y */, name);
    }
    return res;
}

bool CreateFolders(const char *out_file, Ren::ILog *log);


}

void SceneManager::HConvToDDS(assets_context_t &ctx, const char *in_file, const char *out_file) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    auto src_size = (size_t)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    int width, height, channels;
    unsigned char *image_data = SOIL_load_image_from_memory(&src_buf[0], (int)src_size, &width, &height, &channels, 0);

    if (strstr(in_file, "_norm")) {
        // this is normal map, store it in RxGB format
        std::unique_ptr<uint8_t[]> temp_data(new uint8_t[width * height * 4]);
        assert(channels == 3);

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                temp_data[4 * (j * width + i) + 0] = 0;
                temp_data[4 * (j * width + i) + 1] = image_data[3 * (j * width + i) + 1];
                temp_data[4 * (j * width + i) + 2] = image_data[3 * (j * width + i) + 2];
                temp_data[4 * (j * width + i) + 3] = image_data[3 * (j * width + i) + 0];
            }
        }

        Write_DDS(temp_data.get(), width, height, 4, true /* flip_y */, out_file);
        SOIL_free_image_data(image_data);
    } else {
        Write_DDS(image_data, width, height, channels, true /* flip_y */, out_file);
        SOIL_free_image_data(image_data);
    }
}

void SceneManager::HConvToASTC(assets_context_t &ctx, const char *in_file, const char *out_file) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    auto src_size = (size_t)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    int width, height, channels;
    unsigned char *image_data = SOIL_load_image_from_memory(&src_buf[0], (int)src_size, &width, &height, &channels, 0);


    if (strstr(in_file, "_norm")) {
        // this is normal map, store it in RxGB format
        std::unique_ptr<uint8_t[]> temp_data(new uint8_t[width * height * 4]);
        assert(channels == 3);

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                temp_data[4 * (j * width + i) + 0] = 0;
                temp_data[4 * (j * width + i) + 1] = image_data[3 * (j * width + i) + 1];
                temp_data[4 * (j * width + i) + 2] = image_data[3 * (j * width + i) + 2];
                temp_data[4 * (j * width + i) + 3] = image_data[3 * (j * width + i) + 0];
            }
        }

        Write_KTX_ASTC(temp_data.get(), width, height, 4, true /* flip_y */, out_file);
        SOIL_free_image_data(image_data);
    } else {
        Write_KTX_ASTC(image_data, width, height, channels, true /* flip_y */, out_file);
        SOIL_free_image_data(image_data);
    }
}

void SceneManager::HConvHDRToRGBM(assets_context_t &ctx, const char *in_file, const char *out_file) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    int width, height;
    const std::vector<uint8_t> image_rgbe = LoadHDR(in_file, width, height);
    std::unique_ptr<float[]> image_f32 = Ren::ConvertRGBE_to_RGB32F(&image_rgbe[0], width, height);

    std::unique_ptr<float[]> temp(new float[width * 3]);
    for (int j = 0; j < height / 2; j++) {
        int j1 = j, j2 = height - j - 1;
        memcpy(&temp[0], &image_f32[j1 * width * 3], width * 3 * sizeof(float));
        memcpy(&image_f32[j1 * width * 3], &image_f32[j2 * width * 3], width * 3 * sizeof(float));
        memcpy(&image_f32[j2 * width * 3], &temp[0], width * 3 * sizeof(float));
    }

    Write_RGBM(&image_f32[0], width, height, 3, out_file);
}

void SceneManager::HConvImgToDDS(assets_context_t &ctx, const char *in_file, const char *out_file) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    auto src_size = (int)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    int res, mips_count;
    src_stream.read((char *)&res, sizeof(int));
    src_size -= sizeof(int);
    src_stream.read((char *)&mips_count, sizeof(int));
    src_size -= sizeof(int);

    std::unique_ptr<uint8_t[]>
            mipmaps[16],
            compressed_buf(new uint8_t[Net::CalcLZOOutSize(res * res * 4)]);
    uint8_t *_mipmaps[16];
    int widths[16], heights[16];

    for (int i = 0; i < mips_count; i++) {
        const int mip_res = int((unsigned)res >> (unsigned)i);
        const int orig_size = mip_res * mip_res * 4;

        int compressed_size;
        src_stream.read((char *)&compressed_size, sizeof(int));
        src_stream.read((char *)&compressed_buf[0], compressed_size);

        mipmaps[i].reset(new uint8_t[orig_size]);

        const int decompressed_size = Net::DecompressLZO(&compressed_buf[0], compressed_size, &mipmaps[i][0], orig_size);
        assert(decompressed_size == orig_size);

        _mipmaps[i] = mipmaps[i].get();
        widths[i] = heights[i] = mip_res;

        src_size -= sizeof(int);
        src_size -= compressed_size;
    }

    if (src_size != 0) {
        ctx.log->Error("Error reading file %s", in_file);
    }

    Write_DDS_Mips(_mipmaps, widths, heights, mips_count, 4, out_file);
}

void SceneManager::HConvImgToASTC(assets_context_t &ctx, const char *in_file, const char *out_file) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    auto src_size = (int)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    int res, mips_count;
    src_stream.read((char *)&res, sizeof(int));
    src_size -= sizeof(int);
    src_stream.read((char *)&mips_count, sizeof(int));
    src_size -= sizeof(int);

    std::unique_ptr<uint8_t[]>
            mipmaps[16],
            compressed_buf(new uint8_t[Net::CalcLZOOutSize(res * res * 4)]);
    uint8_t *_mipmaps[16];
    int widths[16], heights[16];

    for (int i = 0; i < mips_count; i++) {
        const int mip_res = int((unsigned)res >> (unsigned)i);
        const int orig_size = mip_res * mip_res * 4;

        int compressed_size;
        src_stream.read((char *)&compressed_size, sizeof(int));
        src_stream.read((char *)&compressed_buf[0], compressed_size);

        mipmaps[i].reset(new uint8_t[orig_size]);

        const int decompressed_size = Net::DecompressLZO(&compressed_buf[0], compressed_size, &mipmaps[i][0], orig_size);
        assert(decompressed_size == orig_size);

        _mipmaps[i] = mipmaps[i].get();
        widths[i] = heights[i] = mip_res;

        src_size -= sizeof(int);
        src_size -= compressed_size;
    }

    if (src_size != 0) {
        ctx.log->Error("Error reading file %s", in_file);
    }

    Write_KTX_ASTC_Mips(_mipmaps, widths, heights, mips_count, 4, out_file);
}

bool SceneManager::WriteProbeCache(
        const char *out_folder, const char *scene_name, const ProbeStorage &probes,
        const CompStorage *light_probe_storage, Ren::ILog *log) {
    using namespace SceneManagerInternal;

    const int res = probes.res();
    const int temp_buf_size = 4 * res * res;
    std::unique_ptr<uint8_t[]>
            temp_buf(new uint8_t[temp_buf_size]),
            temp_comp_buf(new uint8_t[Net::CalcLZOOutSize(temp_buf_size)]);

    std::string out_file_name_base;
    out_file_name_base += out_folder;
    if (out_file_name_base.back() != '/') {
        out_file_name_base += '/';
    }
    const size_t prelude_length = out_file_name_base.length();
    out_file_name_base += scene_name;

    if (!CreateFolders(out_file_name_base.c_str(), log)) {
        log->Error("Failed to create folders!");
        return false;
    }

    // write probes
    uint32_t cur_index = light_probe_storage->First();
    while(cur_index != 0xffffffff) {
        const auto *lprobe = (const LightProbe *)light_probe_storage->Get(cur_index);
        assert(lprobe);

        if (lprobe->layer_index != -1) {
            JsArray js_probe_faces;

            std::string out_file_name;

            for (int j = 0; j < 6; j++) {
                const int mipmap_count = probes.max_level() + 1;

                out_file_name.clear();
                out_file_name += out_file_name_base;
                out_file_name += std::to_string(lprobe->layer_index);
                out_file_name += "_";
                out_file_name += std::to_string(j);
                out_file_name += ".img";

                std::ofstream out_file(out_file_name, std::ios::binary);

                out_file.write((char *)&res, 4);
                out_file.write((char *)&mipmap_count, 4);

                for (int k = 0; k < mipmap_count; k++) {
                    const int mip_res = int((unsigned)res >> (unsigned)k);
                    const int buf_size = mip_res * mip_res * 4;

                    if (!probes.GetPixelData(k, lprobe->layer_index, j, buf_size, &temp_buf[0], log)) {
                        log->Error("Failed to read cubemap level %i layer %i face %i", k, lprobe->layer_index, j);
                        return false;
                    }

                    const int comp_size = Net::CompressLZO(&temp_buf[0], buf_size, &temp_comp_buf[0]);
                    out_file.write((char *)&comp_size, sizeof(int));
                    out_file.write((char *)&temp_comp_buf[0], comp_size);
                }
            }
        }

        cur_index = light_probe_storage->Next(cur_index);
    }

    return true;
}

// these are from astc codec
#undef IGNORE
#include <astc/astc_codec_internals.h>
#include <Eng/Gui/BitmapFont.h>

#undef MAX

int astc_main(int argc, char **argv);

void test_inappropriate_extended_precision();
void find_closest_blockdim_2d(float target_bitrate, int *x, int *y, int consider_illegal);

void encode_astc_image(const astc_codec_image *input_image,
                       astc_codec_image *output_image,
                       int xdim, int ydim, int zdim,
                       const error_weighting_params *ewp, astc_decode_mode decode_mode, swizzlepattern swz_encode, swizzlepattern swz_decode, uint8_t *buffer, int pack_and_unpack, int threadcount);

void SceneManager::InitASTCCodec() {
    test_inappropriate_extended_precision();
    prepare_angular_tables();
    build_quantization_mode_table();
}

int SceneManagerInternal::ConvertToASTC(const uint8_t *image_data, int width, int height, int channels, float bitrate, std::unique_ptr<uint8_t[]> &out_buf) {
    const int padding = channels == 4 ? 1 : 0;

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

        ewp.texel_avg_error_limit = (float)pow(0.1f, dblimit_2d * 0.1f) * 65535.0f * 65535.0f;

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
