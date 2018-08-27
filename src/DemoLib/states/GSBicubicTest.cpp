#include "GSBicubicTest.h"

#include <fstream>
#include <random>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSBicubicTestInternal {
    void SampleNearest(const image_t &img, float x, float y, uint8_t *out_col) {
        int x0 = (int)std::floor(x * img.w);
        int y0 = (int)std::floor(y * img.h);

        if (x0 < 0) x0 = 0;
        if (x0 > img.w - 1) x0 = img.w - 1;

        for (int i = 0; i < 3; i++) {
            out_col[i] = img.data[3 * (y0 * img.w + x0) + i];
        }
    }

    void SampleLinear(const image_t &img, float x, float y, uint8_t *out_col) {
        int x0 = (int)std::floor(x * img.w - 0.5f);
        int y0 = (int)std::floor(y * img.h - 0.5f);
        int x1 = std::min(x0 + 1, img.w - 1);
        int y1 = std::min(y0 + 1, img.h - 1);

        float fac_x = (x * img.w - 0.5f) - x0,
                fac_y = (y * img.h - 0.5f) - y0;

        x0 = std::max(x0, 0);
        y0 = std::max(y0, 0);

        for (int i = 0; i < 3; i++) {
            float v1 = img.data[3 * (y0 * img.w + x0) + i] * (1 - fac_x) + img.data[3 * (y0 * img.w + x1) + i] * fac_x;
            float v2 = img.data[3 * (y1 * img.w + x0) + i] * (1 - fac_x) + img.data[3 * (y1 * img.w + x1) + i] * fac_x;

            float v = v1 * (1 - fac_y) + v2 * fac_y;

            out_col[i] = uint8_t(v);
        }
    }
    
    void SampleCubic(const image_t &img, float x, float y, uint8_t *out_col) {
        int x1 = (int)std::floor(x * img.w - 0.5f);
        int y1 = (int)std::floor(y * img.h - 0.5f);
        int x2 = std::min(x1 + 1, img.w - 1);
        int y2 = std::min(y1 + 1, img.h - 1);

        float fac_x = (x * img.w - 0.5f) - x1,
            fac_y = (y * img.h - 0.5f) - y1;

        x1 = std::max(x1, 0);
        y1 = std::max(y1, 0);

        int x0 = std::max(x1 - 1, 0);
        int y0 = std::max(y1 - 1, 0);
        int x3 = std::min(x2 + 1, img.w - 1);
        int y3 = std::min(y2 + 1, img.h - 1);

        auto interpolate = [](float p0, float p1, float p2, float p3, float x) {
            float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
            float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            float c = -0.5f * p0 + 0.5f * p2;
            float d = p1;

            return (a * x * x * x) + (b * x * x) + (c * x) + d;
        };

        auto clamp = [](float x, float min, float max) {
            return std::min(std::max(x, min), max);
        };

        for (int i = 0; i < 3; i++) {
            float p00 = img.data[3 * (y0 * img.w + x0) + i];
            float p10 = img.data[3 * (y0 * img.w + x1) + i];
            float p20 = img.data[3 * (y0 * img.w + x2) + i];
            float p30 = img.data[3 * (y0 * img.w + x3) + i];

            float p01 = img.data[3 * (y1 * img.w + x0) + i];
            float p11 = img.data[3 * (y1 * img.w + x1) + i];
            float p21 = img.data[3 * (y1 * img.w + x2) + i];
            float p31 = img.data[3 * (y1 * img.w + x3) + i];

            float p02 = img.data[3 * (y2 * img.w + x0) + i];
            float p12 = img.data[3 * (y2 * img.w + x1) + i];
            float p22 = img.data[3 * (y2 * img.w + x2) + i];
            float p32 = img.data[3 * (y2 * img.w + x3) + i];

            float p03 = img.data[3 * (y3 * img.w + x0) + i];
            float p13 = img.data[3 * (y3 * img.w + x1) + i];
            float p23 = img.data[3 * (y3 * img.w + x2) + i];
            float p33 = img.data[3 * (y3 * img.w + x3) + i];

            float f0 = interpolate(p00, p10, p20, p30, fac_x);
            float f1 = interpolate(p01, p11, p21, p31, fac_x);
            float f2 = interpolate(p02, p12, p22, p32, fac_x);
            float f3 = interpolate(p03, p13, p23, p33, fac_x);

            float f = clamp(interpolate(f0, f1, f2, f3, fac_y), 0, 255);

            out_col[i] = uint8_t(f);
        }
    }

    enum eSampleMode { Nearest, Linear, Cubic };

    image_t Upscale(const image_t &img, int s, eSampleMode mode) {
        image_t ret;
        ret.format = img.format;
        ret.w = img.w * s;
        ret.h = img.h * s;

        if (ret.format == Ren::RawRGB888) {
            ret.data = std::unique_ptr<uint8_t[]>{ new uint8_t[ret.w * ret.h * 3] };

            const float off_x = 0.5f / ret.w,
                        off_y = 0.5f / ret.h;

            for (int j = 0; j < ret.h; j++) {
                for (int i = 0; i < ret.w; i++) {
                    float x = off_x + float(i) / ret.w,
                          y = off_y + float(j) / ret.h;

                    if (mode == Nearest) {
                        SampleNearest(img, x, y, &ret.data[3 * (j * ret.w + i)]);
                    } else if (mode == Linear) {
                        SampleLinear(img, x, y, &ret.data[3 * (j * ret.w + i)]);
                    } else if (mode == Cubic) {
                        SampleCubic(img, x, y, &ret.data[3 * (j * ret.w + i)]);
                    }
                }
            }
        }

        return ret;
    }

    image_t DownScale(const image_t &img, int s) {
        image_t ret;
        ret.format = img.format;
        ret.w = img.w / s;
        ret.h = img.h / s;

        if (ret.format == Ren::RawRGB888) {
            ret.data = std::unique_ptr<uint8_t[]>{ new uint8_t[ret.w * ret.h * 3] };

            const float off_x = 0,//0.5f / ret.w,
                        off_y = 0;//0.5f / ret.h;

            for (int j = 0; j < ret.h; j++) {
                for (int i = 0; i < ret.w; i++) {
                    uint32_t res[3] = { 0 };
                    for (int k = 0; k < s; k++) {
                        for (int l = 0; l < s; l++) {
                            float x = off_x + (i + float(l) / s) / ret.w,
                                  y = off_y + (j + float(k) / s) / ret.h;

                            uint8_t col[3];
                            SampleLinear(img, x, y, &col[0]);

                            res[0] += col[0];
                            res[1] += col[1];
                            res[2] += col[2];
                        }
                    }

                    ret.data[3 * (j * ret.w + i) + 0] = res[0] / (s * s);
                    ret.data[3 * (j * ret.w + i) + 1] = res[1] / (s * s);
                    ret.data[3 * (j * ret.w + i) + 2] = res[2] / (s * s);
                }
            }
        }

        return ret;
    }

    void WriteTGA(const image_t &img, const std::string &name) {
        assert(img.format == Ren::RawRGB888);
        int bpp = 3;

        std::ofstream file(name, std::ios::binary);

        unsigned char header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        header[12] = img.w & 0xFF;
        header[13] = (img.w >> 8) & 0xFF;
        header[14] = (img.h) & 0xFF;
        header[15] = (img.h >> 8) & 0xFF;
        header[16] = bpp * 8;

        file.write((char *) &header[0], sizeof(unsigned char) * 18);

        auto out_data = std::unique_ptr<uint8_t[]>{ new uint8_t[img.w * img.h * bpp] };
        for (int i = 0; i < img.w * img.h; i++) {
            out_data[i * 3 + 0] = img.data[i * 3 + 2];
            out_data[i * 3 + 1] = img.data[i * 3 + 1];
            out_data[i * 3 + 2] = img.data[i * 3 + 0];
        }

        file.write((const char *) &out_data[0], img.w * img.h * bpp);

        static const char footer[26] = "\0\0\0\0" // no extension area
                                       "\0\0\0\0"// no developer directory
                                       "TRUEVISION-XFILE"// yep, this is a TGA file
                                       ".";
        file.write((const char *) &footer, sizeof(footer));
    }

    template <int InN, int OutN>
    struct layer_t {
        float weights[OutN][InN];
        float biases[OutN];

        float input[InN];
        float output[OutN];

        void process() {
            for (int i = 0; i < OutN; i++) {
                output[i] = biases[i];
                for (int j = 0; j < InN; j++) {
                    output[i] += input[j] * weights[i][j];
                }
                if (output[i] < 0.0f) output[i] = 0.0f;
            }
        }
    };

    const int f1 = 9, f2 = 1, f3 = 5;
    const int c = 3, n1 = 64, n2 = 32;

    layer_t<c * f1 * f1, n1> g_first_layer;
    layer_t<n1 * f2 * f2, n2> g_second_layer;
    layer_t<n2 * f3 * f3, c> g_third_layer;
}

