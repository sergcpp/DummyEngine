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
    image_t UpscaleSimple(const image_t &img) {
        image_t ret;
        ret.format = img.format;
        ret.w = img.w * 2;
        ret.h = img.h * 2;

        if (ret.format == ren::RawRGB888) {
            ret.data = std::unique_ptr<uint8_t[]>{ new uint8_t[ret.w * ret.h * 3] };
            for (int j = 0; j < ret.h; j++) {
                for (int i = 0; i < ret.w; i++) {
                    for (int k = 0; k < 3; k++) {
                        ret.data[3 * (j * ret.w + i) + k] = img.data[3 * ((j / 2) * img.w + (i / 2)) + k];
                    }
                }
            }
        }

        return ret;
    }

    image_t UpscaleLinear(const image_t &img) {
        image_t ret;
        ret.format = img.format;
        ret.w = img.w * 2;
        ret.h = img.h * 2;

        if (ret.format == ren::RawRGB888) {
            ret.data = std::unique_ptr<uint8_t[]>{ new uint8_t[ret.w * ret.h * 3] };
            for (int j = 0; j < img.h; j++) {
                for (int i = 0; i < img.w; i++) {
                    for (int k = 0; k < 3; k++) {
                        ret.data[3 * ((2 * j + 0) * ret.w + (2 * i + 0)) + k] = img.data[3 * (j * img.w + i) + k];
                        ret.data[3 * ((2 * j + 0) * ret.w + (2 * i + 1)) + k] = img.data[3 * (j * img.w + i) + k];
                        ret.data[3 * ((2 * j + 1) * ret.w + (2 * i + 0)) + k] = img.data[3 * (j * img.w + i) + k];
                        ret.data[3 * ((2 * j + 1) * ret.w + (2 * i + 1)) + k] = img.data[3 * (j * img.w + i) + k];
                    }
                }
            }
        }

        return ret;
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
    using namespace math;

    std::ifstream in_file("image.tga", std::ios::binary | std::ios::ate);
    size_t in_file_size = (size_t)in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    auto in_file_data = std::unique_ptr<char[]>{ new char[in_file_size] };
    in_file.read(&in_file_data[0], in_file_size);

    orig_image_.data = ReadTGAFile(&in_file_data[0], orig_image_.w, orig_image_.h, orig_image_.format);
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

        image_t new_img1 = UpscaleSimple(orig_image_);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img1.w, new_img1.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img1.data[0]);
        pos_x += float(new_img1.w * 2)/game_->width;

        image_t new_img2 = UpscaleSimple(new_img1);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img2.w, new_img2.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img2.data[0]);
        pos_x += float(new_img2.w * 2)/game_->width;

        image_t new_img3 = UpscaleSimple(new_img2);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img3.w, new_img3.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img3.data[0]);
        pos_x += float(new_img3.w * 2) / game_->width;

        image_t new_img4 = UpscaleSimple(new_img3);
        glRasterPos2f(pos_x, -1);
        glDrawPixels(new_img4.w, new_img4.h, GL_RGB, GL_UNSIGNED_BYTE, &new_img4.data[0]);
        pos_x += float(new_img4.w * 2) / game_->width;
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
