#include "SceneManager.h"

#include <fstream>
#include <future>

#include <Eng/Utils/Load.h>
#include <Net/Compress.h>
#include <Ren/Utils.h>
#include <Sys/Json.h>
#include <Sys/ThreadPool.h>

#include <Ren/stb/stb_image.h>
#include <Ren/stb/stb_image_write.h>

// faster than std::min/max in debug
#define _MIN(x, y) ((x) < (y) ? (x) : (y))
#define _MAX(x, y) ((x) < (y) ? (y) : (x))

namespace SceneManagerInternal {
void GetTexturesAverageColor(const unsigned char *image_data, int w, int h, int channels, uint8_t out_color[4]) {
    uint32_t sum[4] = {};
    for (int y = 0; y < h; y++) {
        uint32_t line_sum[4] = {};
        for (int x = 0; x < w; x++) {
            for (int i = 0; i < channels; i++) {
                line_sum[i] += image_data[channels * (y * w + x) + i];
            }
        }
        for (int i = 0; i < channels; i++) {
            sum[i] += line_sum[i] / w;
        }
    }

    for (int i = 0; i < channels; i++) {
        out_color[i] = uint8_t(sum[i] / h);
    }
    for (int i = channels; i < 4; i++) {
        out_color[i] = 255;
    }
}

bool GetTexturesAverageColor(const char *in_file, uint8_t out_color[4]) {
    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    const auto src_size = size_t(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    int width, height, channels;
    unsigned char *const image_data = stbi_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);

    GetTexturesAverageColor(image_data, width, height, channels, out_color);

    free(image_data);

    return true;
}

std::unique_ptr<uint8_t[]> ComputeBumpConemap(unsigned char *img_data, int width, int height, int channels,
                                              assets_context_t &ctx) {
    std::unique_ptr<uint8_t[]> _out_conemap(new uint8_t[width * height * 4]);
    // faster access in debug
    uint8_t *out_conemap = &_out_conemap[0];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // height
            out_conemap[4 * (y * width + x) + 0] = img_data[channels * (y * width + x)];

            { // x slope
                int dx;
                if (x == 0) {
                    dx =
                        (int(img_data[channels * (y * width + x + 1)]) - int(img_data[channels * (y * width + x)])) / 2;
                } else if (x == width - 1) {
                    dx =
                        (int(img_data[channels * (y * width + x)]) - int(img_data[channels * (y * width + x - 1)])) / 2;
                } else {
                    dx = (int(img_data[channels * (y * width + x + 1)]) -
                          int(img_data[channels * (y * width + x - 1)])) /
                         2;
                }
                // store in blue channel
                out_conemap[4 * (y * width + x) + 2] = 127 + dx / 2;
            }

            { // y slope
                int dy;
                if (y == 0) {
                    dy = (int(img_data[channels * ((y + 1) * width + x)]) - int(img_data[channels * (y * width + x)])) /
                         2;
                } else if (y == height - 1) {
                    dy = (int(img_data[channels * (y * width + x)]) - int(img_data[channels * ((y - 1) * width + x)])) /
                         2;
                } else {
                    dy = (int(img_data[channels * ((y + 1) * width + x)]) -
                          int(img_data[channels * ((y - 1) * width + x)])) /
                         2;
                }
                // store in alpha channel
                out_conemap[4 * (y * width + x) + 3] = 127 + dy / 2;
            }
        }
    }

    const float MaxRatio = 1.0f;
    const bool Repeat = true;

    const float inv_width = 1.0f / float(width);
    const float inv_height = 1.0f / float(height);

    const int TileSize = 128;

    auto compute_tile = [&](int x_start, int y_start) {
        for (int y = y_start; y < y_start + TileSize; y++) {
            for (int x = x_start; x < x_start + TileSize; x++) {
                const float h = out_conemap[4 * (y * width + x) + 0] / 255.0f;
                const float dhdx = +(out_conemap[4 * (y * width + x) + 2] / 255.0f - 0.5f) * float(width);
                const float dhdy = -(out_conemap[4 * (y * width + x) + 3] / 255.0f - 0.5f) * float(height);

                float min_ratio2 = MaxRatio * MaxRatio;

                for (int rad = 1; (rad * rad <= 1.1f * 1.1f * (1.0f - h) * (1.0f - h) * min_ratio2 * width * height) &&
                                  (rad <= 1.1f * (1.0f - h) * width) && (rad <= 1.1f * (1.0f - h) * height);
                     rad++) {
                    { // west
                        int x1 = x - rad;
                        while (Repeat && x1 < 0) {
                            x1 += width;
                        }
                        if (x1 >= 0) {
                            const float delx = -rad * inv_width;
                            const int y1 = _MAX(y - rad + 1, 0);
                            const int y2 = _MIN(y + rad - 1, height - 1);
                            for (int dy = y1; dy <= y2; dy++) {
                                const float dely = (dy - y) * inv_height;
                                const float r2 = delx * delx + dely * dely;
                                const float h2 = out_conemap[4 * (dy * width + x1)] / 255.0f - h;
                                if ((h2 > 0.0f) && (h2 * h2 * min_ratio2 > r2)) {
                                    min_ratio2 = r2 / (h2 * h2);
                                }
                            }
                        }
                    }

                    { // east
                        int x2 = x + rad;
                        while (Repeat && x2 >= width) {
                            x2 -= width;
                        }
                        if (x2 < width) {
                            const float delx = rad * inv_width;
                            const int y1 = _MAX(y - rad + 1, 0);
                            const int y2 = _MIN(y + rad - 1, height - 1);
                            for (int dy = y1; dy <= y2; dy++) {
                                const float dely = (dy - y) * inv_height;
                                const float r2 = delx * delx + dely * dely;
                                const float h2 = out_conemap[4 * (dy * width + x2)] / 255.0f - h;
                                if ((h2 > 0.0f) && (h2 * h2 * min_ratio2 > r2)) {
                                    min_ratio2 = r2 / (h2 * h2);
                                }
                            }
                        }
                    }

                    { // north
                        int y1 = y - rad;
                        while (Repeat && y1 < 0) {
                            y1 += height;
                        }
                        if (y1 >= 0) {
                            const float dely = -rad * inv_height;
                            const int x1 = _MAX(x - rad, 0);
                            const int x2 = _MIN(x + rad, width - 1);
                            for (int dx = x1; dx <= x2; dx++) {
                                const float delx = (dx - x) * inv_width;
                                const float r2 = delx * delx + dely * dely;
                                const float h2 = out_conemap[4 * (y1 * width + dx)] / 255.0f - h;
                                if ((h2 > 0.0f) && (h2 * h2 * min_ratio2 > r2)) {
                                    min_ratio2 = r2 / (h2 * h2);
                                }
                            }
                        }
                    }

                    { // south
                        int y2 = y + rad;
                        while (Repeat && y2 >= height) {
                            y2 -= height;
                        }
                        if (y2 < height) {
                            const float dely = rad * inv_height;
                            const int x1 = _MAX(x - rad, 0);
                            const int x2 = _MIN(x + rad, width - 1);
                            for (int dx = x1; dx <= x2; dx++) {
                                const float delx = (dx - x) * inv_width;
                                const float r2 = delx * delx + dely * dely;
                                const float h2 = out_conemap[4 * (y2 * width + dx)] / 255.0f - h;
                                if ((h2 > 0.0f) && (h2 * h2 * min_ratio2 > r2)) {
                                    min_ratio2 = r2 / (h2 * h2);
                                }
                            }
                        }
                    }
                }

                float ratio = std::sqrt(min_ratio2);
                ratio = std::sqrt(ratio / MaxRatio);

                // store in green channel
                out_conemap[4 * (y * width + x) + 1] = uint8_t(_MAX(255.0f * ratio + 0.5f, 1.0f));
            }
        }
    };

    std::vector<std::future<void>> futures;

    int counter = 0;
    for (int y = 0; y < height; y += TileSize) {
        for (int x = 0; x < width; x += TileSize) {
            if (ctx.p_threads) {
                futures.emplace_back(ctx.p_threads->Enqueue(std::bind(compute_tile, x, y)));
            } else {
                compute_tile(x, y);
                ctx.log->Info("Computing conemap %i%%",
                              int(100.0f * float(++counter) / float((width * height) / (TileSize * TileSize))));
            }
        }
    }

    for (auto &f : futures) {
        f.wait();
        ctx.log->Info("Computing conemap %i%%", int(100.0f * float(++counter) / float(futures.size())));
    }

    return _out_conemap;
}

