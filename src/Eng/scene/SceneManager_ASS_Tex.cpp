#include "SceneManager.h"

#include <filesystem>
#include <fstream>
#include <future>

#include <Net/Compress.h>
#include <Ren/Utils.h>
#include <Sys/Json.h>
#include <Sys/ScopeExit.h>
#include <Sys/ThreadPool.h>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
extern "C" {
#include <stb/stbi_DDS.h>
}

#include "../utils/Load.h"

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

std::unique_ptr<uint8_t[]> ComputeBumpConemap(const unsigned char *img_data, int width, int height, int channels,
                                              Eng::assets_context_t &ctx) {
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
                // const float dhdx = +(out_conemap[4 * (y * width + x) + 2] / 255.0f - 0.5f) * float(width);
                // const float dhdy = -(out_conemap[4 * (y * width + x) + 3] / 255.0f - 0.5f) * float(height);

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
                            const int y1 = std::max(y - rad + 1, 0);
                            const int y2 = std::min(y + rad - 1, height - 1);
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
                            const int y1 = std::max(y - rad + 1, 0);
                            const int y2 = std::min(y + rad - 1, height - 1);
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
                            const int x1 = std::max(x - rad, 0);
                            const int x2 = std::min(x + rad, width - 1);
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
                            const int x1 = std::max(x - rad, 0);
                            const int x2 = std::min(x + rad, width - 1);
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
                out_conemap[4 * (y * width + x) + 1] = uint8_t(std::max(255.0f * ratio + 0.5f, 1.0f));
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

std::unique_ptr<uint8_t[]> ComputeBumpNormalmap(const unsigned char *img_data, int width, int height, int channels,
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

            // const float h = img_data[(y * width + x) * channels + 0] / 255.0f;
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
    mipmaps[0] = std::make_unique<uint8_t[]>(4 * widths[0] * heights[0]);
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

#if 0
bool Write_RGBE(const Ray::color_rgba_t *out_data, int w, int h, const char *name) {
    std::unique_ptr<uint8_t[]> u8_data = Ren::ConvertRGB32F_to_RGBE(&out_data[0].v[0], w, h, 4);
    return WriteImage(&u8_data[0], w, h, 4, false /* flip_y */, false /* is_rgbm */, name) == 1;
}

bool Write_RGB(const Ray::color_rgba_t *out_data, int w, int h, const char *name) {
    std::vector<uint8_t> u8_data(w * h * 3);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const Ray::color_rgba_t &p = out_data[y * w + x];

            u8_data[(y * w + x) * 3 + 0] = uint8_t(std::min(int(p.v[0] * 255), 255));
            u8_data[(y * w + x) * 3 + 1] = uint8_t(std::min(int(p.v[1] * 255), 255));
            u8_data[(y * w + x) * 3 + 2] = uint8_t(std::min(int(p.v[2] * 255), 255));
        }
    }

    return WriteImage(&u8_data[0], w, h, 3, false /* flip_y */, false /* is_rgbm */, name) == 1;
}
#endif

bool Write_RGBM(Ren::Span<const float> data, const int w, const int h, const int channels, const bool flip_y,
                const char *name) {
    const std::vector<uint8_t> u8_data = Ren::ConvertRGB32F_to_RGBM(data, w, h, channels);
    return WriteImage(&u8_data[0], w, h, 4, flip_y, true /* is_rgbm */, name) == 1;
}