GSBicubicTest::GSBicubicTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSBicubicTest::~GSBicubicTest() {
    
}

void GSBicubicTest::Enter() {
    using namespace GSBicubicTestInternal;

    std::ifstream in_file("test_img2.tga", std::ios::binary | std::ios::ate);
    size_t in_file_size = (size_t)in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    auto in_file_data = std::unique_ptr<char[]>{ new char[in_file_size] };
    in_file.read(&in_file_data[0], in_file_size);

    orig_image_.data = ReadTGAFile(&in_file_data[0], orig_image_.w, orig_image_.h, orig_image_.format);
#if 0
    new_image_.w = 256;
    new_image_.h = 256;
    new_image_.data.reset(new uint8_t[new_image_.w * new_image_.h * 3]);
    new_image_.format = Ren::RawRGB888;

    LOGI("%i", Sys::GetTicks());

    const float pi = 3.14f;

    for (int i = 0; i < new_image_.w; i++) {
        float alpha = 1.0f * pi * float(i) / new_image_.w - 0.5 * pi;

        Ren::Vec2f u_vec = { std::cos(alpha), std::sin(alpha) },
                   v_vec = { -u_vec[1], u_vec[0] };

        for (int j = 0; j < new_image_.h; j++) {
            float u = std::sqrt(2.0f) * (float(j) / new_image_.h - 0.5f);

            uint32_t sum[3] = { 0, 0, 0 };
            uint32_t sample_count = 0;

            for (float v = -0.5f * std::sqrt(2.0f); v < 0.5f * std::sqrt(2.0f); v += 0.005f) {
                auto uv = Ren::Vec2f{ 0.5f, 0.5f } + u * u_vec + v * v_vec;

                if (uv[0] >= 0.0f && uv[0] <= 1.0f && uv[1] >= 0.0f && uv[1] <= 1.0f) {
                    uint8_t col[3];
                    //SampleLinear(orig_image_, uv[0], uv[1], col);
                    SampleNearest(orig_image_, uv[0], uv[1], col);

                    sum[0] += col[0];
                    sum[1] += col[1];
                    sum[2] += col[2];
                    sample_count++;
                }
            }

            sample_count = 1;


            float k = 1.0f / (std::sqrt(2.0f) / 0.005f);

            uint32_t r = (uint32_t)(sum[0] * k),
                     g = (uint32_t)(sum[1] * k),
                     b = (uint32_t)(sum[2] * k);

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            new_image_.data[3 * (j * new_image_.w + i) + 0] = (uint8_t)(r);
            new_image_.data[3 * (j * new_image_.w + i) + 1] = (uint8_t)(g);
            new_image_.data[3 * (j * new_image_.w + i) + 2] = (uint8_t)(b);
        }
    }

    LOGI("%i", Sys::GetTicks());

    WriteTGA(new_image_, "pre.tga");
#endif

    //image_t new_img2 = Upscale(orig_image_, 16, Cubic);
    //WriteTGA(new_img2, "upscaled.tga");
    
    /*uint8_t _matrix[8][8] = { { 0,  48, 12, 60, 3,  51, 15, 63 },
                             { 32, 16, 44, 28, 35, 19, 47, 31 },
                             { 8,  56, 4,  52, 11, 59, 7,  55 },
                             { 40, 24, 36, 20, 43, 27, 39, 23 },
                             { 2,  50, 14, 62, 1,  49, 13, 61 },
                             { 34, 18, 46, 30, 33, 17, 45, 29 },
                             { 10, 58, 6,  54, 9,  57, 5,  53 },
                             { 42, 26, 38, 22, 41, 25, 37, 21 } };

    uint8_t matrix[4][4] = { { 0,  8,  2,  10 },
                             { 12, 4,  14, 6  },
                             { 3,  11, 1,  9  },
                             { 15, 7,  13, 5  } };

    for (int j = 0; j < orig_image_.h; j++) {
        for (int i = 0; i < orig_image_.w; i++) {
            int x = i % 4, y = j % 4;

            orig_image_.data[3 * (j * orig_image_.w + i) + 0] += (matrix[y][x] / 2 - 4);
            orig_image_.data[3 * (j * orig_image_.w + i) + 1] += (matrix[y][x] / 4 - 2);
            orig_image_.data[3 * (j * orig_image_.w + i) + 2] += (matrix[y][x] / 2 - 4);

            orig_image_.data[3 * (j * orig_image_.w + i) + 0] -= (orig_image_.data[3 * (j * orig_image_.w + i) + 0] + 4) % 8;
            orig_image_.data[3 * (j * orig_image_.w + i) + 1] -= (orig_image_.data[3 * (j * orig_image_.w + i) + 1] + 2) % 4;
            orig_image_.data[3 * (j * orig_image_.w + i) + 2] -= (orig_image_.data[3 * (j * orig_image_.w + i) + 2] + 4) % 8;
        }
    }*/

    /*for (int j = 0; j < orig_image_.h; j++) {
        for (int i = 0; i < orig_image_.w; i++) {
            float x = float(i) / orig_image_.w;
            float y = float(j) / orig_image_.h;
            float z = 1.0f - x - y;
            //float z = std::sqrt(1.0f - x - y);

            orig_image_.data[3 * (j * orig_image_.w + i) + 0] = (uint8_t)(x * 255);
            orig_image_.data[3 * (j * orig_image_.w + i) + 1] = (uint8_t)(y * 255);
            orig_image_.data[3 * (j * orig_image_.w + i) + 2] = (uint8_t)(z * 255);
        }
    }*/

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (int i = 0; i < n1; i++) {
        for (int j = 0; j < c * f1 * f1; j++) {
            g_first_layer.weights[i][j] = dis(gen);
            g_first_layer.biases[i] = dis(gen);
        }
    }

    for (int i = 0; i < n2; i++) {
        for (int j = 0; j < n1 * f2 * f2; j++) {
            g_second_layer.weights[i][j] = dis(gen);
            g_second_layer.biases[i] = dis(gen);
        }
    }

    for (int i = 0; i < c; i++) {
        for (int j = 0; j < n2 * f3 * f3; j++) {
            g_third_layer.weights[i][j] = dis(gen);
            g_third_layer.biases[i] = dis(gen);
        }
    }
}