std::unique_ptr<uint8_t[]> ComputeBumpNormalmap(unsigned char *img_data, int width, int height, int channels,
                                                Ren::ILog *log) {
    std::unique_ptr<uint8_t[]> _out_normalmap(new uint8_t[width * height * 3]);
    // faster access in debug
    uint8_t *out_normalmap = &_out_normalmap[0];

    for (int y = 0; y < height; y++) {
        const int y_top = (y - 1 >= 0) ? (y - 1) : (height - 1);
        const int y_bottom = (y + 1 < height) ? (y + 1) : 0;

        for (int x = 0; x < width; x++) {
            const int x_left = (x - 1 >= 0) ? (x - 1) : (width - 1);
            const int x_right = (x + 1 < width) ? (x + 1) : 0;

            const float h = img_data[(y * width + x) * channels + 0] / 255.0f;
            const float h_top_left = img_data[(y_top * width + x_left) * channels + 0] / 255.0f;
            const float h_top = img_data[(y_top * width + x) * channels + 0] / 255.0f;
            const float h_top_right = img_data[(y_top * width + x_right) * channels + 0] / 255.0f;
            const float h_left = img_data[(y * width + x_left) * channels + 0] / 255.0f;
            const float h_right = img_data[(y * width + x_right) * channels + 0] / 255.0f;
            const float h_bot_left = img_data[(y_bottom * width + x_left) * channels + 0] / 255.0f;
            const float h_bot = img_data[(y_bottom * width + x) * channels + 0] / 255.0f;
            const float h_bot_right = img_data[(y_bottom * width + x_right) * channels + 0] / 255.0f;

            Ren::Vec3f n;
            n[0] = (h_top_right + 2.0f * h_right + h_bot_right) - (h_top_left + 2.0f * h_left + h_bot_left);
            n[1] = (h_bot_left + 2.0f * h_bot + h_bot_right) - (h_top_left + 2.0f * h_top + h_top_right);
            n[2] = 0.25f;

            n = Normalize(n);

            n = 0.5f * n + Ren::Vec3f{0.5f};
            n *= 255.0f;

            out_normalmap[(y * width + x) * 3 + 0] = uint8_t(n[0]);
            out_normalmap[(y * width + x) * 3 + 1] = uint8_t(n[1]);
            out_normalmap[(y * width + x) * 3 + 2] = uint8_t(n[2]);
        }
    }

    return _out_normalmap;
}