bool Write_DDS_Mips(const uint8_t *const *mipmaps, const int *widths, const int *heights, const int mip_count,
                    const int channels, const bool use_YCoCg, const char *out_file) {
    //
    // Compress mip images
    //
    std::unique_ptr<uint8_t[]> compressed_data[16];
    int compressed_size[16] = {}, compressed_size_total = 0;

    const bool use_BC3 = (channels == 4) || use_YCoCg;

    for (int i = 0; i < mip_count; i++) {
        if (channels == 1) {
            compressed_size[i] = Ren::GetRequiredMemory_BC4(widths[i], heights[i], 1);
            // NOTE: 1 byte is added due to BC4/BC5 compression write outside of memory block
            compressed_data[i] = std::make_unique<uint8_t[]>(compressed_size[i] + 1);
            Ren::CompressImage_BC4(mipmaps[i], widths[i], heights[i], compressed_data[i].get());
        } else if (channels == 2) {
            compressed_size[i] = Ren::GetRequiredMemory_BC5(widths[i], heights[i], 1);
            // NOTE: 1 byte is added due to BC4/BC5 compression write outside of memory block
            compressed_data[i] = std::make_unique<uint8_t[]>(compressed_size[i] + 1);
            Ren::CompressImage_BC5(mipmaps[i], widths[i], heights[i], compressed_data[i].get());
        } else if (channels == 3) {
            if (use_YCoCg) {
                compressed_size[i] = Ren::GetRequiredMemory_BC3(widths[i], heights[i], 1);
                compressed_data[i] = std::make_unique<uint8_t[]>(compressed_size[i]);
                auto temp_YCoCg = Ren::ConvertRGB_to_CoCgxY(mipmaps[i], widths[i], heights[i]);
                Ren::CompressImage_BC3<true /* Is_YCoCg */>(temp_YCoCg.get(), widths[i], heights[i],
                                                            compressed_data[i].get());
            } else {
                compressed_size[i] = Ren::GetRequiredMemory_BC1(widths[i], heights[i], 1);
                compressed_data[i] = std::make_unique<uint8_t[]>(compressed_size[i]);
                Ren::CompressImage_BC1<3>(mipmaps[i], widths[i], heights[i], compressed_data[i].get());
            }
        } else {
            assert(channels == 4);
            compressed_size[i] = Ren::GetRequiredMemory_BC3(widths[i], heights[i], 1);
            compressed_data[i] = std::make_unique<uint8_t[]>(compressed_size[i]);
            Ren::CompressImage_BC3(mipmaps[i], widths[i], heights[i], compressed_data[i].get());
        }
        compressed_size_total += compressed_size[i];
    }

    //
    // Write out file
    //
    Ren::DDSHeader header = {};
    header.dwMagic = (unsigned('D') << 0u) | (unsigned('D') << 8u) | (unsigned('S') << 16u) | (unsigned(' ') << 24u);
    header.dwSize = 124;
    header.dwFlags = Ren::DDSD_CAPS | Ren::DDSD_HEIGHT | Ren::DDSD_WIDTH | Ren::DDSD_PIXELFORMAT |
                     Ren::DDSD_LINEARSIZE | Ren::DDSD_MIPMAPCOUNT;
    header.dwWidth = widths[0];
    header.dwHeight = heights[0];
    header.dwPitchOrLinearSize = compressed_size_total;
    header.dwMipMapCount = mip_count;
    header.sPixelFormat.dwSize = 32;
    header.sPixelFormat.dwFlags = Ren::DDPF_FOURCC;

    if (channels == 1) {
        header.sPixelFormat.dwFourCC = Ren::FourCC_BC4_UNORM;
    } else if (channels == 2) {
        header.sPixelFormat.dwFourCC = Ren::FourCC_BC5_UNORM;
    } else if (!use_BC3) {
        header.sPixelFormat.dwFourCC = Ren::FourCC_BC1_UNORM;
    } else {
        header.sPixelFormat.dwFourCC = Ren::FourCC_BC3_UNORM;
    }

    header.sCaps.dwCaps1 = Ren::DDSCAPS_TEXTURE | Ren::DDSCAPS_MIPMAP;

    std::ofstream out_stream(out_file, std::ios::binary);
    out_stream.write((char *)&header, sizeof(header));

    for (int i = 0; i < mip_count; i++) {
        out_stream.write((char *)compressed_data[i].get(), compressed_size[i]);
    }

    return out_stream.good();
}