void GSBicubicTest::OnMouse(int x, int y) {
    //LOGI("%i %i", x, y);

    float r = float(x) / orig_image_.w;
    float g = float(y) / orig_image_.h;
    float b = 1.0f - r - g;

    // pointed color
    for (int j = 450; j < orig_image_.h; j++) {
        for (int i = 450; i < orig_image_.w; i++) {
            orig_image_.data[3 * (j * orig_image_.w + i) + 0] = (uint8_t)(r * 255);
            orig_image_.data[3 * (j * orig_image_.w + i) + 1] = (uint8_t)(g * 255);
            orig_image_.data[3 * (j * orig_image_.w + i) + 2] = (uint8_t)(b * 255);
        }
    }

    float fx = float(x), fy = float(y);

    float u = std::sqrt(fx * fx + fy * fy);
    float v = std::sqrt((500.0f - fx) * (500.0f - fx) + fy * fy);
    float w = std::sqrt(fx * fx + (500.0f - fy) * (500.0f - fy));

    float s = u + v + w;

    //u /= s;
    //v /= s;
    //w /= s;

    LOGI("%f %f %f", u, v, w);
}

void GSBicubicTest::Exit() {

}

void GSBicubicTest::Draw(float dt_s) {
    using namespace GSBicubicTestInternal;

    {
        glClearColor(0, 0.2f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        float pos_x = -1;

        glUseProgram(0);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glRasterPos2f(pos_x, -1);
        glDrawPixels(orig_image_.w, orig_image_.h, GL_RGB, GL_UNSIGNED_BYTE, &orig_image_.data[0]);
        pos_x += float(orig_image_.w * 2)/game_->width;

        /*glRasterPos2f(pos_x, -1);
        glDrawPixels(new_image_.w, new_image_.h, GL_RGB, GL_UNSIGNED_BYTE, &new_image_.data[0]);
        pos_x += float(new_image_.w * 2) / game_->width;*/

        image_t new_img1 = DownScale(orig_image_, 3);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img1.w, new_img1.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img1.data[0]);
        pos_x += float(new_img1.w * 2)/game_->width;

        image_t new_img2 = Upscale(new_img1, 3, Cubic);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img2.w, new_img2.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img2.data[0]);
        pos_x += float(new_img2.w * 2) / game_->width;

        std::vector<float> results1;

        for (int j = 0; j < new_img2.h - f1; j++) {
            for (int i = 0; i < new_img2.w - f1; i++) {
                for (int _y = 0; _y < f1; _y++) {
                    for (int _x = 0; _x < f1; _x++) {
                        int y = j + _y, x = i + _x;

                        float r = new_img2.data[3 * (y * new_img2.w + x) + 0] / 255.0f;
                        float g = new_img2.data[3 * (y * new_img2.w + x) + 1] / 255.0f;
                        float b = new_img2.data[3 * (y * new_img2.w + x) + 2] / 255.0f;

                        g_first_layer.input[c * (_y * f1 + _x) + 0] = r;
                        g_first_layer.input[c * (_y * f1 + _x) + 1] = g;
                        g_first_layer.input[c * (_y * f1 + _x) + 2] = b;
                    }
                }

                g_first_layer.process();

                for (int k = 0; k < n1; k++) {
                    results1.push_back(g_first_layer.output[k]);
                }
            }
        }

        const int l1_w = new_img2.w - f1, l1_h = new_img2.h - f1;

        std::vector<float> results2;

        for (int j = 0; j < l1_h - f2; j++) {
            for (int i = 0; i < l1_w - f2; i++) {
                for (int _y = 0; _y < f2; _y++) {
                    for (int _x = 0; _x < f2; _x++) {
                        int y = j + _y, x = i + _x;

                        for (int k = 0; k < n1; k++) {
                            float val = results1[n1 * (y * f2 + x) + k];
                            g_second_layer.input[n1 * (_y * f2 + _x) + k] = val;
                        }
                    }
                }

                g_second_layer.process();

                for (int k = 0; k < n2; k++) {
                    results2.push_back(g_second_layer.output[k]);
                }
            }
        }

        const int l2_w = new_img2.w - f1 - f2, l2_h = new_img2.h - f1 - f2;

        std::vector<float> results3;

        for (int j = 0; j < l2_h - f3; j++) {
            for (int i = 0; i < l2_w - f3; i++) {
                for (int _y = 0; _y < f3; _y++) {
                    for (int _x = 0; _x < f3; _x++) {
                        int y = j + _y, x = i + _x;

                        for (int k = 0; k < n2; k++) {
                            float val = results2[n2 * (y * f3 + x) + k];
                            g_third_layer.input[n2 * (_y * f3 + _x) + k] = val;
                        }
                    }
                }

                g_third_layer.process();

                for (int k = 0; k < c; k++) {
                    results3.push_back(g_third_layer.output[k]);
                }
            }
        }

        const int l3_w = new_img2.w - f1 - f2 - f3, l3_h = new_img2.h - f1 - f2 - f3;

        image_t new_img3;
        new_img3.w = l3_w;
        new_img3.h = l3_h;
        new_img3.data.reset(new uint8_t[3 * l3_w * l3_h]);

        for (int j = 0; j < l3_h; j++) {
            for (int i = 0; i < l3_w; i++) {
                new_img3.data[3 * (j * l3_w + i) + 0] = (uint8_t)(results3[3 * (j * l3_w + i) + 0] * 255);
                new_img3.data[3 * (j * l3_w + i) + 1] = (uint8_t)(results3[3 * (j * l3_w + i) + 1] * 255);
                new_img3.data[3 * (j * l3_w + i) + 2] = (uint8_t)(results3[3 * (j * l3_w + i) + 2] * 255);
            }
        }

        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img3.w, new_img3.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img3.data[0]);
        pos_x += float(new_img3.w * 2) / game_->width;

        volatile int ii = 0;
    }

    {
        // ui draw
        ui_renderer_->BeginDraw();

        //font_->DrawText(ui_renderer_.get(), "111", { -1, 1.0f - 1 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s2.c_str(), { -1, 1.0f - 2 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s3.c_str(), { -1, 1.0f - 3 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s4.c_str(), { -1, 1.0f - 4 * font_->height(ui_root_.get()) }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSBicubicTest::Update(int dt_ms) {

}