int ComputeBumpQuadtree(unsigned char *img_data, int channels, Ren::ILog *log, std::unique_ptr<uint8_t[]> mipmaps[16],
                        int widths[16], int heights[16]) {
    mipmaps[0].reset(new uint8_t[4 * widths[0] * heights[0]]);
    for (int y = 0; y < heights[0]; y++) {
        for (int x = 0; x < widths[0]; x++) {
            const uint8_t h = img_data[(y * widths[0] + x) * channels];
            mipmaps[0][4 * (y * widths[0] + x) + 0] = 0;
            mipmaps[0][4 * (y * widths[0] + x) + 1] = 255 - h;
            mipmaps[0][4 * (y * widths[0] + x) + 2] = 0;
            mipmaps[0][4 * (y * widths[0] + x) + 3] = 0;
        }
    }

    const Ren::eMipOp ops[4] = {Ren::eMipOp::Zero, Ren::eMipOp::MinBilinear, Ren::eMipOp::Zero, Ren::eMipOp::Skip};
    return Ren::InitMipMaps(mipmaps, widths, heights, 4, ops);
}

int WriteImage(const uint8_t *out_data, int w, int h, int channels, bool flip_y, bool is_rgbm, const char *name);

bool Write_RGBE(const Ray::pixel_color_t *out_data, int w, int h, const char *name) {
    std::unique_ptr<uint8_t[]> u8_data = Ren::ConvertRGB32F_to_RGBE(&out_data[0].r, w, h, 4);
    return WriteImage(&u8_data[0], w, h, 4, false /* flip_y */, false /* is_rgbm */, name) == 1;
}

bool Write_RGB(const Ray::pixel_color_t *out_data, int w, int h, const char *name) {
    std::vector<uint8_t> u8_data(w * h * 3);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const Ray::pixel_color_t &p = out_data[y * w + x];

            u8_data[(y * w + x) * 3 + 0] = uint8_t(std::min(int(p.r * 255), 255));
            u8_data[(y * w + x) * 3 + 1] = uint8_t(std::min(int(p.g * 255), 255));
            u8_data[(y * w + x) * 3 + 2] = uint8_t(std::min(int(p.b * 255), 255));
        }
    }

    return WriteImage(&u8_data[0], w, h, 3, false /* flip_y */, false /* is_rgbm */, name) == 1;
}

bool Write_RGBM(const float *out_data, const int w, const int h, const int channels, const bool flip_y,
                const char *name) {
    const std::unique_ptr<uint8_t[]> u8_data = Ren::ConvertRGB32F_to_RGBM(out_data, w, h, channels);
    return WriteImage(&u8_data[0], w, h, 4, flip_y, true /* is_rgbm */, name) == 1;
}