bool Write_DDS(const uint8_t *image_data, const int w, const int h, const int channels, const bool flip_y,
               const bool use_YCoCg, const char *out_file, uint8_t out_avg_color[4]) {
    // Check if resolution is power of two
    const bool store_mipmaps = (unsigned(w) & unsigned(w - 1)) == 0 && (unsigned(h) & unsigned(h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {}, heights[16] = {};

    mipmaps[0] = std::make_unique<uint8_t[]>(w * h * channels);
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
        const Ren::eMipOp ops[4] = {Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg, Ren::eMipOp::Avg};
        mip_count = Ren::InitMipMaps(mipmaps, widths, heights, channels, ops);

        if (out_avg_color) {
            // Use color of the last mip level
            memcpy(out_avg_color, &mipmaps[mip_count - 1][0], channels);
            for (int i = channels; i < 4; ++i) {
                out_avg_color[i] = out_avg_color[channels - 1];
            }
        }
    } else {
        mip_count = 1;

        if (out_avg_color) {
            GetTexturesAverageColor(image_data, w, h, channels, out_avg_color);
        }
    }

    if (out_avg_color && channels == 3) {
        out_avg_color[3] = 255;
    }

    if (use_YCoCg && out_avg_color) {
        uint8_t YCoCg[3];
        Ren::ConvertRGB_to_YCoCg(out_avg_color, YCoCg);

        out_avg_color[0] = YCoCg[1];
        out_avg_color[1] = YCoCg[2];
        out_avg_color[2] = 0;
        out_avg_color[3] = YCoCg[0];
    }

    uint8_t *_mipmaps[16];
    for (int i = 0; i < mip_count; i++) {
        _mipmaps[i] = mipmaps[i].get();
    }

    return Write_DDS_Mips(_mipmaps, widths, heights, mip_count, channels, use_YCoCg, out_file);
}

bool Write_KTX_DXT(const uint8_t *image_data, const int w, const int h, const int channels, const bool is_rgbm,
                   const char *out_file) {
    // Check if power of two
    bool store_mipmaps = (w & (w - 1)) == 0 && (h & (h - 1)) == 0;

    std::unique_ptr<uint8_t[]> mipmaps[16] = {};
    int widths[16] = {}, heights[16] = {};

    mipmaps[0] = std::make_unique<uint8_t[]>(w * h * channels);
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
    [[maybe_unused]] int dxt_size_total = 0;

    for (int i = 0; i < mip_count; i++) {
        if (channels == 3) {
            dxt_size[i] = Ren::GetRequiredMemory_BC1(widths[i], heights[i], 1);
            dxt_data[i] = std::make_unique<uint8_t[]>(dxt_size[i]);
            Ren::CompressImage_BC1<3>(mipmaps[i].get(), widths[i], heights[i], dxt_data[i].get());
        } else if (channels == 4) {
            dxt_size[i] = Ren::GetRequiredMemory_BC3(widths[i], heights[i], 1);
            dxt_data[i] = std::make_unique<uint8_t[]>(dxt_size[i]);
            Ren::CompressImage_BC3(mipmaps[i].get(), widths[i], heights[i], dxt_data[i].get());
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
        auto size = uint32_t(dxt_size[i]);
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

int WriteImage(const uint8_t *out_data, const int w, const int h, const int channels, const bool flip_y,
               const bool is_rgbm, const char *name) {
    int res = 0;
    if (strstr(name, ".tga") || strstr(name, ".png")) {
        // TODO: check if negative stride can be used instead of this
        std::unique_ptr<uint8_t[]> temp_data;
        if (flip_y) {
            temp_data = std::make_unique<uint8_t[]>(w * h * channels);
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
        Write_DDS(out_data, w, h, channels, flip_y, false, name, nullptr);
    }
    return res;
}

bool WriteCubemapDDS(Ren::Span<uint32_t> data[6], const int res, const int channels, const char *out_name) {
    assert(channels == 4);
    const int mip_count = Ren::CalcMipCount(res, res, 1);

    int total_size = 0;
    for (int i = 0; i < mip_count; ++i) {
        total_size += (res >> i) * (res >> i) * 4;
    }

    Ren::DDSHeader header = {};
    header.dwMagic = (unsigned('D') << 0u) | (unsigned('D') << 8u) | (unsigned('S') << 16u) | (unsigned(' ') << 24u);
    header.dwSize = 124;
    header.dwFlags = Ren::DDSD_CAPS | Ren::DDSD_HEIGHT | Ren::DDSD_WIDTH | Ren::DDSD_PIXELFORMAT |
                     Ren::DDSD_LINEARSIZE | Ren::DDSD_MIPMAPCOUNT;
    header.dwWidth = res;
    header.dwHeight = res;
    header.dwPitchOrLinearSize = 6 * total_size;
    header.dwMipMapCount = mip_count;
    header.sPixelFormat.dwSize = 32;
    header.sPixelFormat.dwFlags = Ren::DDPF_FOURCC;
    header.sPixelFormat.dwFourCC =
        (uint32_t('D') << 0u) | (uint32_t('X') << 8u) | (uint32_t('1') << 16u) | (uint32_t('0') << 24u);

    header.sCaps.dwCaps1 = Ren::DDSCAPS_TEXTURE | Ren::DDSCAPS_COMPLEX | Ren::DDSCAPS_MIPMAP;
    header.sCaps.dwCaps2 = Ren::DDSCAPS2_CUBEMAP | Ren::DDSCAPS2_CUBEMAP_POSITIVEX | Ren::DDSCAPS2_CUBEMAP_NEGATIVEX |
                           Ren::DDSCAPS2_CUBEMAP_POSITIVEY | Ren::DDSCAPS2_CUBEMAP_NEGATIVEY |
                           Ren::DDSCAPS2_CUBEMAP_POSITIVEZ | Ren::DDSCAPS2_CUBEMAP_NEGATIVEZ;

    Ren::DDS_HEADER_DXT10 dx10_header = {};
    dx10_header.dxgiFormat = Ren::DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    dx10_header.resourceDimension = Ren::D3D10_RESOURCE_DIMENSION::D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10_header.arraySize = 1;

    std::ofstream out_stream(out_name, std::ios::binary);
    out_stream.write((char *)&header, sizeof(header));
    out_stream.write((char *)&dx10_header, sizeof(dx10_header));

    int _total_size = 0;
    for (int i = 0; i < 6; ++i) {
        std::vector<float> mipmaps[16];
        int widths[16] = {}, heights[16] = {};

        mipmaps[0] = Ren::ConvertRGB9E5_to_RGB32F(data[i], res, res);
        widths[0] = heights[0] = res;

        for (int j = 1; j < mip_count; ++j) {
            mipmaps[j].resize(res * res * 3);
            widths[j] = heights[j] = (res >> j);
            for (int y = 0; y < heights[j]; ++y) {
                for (int x = 0; x < widths[j]; ++x) {
                    for (int c = 0; c < 3; ++c) {
                        mipmaps[j][3 * (y * widths[j] + x) + c] =
                            0.25f * (mipmaps[j - 1][3 * ((2 * y + 0) * widths[j - 1] + (2 * x + 0)) + c] +
                                     mipmaps[j - 1][3 * ((2 * y + 0) * widths[j - 1] + (2 * x + 1)) + c] +
                                     mipmaps[j - 1][3 * ((2 * y + 1) * widths[j - 1] + (2 * x + 0)) + c] +
                                     mipmaps[j - 1][3 * ((2 * y + 1) * widths[j - 1] + (2 * x + 1)) + c]);
                    }
                }
            }
        }

        for (int j = 0; j < mip_count; ++j) {
            std::vector<uint32_t> out_data = Ren::ConvertRGB32F_to_RGB9E5(mipmaps[j], widths[j], heights[j]);
            out_stream.write((char *)out_data.data(), widths[j] * heights[j] * sizeof(uint32_t));
            _total_size += widths[j] * heights[j] * sizeof(uint32_t);
        }
    }
    assert(_total_size = 6 * total_size);

    return out_stream.good();
}

enum class eImageType { Color, NormalMap, Metallic, Roughness, Opacity };

struct tex_config_t {
    std::string image_name;
    eImageType image_type = eImageType::Color;
    bool compress = true;
    bool dx_convention = false;
    bool copy = false;
    bool srgb_to_linear = false;
    int extract_channel = -1;
};

tex_config_t ParseTextureConfig(const char *in_file) {
    tex_config_t ret;

    std::ifstream src_stream(in_file, std::ios::binary);
    if (!src_stream) {
        return ret;
    }

    std::string line;
    while (std::getline(src_stream, line)) {
        if (!line.empty()) {
            if (line.back() == '\n') {
                line.pop_back();
            }
            if (line.back() == '\r') {
                line.pop_back();
            }

            if (line.rfind("image:", 0) == 0) {
                if (std::getline(src_stream, line)) {
                    if (line.rfind("    - ", 0) != 0) {
                        break;
                    }

                    if (line.back() == '\n') {
                        line.pop_back();
                    }
                    if (line.back() == '\r') {
                        line.pop_back();
                    }

                    const size_t n2 = line.find(' ', 6);
                    ret.image_name = line.substr(6, n2 - 6);

                    const size_t n3 = ret.image_name.find('@');
                    if (n3 != std::string::npos) {
                        std::string flag = ret.image_name.substr(n3 + 1);
                        ret.image_name = ret.image_name.substr(0, n3);
                        if (flag == "red") {
                            ret.extract_channel = 0;
                        } else if (flag == "green") {
                            ret.extract_channel = 1;
                        } else if (flag == "blue") {
                            ret.extract_channel = 2;
                        } else if (flag == "alpha") {
                            ret.extract_channel = 3;
                        } else if (flag == "dx") {
                            ret.dx_convention = true;
                        } else if (flag == "copy") {
                            ret.copy = true;
                        } else if (flag == "srgb") {
                            ret.srgb_to_linear = true;
                        }
                    }
                }
            }

            if (line.rfind("type:", 0) == 0) {
                if (std::getline(src_stream, line)) {
                    if (line.rfind("    - ", 0) != 0) {
                        break;
                    }

                    if (line.back() == '\n') {
                        line.pop_back();
                    }
                    if (line.back() == '\r') {
                        line.pop_back();
                    }

                    const size_t n2 = line.find(' ', 6);
                    const std::string type_name = line.substr(6, n2 - 6);
                    if (type_name == "color") {
                        ret.image_type = eImageType::Color;
                    } else if (type_name == "normalmap") {
                        ret.image_type = eImageType::NormalMap;
                    } else if (type_name == "metallic") {
                        ret.image_type = eImageType::Metallic;
                    } else if (type_name == "roughness") {
                        ret.image_type = eImageType::Roughness;
                    } else if (type_name == "opacity") {
                        ret.image_type = eImageType::Opacity;
                    }
                }
            }

            if (line.rfind("flags:", 0) == 0) {
                while (std::getline(src_stream, line)) {
                    if (line.rfind("    - ", 0) != 0) {
                        break;
                    }

                    if (line.back() == '\n') {
                        line.pop_back();
                    }
                    if (line.back() == '\r') {
                        line.pop_back();
                    }

                    const size_t n2 = line.find(' ', 6);
                    const std::string flag_str = line.substr(6, n2 - 6);
                    if (flag_str == "compressed") {
                        ret.compress = true;
                    } else if (flag_str == "uncompressed") {
                        ret.compress = false;
                    }
                }
            }
        }
    }
    return ret;
}

bool GetTexturesAverageColor(const char *in_file, uint8_t out_color[4]) {
    if (strcmp(in_file, "assets/textures/default_normalmap.dds") == 0) {
        out_color[0] = out_color[1] = 127;
        return true;
    }

    const tex_config_t tex = ParseTextureConfig(in_file);
    if (tex.image_name.empty()) {
        return false;
    }

    std::filesystem::path image_path = in_file;
    image_path.replace_filename(tex.image_name);

    std::ifstream src_stream(image_path, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    const auto src_size = size_t(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);
    if (!src_stream) {
        return false;
    }

    int width, height, channels;
    unsigned char *image_data = stbi_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);
    if (!image_data) {
        image_data = stbi__dds_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);
        if (!image_data) {
            return false;
        }
    }
    SCOPE_EXIT({ stbi_image_free(image_data); })

    GetTexturesAverageColor(image_data, width, height, channels, out_color);

    if (tex.image_type == eImageType::Color) {
        uint8_t YCoCg[3];
        Ren::ConvertRGB_to_YCoCg(out_color, YCoCg);

        out_color[0] = YCoCg[1];
        out_color[1] = YCoCg[2];
        out_color[2] = 0;
        out_color[3] = YCoCg[0];
    } else if (tex.image_type == eImageType::NormalMap && tex.dx_convention) {
        out_color[1] = 255 - out_color[1];
    }

    for (int i = 0; i < 4 && tex.extract_channel != -1; ++i) {
        out_color[i] = out_color[tex.extract_channel];
    }

    return true;
}

} // namespace SceneManagerInternal

bool Eng::SceneManager::HConvToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                                   Ren::SmallVectorImpl<std::string> &out_dependencies,
                                   Ren::SmallVectorImpl<asset_output_t> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("Conv %s", out_file);

    const tex_config_t tex = ParseTextureConfig(in_file);
    if (tex.image_name.empty()) {
        return false;
    }

    std::filesystem::path image_path = in_file;
    image_path.replace_filename(tex.image_name);

    if (tex.copy) {
        return std::filesystem::copy_file(image_path, out_file, std::filesystem::copy_options::overwrite_existing);
    }

    out_dependencies.push_back(image_path.generic_u8string());

    std::ifstream src_image(image_path, std::ios::binary | std::ios::ate);
    if (!src_image) {
        return false;
    }
    const auto src_size = size_t(src_image.tellg());
    src_image.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_image.read((char *)&src_buf[0], src_size);
    if (!src_image) {
        return false;
    }

    int width, height, channels;
    unsigned char *image_data = stbi_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);
    if (!image_data) {
        image_data = stbi__dds_load_from_memory(&src_buf[0], int(src_size), &width, &height, &channels, 0);
        if (!image_data) {
            return false;
        }
    }
    SCOPE_EXIT({ free(image_data); })

    bool is_1px_texture = (width > 4 && height > 4),
         is_single_channel =
             (tex.image_type != eImageType::NormalMap) && (tex.image_type != eImageType::Color) && (channels > 1),
         is_too_many_channels = (tex.image_type == eImageType::Color) && (channels > 3);
    for (int i = 0; i < width * height && (is_1px_texture || is_single_channel); ++i) {
        for (int j = 0; j < channels; ++j) {
            is_1px_texture &= (image_data[i * channels + j] == image_data[j]);
            is_single_channel &= (image_data[i * channels + j] == image_data[i * channels + 0]);
        }
    }
    is_single_channel |= (tex.extract_channel != -1);
    is_single_channel |= (tex.image_type == eImageType::Metallic);
    is_single_channel |= (tex.image_type == eImageType::Roughness);
    is_single_channel |= (tex.image_type == eImageType::Opacity);
    if (is_single_channel || is_1px_texture || is_too_many_channels) {
        // drop resolution to minimal block size
        const int width_new = is_1px_texture ? 4 : width;
        const int height_new = is_1px_texture ? 4 : height;
        // drop unnecessary channels
        int channels_new = is_single_channel ? 1 : channels;
        if (tex.image_type == eImageType::Color && channels_new > 3) {
            channels_new = 3;
        }
        auto *image_data_new = (unsigned char *)malloc(width_new * height_new * channels_new);
        if (!image_data_new) {
            return false;
        }
        SCOPE_EXIT({ free(image_data_new); })

        for (int y = 0; y < height_new; ++y) {
            for (int x = 0; x < width_new; ++x) {
                for (int j = 0; j < channels_new; ++j) {
                    const int jj = (tex.extract_channel != -1) ? tex.extract_channel : j;
                    image_data_new[(y * width_new + x) * channels_new + j] =
                        image_data[(y * width + x) * channels + jj];
                }
            }
        }
        std::swap(image_data, image_data_new);
        width = width_new;
        height = height_new;
        channels = channels_new;
    }

    if (tex.image_type == eImageType::NormalMap && channels == 3) {
        // this is normal map, drop it to two channels
        auto *image_data_new = (unsigned char *)malloc(width * height * 2);
        if (!image_data_new) {
            return false;
        }
        SCOPE_EXIT({ free(image_data_new); })

        const bool invert_y = tex.dx_convention;
        bool z_is_one = true;
        for (int i = 0; i < width * height; ++i) {
            image_data_new[2 * i + 0] = image_data[3 * i + 0];
            image_data_new[2 * i + 1] = invert_y ? 255 - image_data[3 * i + 1] : image_data[3 * i + 1];
            z_is_one &= (image_data[3 * i + 2] >= 250);
        }

        for (int i = 0; z_is_one && i < width * height; ++i) {
            auto n =
                Ren::Vec3f{float(image_data_new[2 * i + 0]) / 255.0f, float(image_data_new[2 * i + 1]) / 255.0f, 1.0f};
            n = Normalize(n * 2.0f - 1.0f);
            n = n * 0.5f + 0.5f;

            image_data_new[2 * i + 0] = std::min(std::max(int(n[0] * 255.0f), 0), 255);
            image_data_new[2 * i + 1] = std::min(std::max(int(n[1] * 255.0f), 0), 255);
        }

        std::swap(image_data, image_data_new);
        channels = 2;
    } else if (tex.image_type == eImageType::NormalMap && channels == 2 && tex.dx_convention) {
        for (int i = 0; i < width * height; ++i) {
            image_data[2 * i + 1] = 255 - image_data[2 * i + 1];
        }
    } else if (tex.srgb_to_linear) {
        for (int i = 0; i < width * height; ++i) {
            for (int j = 0; j < channels; ++j) {
                float val = float(image_data[channels * i + j]) / 255.0f;
                if (val > 0.04045f) {
                    val = powf((val + 0.055f) / 1.055f, 2.4f);
                } else {
                    val = val / 12.92f;
                }
                image_data[channels * i + j] = uint8_t(std::min(std::max(int(val * 255), 0), 255));
            }
        }
    }

    const bool use_YCoCg = (tex.image_type == eImageType::Color);

    uint8_t average_color[4] = {};
    const bool res =
        Write_DDS(image_data, width, height, channels, false /* flip_y */, use_YCoCg, out_file, average_color);
    if (res) {
        std::lock_guard<std::mutex> _(ctx.cache_mtx);
        ctx.cache->WriteTextureAverage(in_file, average_color);
    }
    return res;
}

bool Eng::SceneManager::HConvHDRToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                                      Ren::SmallVectorImpl<std::string> &out_dependencies,
                                      Ren::SmallVectorImpl<asset_output_t> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("Conv %s", out_file);

    std::string out_hdr_path = out_file;
    out_hdr_path.replace(out_hdr_path.length() - 3, 3, "hdr");
    if (!std::filesystem::copy_file(in_file, out_hdr_path, std::filesystem::copy_options::overwrite_existing)) {
        return false;
    }

    int width, height;
    const std::vector<uint8_t> image_rgbe = LoadHDR(in_file, width, height);
    const std::vector<float> image_f32 = Ren::ConvertRGBE_to_RGB32F(image_rgbe, width, height);

    auto fetch_hdr = [&image_f32, width](const int x, const int y) {
        return Ren::Vec4f{image_f32[3 * (y * width + x) + 0], image_f32[3 * (y * width + x) + 1],
                          image_f32[3 * (y * width + x) + 2], 0.0f};
    };

    auto sample_hdr = [&fetch_hdr, width, height](Ren::Vec2f uv) {
        uv *= Ren::Vec2f{float(width), float(height)};
        auto iuv0 = Ren::Vec2i{uv};
        iuv0 = Ren::Clamp(iuv0, Ren::Vec2i{0, 0}, Ren::Vec2i{width - 1, height - 1});
        const Ren::Vec2i iuv1 = (iuv0 + 1) % Ren::Vec2i{width, height};

        const Ren::Vec4f p00 = fetch_hdr(iuv0[0], iuv0[1]), p01 = fetch_hdr(iuv1[0], iuv0[1]),
                         p10 = fetch_hdr(iuv0[0], iuv1[1]), p11 = fetch_hdr(iuv1[0], iuv1[1]);

        const Ren::Vec2f k = Fract(uv);
        const Ren::Vec4f p0 = p01 * k[0] + p00 * (1.0f - k[0]), p1 = p11 * k[0] + p10 * (1.0f - k[0]);

        return (p1 * k[1] + p0 * (1.0f - k[1]));
    };

    auto sample_hdr_latlong = [&sample_hdr](const Ren::Vec3f &dir, const float y_rotation) {
        const float theta = acosf(std::clamp(dir[1], -1.0f, 1.0f)) / Ren::Pi<float>();
        float phi = atan2f(dir[2], dir[0]) + y_rotation;
        if (phi < 0) {
            phi += 2 * Ren::Pi<float>();
        }
        if (phi > 2 * Ren::Pi<float>()) {
            phi -= 2 * Ren::Pi<float>();
        }

        float u = 0.5f * phi / Ren::Pi<float>();
        u = u - std::floor(u);

        return sample_hdr(Ren::Vec2f{u, theta});
    };

    int CubemapRes = 4;
    while (CubemapRes < (width / 4)) {
        CubemapRes *= 2;
    }

    const Ren::Vec3f axis[6] = {Ren::Vec3f{1.0f, 0.0f, 0.0f}, Ren::Vec3f{-1.0f, 0.0f, 0.0f},
                                Ren::Vec3f{0.0f, 1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f},
                                Ren::Vec3f{0.0f, 0.0f, 1.0f}, Ren::Vec3f{0.0f, 0.0f, -1.0f}};
    const Ren::Vec3f up[6] = {Ren::Vec3f{0.0f, -1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f},
                              Ren::Vec3f{0.0f, 0.0f, 1.0f},  Ren::Vec3f{0.0f, 0.0f, -1.0f},
                              Ren::Vec3f{0.0f, -1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f}};

    std::vector<uint32_t> output[6];
    Ren::Span<uint32_t> _output[6];
    for (int face = 0; face < 6; ++face) {
        const Ren::Vec3f side = Cross(axis[face], up[face]);

        std::vector<float> temp_output(3 * CubemapRes * CubemapRes);
        for (int y = 0; y < CubemapRes; ++y) {
            const float v = 2.0f * ((float(y) + 0.0f) / CubemapRes) - 1.0f;
            for (int x = 0; x < CubemapRes; ++x) {
                const float u = 2.0f * ((float(x) + 0.0f) / CubemapRes) - 1.0f;

                const Ren::Vec3f dir = Normalize(axis[face] + u * side + v * up[face]);
                const Ren::Vec4f val = Clamp(sample_hdr_latlong(dir, 0.0f), Ren::Vec4f{0}, Ren::Vec4f{255});

                temp_output[3 * (y * CubemapRes + x) + 0] = val[0];
                temp_output[3 * (y * CubemapRes + x) + 1] = val[1];
                temp_output[3 * (y * CubemapRes + x) + 2] = val[2];
            }
        }
        output[face] = Ren::ConvertRGB32F_to_RGB9E5(temp_output, CubemapRes, CubemapRes);
        _output[face] = output[face];
    }

    return WriteCubemapDDS(_output, CubemapRes, 4, out_file);
}