void GSBicubicTest::HandleInput(InputManager::Event evt) {
    using namespace GSBicubicTestInternal;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        
        break;
    case InputManager::RAW_INPUT_P1_UP: {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (int i = 0; i < n1; i++) {
            for (int j = 0; j < c * f1 * f1; j++) {
                g_first_layer.weights[i][j] = dis(gen);
                g_first_layer.biases[i] = dis(gen);
            }
        }

        for (int i = 0; i < n2; i++) {
            for (int j = 0; j < n1 * f2 * f2; j++) {
                g_second_layer.weights[i][j] = dis(gen);
                g_second_layer.biases[i] = dis(gen);
            }
        }

        for (int i = 0; i < c; i++) {
            for (int j = 0; j < n2 * f3 * f3; j++) {
                g_third_layer.weights[i][j] = dis(gen);
                g_third_layer.biases[i] = dis(gen);
            }
        }
    } break;
    case InputManager::RAW_INPUT_P1_MOVE:
        //OnMouse(int(evt.point.x), 500 - (int(evt.point.y) - 140));
        break;
    case InputManager::RAW_INPUT_KEY_DOWN:
        
        break;
    case InputManager::RAW_INPUT_KEY_UP:
        break;
    case InputManager::RAW_INPUT_RESIZE:

        break;
    default:
        break;
    }
}