bool Write_DDS_Mips(const uint8_t *const *mipmaps, const int *widths, const int *heights, const int mip_count,
                    const int channels, const char *out_file) {
    //
    // Compress mip images
    //
    std::unique_ptr<uint8_t[]> dxt_data[16];
    int dxt_size[16] = {}, dxt_size_total = 0;

    const bool use_YCoCg = strstr(out_file, "_diff."); // Store diffuse as YCoCg
    const bool use_DXT5 = (channels == 4) || use_YCoCg;

    for (int i = 0; i < mip_count; i++) {
        if (channels == 3) {
            if (use_YCoCg) {
                dxt_size[i] = Ren::GetRequiredMemory_DXT5(widths[i], heights[i]);
                dxt_data[i].reset(new uint8_t[dxt_size[i]]);
                auto temp_YCoCg = Ren::ConvertRGB_to_CoCgxY(mipmaps[i], widths[i], heights[i]);
                Ren::CompressImage_DXT5<true /* Is_YCoCg */>(temp_YCoCg.get(), widths[i], heights[i],
                                                             dxt_data[i].get());
            } else {
                dxt_size[i] = Ren::GetRequiredMemory_DXT1(widths[i], heights[i]);
                dxt_data[i].reset(new uint8_t[dxt_size[i]]);
                Ren::CompressImage_DXT1<3>(mipmaps[i], widths[i], heights[i], dxt_data[i].get());
            }
        } else {
            dxt_size[i] = Ren::GetRequiredMemory_DXT5(widths[i], heights[i]);
            dxt_data[i].reset(new uint8_t[dxt_size[i]]);
            Ren::CompressImage_DXT5(mipmaps[i], widths[i], heights[i], dxt_data[i].get());
        }
        dxt_size_total += dxt_size[i];
    }

    //
    // Write out file
    //
    Ren::DDSHeader header = {};
    header.dwMagic = (unsigned('D') << 0u) | (unsigned('D') << 8u) | (unsigned('S') << 16u) | (unsigned(' ') << 24u);
    header.dwSize = 124;
    header.dwFlags = unsigned(DDSD_CAPS) | unsigned(DDSD_HEIGHT) | unsigned(DDSD_WIDTH) | unsigned(DDSD_PIXELFORMAT) |
                     unsigned(DDSD_LINEARSIZE) | unsigned(DDSD_MIPMAPCOUNT);
    header.dwWidth = widths[0];
    header.dwHeight = heights[0];
    header.dwPitchOrLinearSize = dxt_size_total;
    header.dwMipMapCount = mip_count;
    header.sPixelFormat.dwSize = 32;
    header.sPixelFormat.dwFlags = DDPF_FOURCC;

    if (!use_DXT5) {
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
        out_stream.write((char *)dxt_data[i].get(), dxt_size[i]);
    }

    for (int i = 0; i < mip_count; i++) {
        dxt_data[i].reset();
    }

    return out_stream.good();
}

bool Write_DDS(const uint8_t *image_data, const int w, const int h, const int channels, const bool flip_y,
               const bool is_rgbm, const char *out_file, uint8_t out_color[4]) {
    // Check if resolution is power of two
    const bool store_mipmaps = (unsigned(w) & unsigned(w - 1)) == 0 && (unsigned(h) & unsigned(h - 1)) == 0;
    const bool use_YCoCg = strstr(out_file, "_diff."); // Store diffuse as YCoCg

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {}, heights[16] = {};

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
        if (is_rgbm) {
            assert(channels == 4);
            mip_count = Ren::InitMipMapsRGBM(mipmaps, widths, heights);
        } else {
            const Ren::eMipOp ops[4] = {Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg};
            mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels, ops);
        }

        if (out_color) {
            // Use color of the last mip level
            memcpy(out_color, &mipmaps[mip_count - 1][0], channels);
        }
    } else {
        mip_count = 1;

        if (out_color) {
            GetTexturesAverageColor(image_data, w, h, channels, out_color);
        }
    }

    if (out_color && channels == 3) {
        out_color[3] = 255;
    }

    if (use_YCoCg && out_color) {
        uint8_t YCoCg[3];
        Ren::ConvertRGB_to_YCoCg(out_color, YCoCg);

        out_color[0] = YCoCg[1];
        out_color[1] = YCoCg[2];
        out_color[2] = 0;
        out_color[3] = YCoCg[0];
    }

    uint8_t *_mipmaps[16];
    for (int i = 0; i < mip_count; i++) {
        _mipmaps[i] = mipmaps[i].get();
    }

    return Write_DDS_Mips(_mipmaps, widths, heights, mip_count, channels, out_file);
}