bool Eng::SceneManager::HConvHDRToRGBM(assets_context_t &ctx, const char *in_file, const char *out_file,
                                       Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("Conv %s", out_file);

    int width, height;
    const std::vector<uint8_t> image_rgbe = LoadHDR(in_file, width, height);
    const std::vector<float> image_f32 = Ren::ConvertRGBE_to_RGB32F(image_rgbe, width, height);

    return Write_RGBM(image_f32, width, height, 3, false /* flip_y */, out_file);
}

bool Eng::SceneManager::HConvImgToDDS(assets_context_t &ctx, const char *in_file, const char *out_file,
                                      Ren::SmallVectorImpl<std::string> &, Ren::SmallVectorImpl<asset_output_t> &) {
    using namespace SceneManagerInternal;

    ctx.log->Info("Conv %s", out_file);

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

        mipmaps[i] = std::make_unique<uint8_t[]>(orig_size);

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

    return Write_DDS_Mips(_mipmaps, widths, heights, mips_count, 4, false, out_file);
}

bool Eng::SceneManager::WriteProbeCache(const char *out_folder, const char *scene_name, const Ren::ProbeStorage &probes,
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
    // const size_t prelude_length = out_file_name_base.length();
    out_file_name_base += scene_name;

    std::error_code ec;
    std::filesystem::create_directories(out_file_name_base, ec);
    if (ec) {
        log->Error("Failed to create folders!");
        return false;
    }

    // write probes
    uint32_t cur_index = light_probe_storage->First();
    while (cur_index != 0xffffffff) {
        const auto *lprobe = (const LightProbe *)light_probe_storage->Get(cur_index);
        assert(lprobe);

        if (lprobe->layer_index != -1) {
            Sys::JsArray js_probe_faces;

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
