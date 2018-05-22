#include "GSBicubicTest.h"

#include <fstream>

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <ren/GL.h>
#include <ren/Utils.h>
#include <sys/Time_.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSBicubicTestInternal {
    void SampleNearest(const image_t &img, float x, float y, uint8_t *out_col) {
        int x0 = (int)std::floor(x * img.w);
        int y0 = (int)std::floor(y * img.h);

        if (x0 < 0) __debugbreak();
        if (x0 > img.w - 1) __debugbreak();

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

        if (ret.format == ren::RawRGB888) {
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

        if (ret.format == ren::RawRGB888) {
            ret.data = std::unique_ptr<uint8_t[]>{ new uint8_t[ret.w * ret.h * 3] };

            const float off_x = 0,//0.5f / ret.w,
                        off_y = 0;//0.5f / ret.h;

            for (int j = 0; j < ret.h; j++) {
                for (int i = 0; i < ret.w; i++) {
                    float x = off_x + float(i) / ret.w,
                          y = off_y + float(j) / ret.h;

                    SampleNearest(img, x, y, &ret.data[3 * (j * ret.w + i)]);
                }
            }
        }

        return ret;
    }

    void WriteTGA(const image_t &img, const std::string &name) {
        assert(img.format == ren::RawRGB888);
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

}

GSBicubicTest::GSBicubicTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSBicubicTest::~GSBicubicTest() {
    
}

void GSBicubicTest::Enter() {
    using namespace GSBicubicTestInternal;
    using namespace math;

    std::ifstream in_file("grad.tga", std::ios::binary | std::ios::ate);
    size_t in_file_size = (size_t)in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    auto in_file_data = std::unique_ptr<char[]>{ new char[in_file_size] };
    in_file.read(&in_file_data[0], in_file_size);

    orig_image_.data = ReadTGAFile(&in_file_data[0], orig_image_.w, orig_image_.h, orig_image_.format);

    //image_t new_img2 = Upscale(orig_image_, 16, Cubic);
    //WriteTGA(new_img2, "upscaled.tga");
    
    uint8_t _matrix[8][8] = { { 0,  48, 12, 60, 3,  51, 15, 63 },
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
    }
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

        glRasterPos2f(pos_x, -1);
        glDrawPixels(orig_image_.w, orig_image_.h, GL_RGB, GL_UNSIGNED_BYTE, &orig_image_.data[0]);
        pos_x += float(orig_image_.w * 2)/game_->width;

        /*image_t new_img1 = DownScale(orig_image_, 3);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img1.w, new_img1.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img1.data[0]);
        pos_x += float(new_img1.w * 2)/game_->width;*/

        /*image_t new_img2 = DownScale(new_img1, 7);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img2.w, new_img2.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img2.data[0]);
        pos_x += float(new_img2.w * 2) / game_->width;*/
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
    using namespace math;

    
}

void GSBicubicTest::HandleInput(InputManager::Event evt) {
    using namespace math;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        
        break;
    case InputManager::RAW_INPUT_P1_UP:
        
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        
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