bool Write_KTX_DXT(const uint8_t *image_data, const int w, const int h, const int channels, const bool is_rgbm,
                   const char *out_file) {
    // Check if power of two
    bool store_mipmaps = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {}, heights[16] = {};

    mipmaps[0].reset(new uint8_t[w * h * channels]);
    // mirror by y (????)
    for (int j = 0; j < h; j++) {
        memcpy(&mipmaps[0][j * w * channels], &image_data[(h - j - 1) * w * channels], w * channels);
    }
    widths[0] = w;
    heights[0] = h;
    int mip_count;

    if (store_mipmaps) {
        if (is_rgbm) {
            assert(channels == 4);
            mip_count = Ren::InitMipMapsRGBM(mipmaps, widths, heights);
        } else {
            const Ren::eMipOp ops[4] = {Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg};
            mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels, ops);
        }
    } else {
        mip_count = 1;
    }

    //
    // Compress mip images
    //
    std::unique_ptr<uint8_t[]> dxt_data[16];
    int dxt_size[16] = {};
    int dxt_size_total = 0;

    for (int i = 0; i < mip_count; i++) {
        if (channels == 3) {
            dxt_size[i] = Ren::GetRequiredMemory_DXT1(widths[i], heights[i]);
            dxt_data[i].reset(new uint8_t[dxt_size[i]]);
            Ren::CompressImage_DXT1<3>(mipmaps[i].get(), widths[i], heights[i], dxt_data[i].get());
        } else if (channels == 4) {
            dxt_size[i] = Ren::GetRequiredMemory_DXT5(widths[i], heights[i]);
            dxt_data[i].reset(new uint8_t[dxt_size[i]]);
            Ren::CompressImage_DXT5(mipmaps[i].get(), widths[i], heights[i], dxt_data[i].get());
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
        out_stream.write((char *)dxt_data[i].get(), size);
        file_offset += size;

        uint32_t pad = (file_offset % 4) ? (4 - (file_offset % 4)) : 0;
        while (pad) {
            const uint8_t zero_byte = 0;
            out_stream.write((char *)&zero_byte, 1);
            pad--;
        }
    }

    return out_stream.good();
}

int ConvertToASTC(const uint8_t *image_data, int width, int height, int channels, float bitrate,
                  std::unique_ptr<uint8_t[]> &out_buf);
std::unique_ptr<uint8_t[]> DecodeASTC(const uint8_t *image_data, int data_size, int xdim, int ydim, int width,
                                      int height);
std::unique_ptr<uint8_t[]> Decode_KTX_ASTC(const uint8_t *image_data, int data_size, int &width, int &height);

bool Write_KTX_ASTC_Mips(const uint8_t *const *mipmaps, const int *widths, const int *heights, const int mip_count,
                         const int channels, const char *out_file) {

    int quality = 0;
    if (strstr(out_file, "_norm") || strstr(out_file, "/env/") || strstr(out_file, "\\env\\")) {
        quality = 1;
    } else if (strstr(out_file, "lightmaps") || strstr(out_file, "probes_cache")) {
        quality = 2;
    }

    const float bits_per_pixel_sel[] = {2.0f, 3.56f, 8.0f};

    // Write file
    std::unique_ptr<uint8_t[]> astc_data[16];
    int astc_size[16] = {};
    int astc_size_total = 0;

    for (int i = 0; i < mip_count; i++) {
        astc_size[i] =
            ConvertToASTC(mipmaps[i], widths[i], heights[i], channels, bits_per_pixel_sel[quality], astc_data[i]);
        astc_size_total += astc_size[i];
    }

    const uint32_t gl_rgb = 0x1907;
    const uint32_t gl_rgba = 0x1908;

    const uint32_t gl_compressed_rgba_astc_4x4_khr = 0x93B0;
    const uint32_t gl_compressed_rgba_astc_6x6_khr = 0x93B4;
    const uint32_t gl_compressed_rgba_astc_8x8_khr = 0x93B7;

    const uint32_t gl_format_sel[] = {gl_compressed_rgba_astc_8x8_khr, gl_compressed_rgba_astc_6x6_khr,
                                      gl_compressed_rgba_astc_4x4_khr};

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
        const auto size = uint32_t(astc_size[i]);
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

    return out_stream.good();
}

bool Write_KTX_ASTC(const uint8_t *image_data, const int w, const int h, const int channels, const bool flip_y,
                    const bool is_rgbm, const char *out_file) {
    // Check if power of two
    const bool store_mipmaps = (unsigned(w) & unsigned(w - 1)) == 0 && (unsigned(h) & unsigned(h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {}, heights[16] = {};

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
        if (is_rgbm) {
            assert(channels == 4);
            mip_count = Ren::InitMipMapsRGBM(mipmaps, widths, heights);
        } else {
            const Ren::eMipOp ops[4] = {Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg};
            mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels, ops);
        }
    } else {
        mip_count = 1;
    }

    uint8_t *_mipmaps[16];
    for (int i = 0; i < mip_count; i++) {
        _mipmaps[i] = mipmaps[i].get();
    }

    return Write_KTX_ASTC_Mips(_mipmaps, widths, heights, mip_count, channels, out_file);
}

int WriteImage(const uint8_t *out_data, const int w, const int h, const int channels, const bool flip_y,
               const bool is_rgbm, const char *name) {
    int res = 0;
    if (strstr(name, ".tga") || strstr(name, ".png")) {
        // TODO: check if negative stride can be used instead of this
        std::unique_ptr<uint8_t[]> temp_data;
        if (flip_y) {
            temp_data.reset(new uint8_t[w * h * channels]);
            for (int j = 0; j < h; j++) {
                memcpy(&temp_data[j * w * channels], &out_data[(h - j - 1) * w * channels], w * channels);
            }
            out_data = &temp_data[0];
        }

        if (strstr(name, ".tga")) {
            res = stbi_write_tga(name, w, h, channels, out_data);
        } else if (strstr(name, ".png")) {
            res = stbi_write_png(name, w, h, channels, out_data, 0);
        }
    } else if (strstr(name, ".dds")) {
        res = 1;
        Write_DDS(out_data, w, h, channels, flip_y, is_rgbm, name, nullptr);
    } else if (strstr(name, ".ktx")) {
        res = 1;
        Write_KTX_ASTC(out_data, w, h, channels, flip_y, is_rgbm, name);
    }
    return res;
}

bool CreateFolders(const char *out_file, Ren::ILog *log);

extern bool g_astc_initialized;
} // namespace SceneManagerInternal

bool SceneManager::HConvToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                              Ren::SmallVectorImpl<std::string> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    const auto src_size = size_t(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    int width, height, channels;
    unsigned char *const image_data = stbi_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);

    uint8_t average_color[4] = {};

    bool res = true;
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

        res &= Write_DDS(temp_data.get(), width, height, 4, false /* flip_y */, false /* is_rgbm */, out_file,
                         average_color);
    } else if (strstr(in_file, "_bump")) {
        if (channels != 1) {
            ctx.log->Info("Bump map has too many channels (%i)", channels);
        }

        // prepare data for cone stepping
        // std::unique_ptr<uint8_t[]> conemap_data =
        //    ComputeBumpConemap(image_data, width, height, channels, ctx);

        // prepare data for quad tree displacement
        std::unique_ptr<uint8_t[]> mipmaps[16];
        int widths[16], heights[16];
        widths[0] = width;
        heights[0] = height;

        const int mip_count = ComputeBumpQuadtree(image_data, channels, ctx.log, mipmaps, widths, heights);

        // combine data into one image
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t *rgba = &mipmaps[0][4 * (y * height + x)];
                // store cone map in alpha channel
                // rgba[3] = conemap_data[4 * (y * height + x) + 1];
            }
        }

        // WriteImage(&mipmaps[0][0], width, height, 4, true, false,
        //    "assets_pc/textures/pom_test_bump.uncompressed.png");

        // apply padding to account for compression artifacts
        for (int i = 1; i < mip_count; i++) {
            for (int y = 0; y < heights[i]; y++) {
                for (int x = 0; x < widths[i]; x++) {
                    uint8_t *rgba = &mipmaps[i][4 * (y * heights[i] + x)];
                    if (rgba[1] > i) {
                        rgba[1] -= i;
                    } else {
                        rgba[1] = 0;
                    }
                }
            }
        }

        memcpy(average_color, &mipmaps[mip_count - 1][0], channels);
        if (channels == 3) {
            average_color[3] = 255;
        }

        uint8_t *_mipmaps[16];
        for (int i = 0; i < mip_count; i++) {
            _mipmaps[i] = mipmaps[i].get();
        }
        res &= Write_DDS_Mips(_mipmaps, widths, heights, mip_count, 4, out_file);
    } else {
        const bool is_rgbm = channels == 4 && strstr(in_file, "lightmaps") != nullptr;
        res &= Write_DDS(image_data, width, height, channels, false /* flip_y */, is_rgbm /* is_rgbm */, out_file,
                         average_color);
    }
    free(image_data);

    ctx.cache->WriteTextureAverage(in_file, average_color);

    return res;
}

bool SceneManager::HConvToASTC(assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    auto src_size = (size_t)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    int width, height, channels;
    unsigned char *const image_data = stbi_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);

    bool res = true;
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

        res &= Write_KTX_ASTC(temp_data.get(), width, height, 4, false /* flip_y */, false /* is_rgbm */, out_file);
    } else if (strstr(in_file, "_bump")) {
        if (channels != 1) {
            ctx.log->Info("Bump map has too many channels (%i)", channels);
        }

        // prepare data for cone stepping
        // std::unique_ptr<uint8_t[]> conemap_data =
        //    ComputeBumpConemap(image_data, width, height, channels, ctx);

        // prepare data for quad tree displacement
        std::unique_ptr<uint8_t[]> mipmaps[16];
        int widths[16], heights[16];
        widths[0] = width;
        heights[0] = height;

        const int mip_count = ComputeBumpQuadtree(image_data, channels, ctx.log, mipmaps, widths, heights);

        // combine data into one image
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t *rgba = &mipmaps[0][4 * (y * height + x)];
                // store cone map in alpha channel
                // rgba[3] = conemap_data[4 * (y * height + x) + 1];
            }
        }

        // apply padding to account for compression artifacts
        for (int i = 1; i < mip_count; i++) {
            for (int y = 0; y < heights[i]; y++) {
                for (int x = 0; x < widths[i]; x++) {
                    uint8_t *rgba = &mipmaps[i][4 * (y * heights[i] + x)];
                    if (rgba[1] > i) {
                        rgba[1] -= i;
                    } else {
                        rgba[1] = 0;
                    }
                }
            }
        }

        uint8_t *_mipmaps[16];
        for (int i = 0; i < mip_count; i++) {
            _mipmaps[i] = mipmaps[i].get();
        }
        res &= Write_KTX_ASTC_Mips(_mipmaps, widths, heights, mip_count, 4, out_file);
    } else {
        res &= Write_KTX_ASTC(image_data, width, height, channels, false /* flip_y */, false /* is_rgbm */, out_file);
    }

    free(image_data);

    return res;
}

bool SceneManager::HConvHDRToRGBM(assets_context_t &ctx, const char *in_file, const char *out_file,
                                  Ren::SmallVectorImpl<std::string> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    int width, height;
    const std::vector<uint8_t> image_rgbe = LoadHDR(in_file, width, height);
    const std::unique_ptr<float[]> image_f32 = Ren::ConvertRGBE_to_RGB32F(&image_rgbe[0], width, height);

    return Write_RGBM(&image_f32[0], width, height, 3, false /* flip_y */, out_file);
}

bool SceneManager::HConvImgToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                                 Ren::SmallVectorImpl<std::string> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    auto src_size = int(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    int res, mips_count;
    src_stream.read((char *)&res, sizeof(int));
    src_size -= sizeof(int);
    src_stream.read((char *)&mips_count, sizeof(int));
    src_size -= sizeof(int);

    std::unique_ptr<uint8_t[]> mipmaps[16], compressed_buf(new uint8_t[Net::CalcLZOOutSize(res * res * 4)]);
    uint8_t *_mipmaps[16];
    int widths[16], heights[16];

    for (int i = 0; i < mips_count; i++) {
        const int mip_res = int(unsigned(res) >> unsigned(i));
        const int orig_size = mip_res * mip_res * 4;

        int compressed_size;
        src_stream.read((char *)&compressed_size, sizeof(int));
        src_stream.read((char *)&compressed_buf[0], compressed_size);

        mipmaps[i].reset(new uint8_t[orig_size]);

        const int decompressed_size =
            Net::DecompressLZO(&compressed_buf[0], compressed_size, &mipmaps[i][0], orig_size);
        assert(decompressed_size == orig_size);

        _mipmaps[i] = mipmaps[i].get();
        widths[i] = heights[i] = mip_res;

        src_size -= sizeof(int);
        src_size -= compressed_size;
    }

    if (src_size != 0) {
        ctx.log->Error("Error reading file %s", in_file);
        return false;
    }

    return Write_DDS_Mips(_mipmaps, widths, heights, mips_count, 4, out_file);
}

bool SceneManager::HConvImgToASTC(assets_context_t &ctx, const char *in_file, const char *out_file,
                                  Ren::SmallVectorImpl<std::string> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    auto src_size = int(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    int res, mips_count;
    src_stream.read((char *)&res, sizeof(int));
    src_size -= sizeof(int);
    src_stream.read((char *)&mips_count, sizeof(int));
    src_size -= sizeof(int);

    std::unique_ptr<uint8_t[]> mipmaps[16], compressed_buf(new uint8_t[Net::CalcLZOOutSize(res * res * 4)]);
    uint8_t *_mipmaps[16];
    int widths[16], heights[16];

    for (int i = 0; i < mips_count; i++) {
        const int mip_res = int(unsigned(res) >> unsigned(i));
        const int orig_size = mip_res * mip_res * 4;

        int compressed_size;
        src_stream.read((char *)&compressed_size, sizeof(int));
        src_stream.read((char *)&compressed_buf[0], compressed_size);

        mipmaps[i].reset(new uint8_t[orig_size]);

        const int decompressed_size =
            Net::DecompressLZO(&compressed_buf[0], compressed_size, &mipmaps[i][0], orig_size);
        assert(decompressed_size == orig_size);

        _mipmaps[i] = mipmaps[i].get();
        widths[i] = heights[i] = mip_res;

        src_size -= sizeof(int);
        src_size -= compressed_size;
    }

    if (src_size != 0) {
        ctx.log->Error("Error reading file %s", in_file);
        return false;
    }

    return Write_KTX_ASTC_Mips(_mipmaps, widths, heights, mips_count, 4, out_file);
}

bool SceneManager::WriteProbeCache(const char *out_folder, const char *scene_name, const Ren::ProbeStorage &probes,
                                   const CompStorage *light_probe_storage, Ren::ILog *log) {
    using namespace SceneManagerInternal;

    const int res = probes.res();
    const int temp_buf_size = 4 * res * res;
    std::unique_ptr<uint8_t[]> temp_buf(new uint8_t[temp_buf_size]),
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
    while (cur_index != 0xffffffff) {
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
                    const int mip_res = int(unsigned(res) >> unsigned(k));
                    const int buf_size = mip_res * mip_res * 4;

                    if (!probes.GetPixelData(k, lprobe->layer_index, j, buf_size, &temp_buf[0], log)) {
                        log->Error("Failed to read cubemap level %i layer %i face %i", k, lprobe->layer_index, j);
                        return false;
                    }

                    const int comp_size = Net::CompressLZO(&temp_buf[0], buf_size, &temp_comp_buf[0]);
                    out_file.write((char *)&comp_size, sizeof(int));
                    out_file.write((char *)&temp_comp_buf[0], comp_size);
                }

                if (!out_file.good()) {
                    log->Error("Failed to write %s", out_file_name.c_str());
                    return false;
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

#undef MAX

int astc_main(int argc, char **argv);

void test_inappropriate_extended_precision();
void find_closest_blockdim_2d(float target_bitrate, int *x, int *y, int consider_illegal);

void encode_astc_image(const astc_codec_image *input_image, astc_codec_image *output_image, int xdim, int ydim,
                       int zdim, const error_weighting_params *ewp, astc_decode_mode decode_mode,
                       swizzlepattern swz_encode, swizzlepattern swz_decode, uint8_t *buffer, int pack_and_unpack,
                       int threadcount);

void SceneManager::InitASTCCodec() {
    test_inappropriate_extended_precision();
    prepare_angular_tables();
    build_quantization_mode_table();
}

int SceneManagerInternal::ConvertToASTC(const uint8_t *image_data, int width, int height, int channels, float bitrate,
                                        std::unique_ptr<uint8_t[]> &out_buf) {
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

        const float log10_texels_2d = (std::log(float(xdim * ydim)) / std::log(10.0f));

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

        const float max_color_comp_weight = std::max(std::max(ewp.rgba_weights[0], ewp.rgba_weights[1]),
                                                     std::max(ewp.rgba_weights[2], ewp.rgba_weights[3]));
        ewp.rgba_weights[0] = std::max(ewp.rgba_weights[0], max_color_comp_weight / 1000.0f);
        ewp.rgba_weights[1] = std::max(ewp.rgba_weights[1], max_color_comp_weight / 1000.0f);
        ewp.rgba_weights[2] = std::max(ewp.rgba_weights[2], max_color_comp_weight / 1000.0f);
        ewp.rgba_weights[3] = std::max(ewp.rgba_weights[3], max_color_comp_weight / 1000.0f);

        if (channels == 4) {
            ewp.enable_rgb_scale_with_alpha = 1;
            ewp.alpha_radius = 1;
        }

        ewp.texel_avg_error_limit = float(std::pow(0.1f, dblimit_2d * 0.1f)) * 65535.0f * 65535.0f;

        expand_block_artifact_suppression(xdim, ydim, 1, &ewp);

        swizzlepattern swz_encode = {0, 1, 2, 3};

        // int padding = std::max(ewp.mean_stdev_radius, ewp.alpha_radius);

        if (channels == 4 /*ewp.rgb_mean_weight != 0.0f || ewp.rgb_stdev_weight != 0.0f || ewp.alpha_mean_weight != 0.0f || ewp.alpha_stdev_weight != 0.0f*/) {
            compute_averages_and_variances(src_image, ewp.rgb_power, ewp.alpha_power, ewp.mean_stdev_radius,
                                           ewp.alpha_radius, swz_encode);
        }

        const int xsize = src_image->xsize;
        const int ysize = src_image->ysize;

        const int xblocks = (xsize + xdim - 1) / xdim;
        const int yblocks = (ysize + ydim - 1) / ydim;
        const int zblocks = 1;

        buf_size = xblocks * yblocks * zblocks * 16;
        out_buf.reset(new uint8_t[buf_size]);

        encode_astc_image(src_image, nullptr, xdim, ydim, 1, &ewp, DECODE_LDR, swz_encode, swz_encode, &out_buf[0], 0,
                          8);
    }

    destroy_image(src_image);

    return buf_size;
}

std::unique_ptr<uint8_t[]> SceneManagerInternal::DecodeASTC(const uint8_t *image_data, int data_size, int xdim,
                                                            int ydim, int width, int height) {
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

    swizzlepattern swz_decode = {0, 1, 2, 3};

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

std::unique_ptr<uint8_t[]> SceneManagerInternal::Decode_KTX_ASTC(const uint8_t *image_data, int data_size, int &width,
                                                                 int &height) {
    Ren::KTXHeader header;
    memcpy(&header, &image_data[0], sizeof(Ren::KTXHeader));

    width = int(header.pixel_width);
    height = int(header.pixel_height);

    int data_offset = sizeof(Ren::KTXHeader);

    { // Decode first mip level
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

#undef _MIN
#undef _MAX