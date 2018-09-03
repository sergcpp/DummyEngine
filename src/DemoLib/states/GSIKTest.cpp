#include "GSIKTest.h"

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

namespace GSIKTestInternal {

}

GSIKTest::GSIKTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSIKTest::~GSIKTest() {
    
}

void GSIKTest::Enter() {
    using namespace GSIKTestInternal;

    
}

void GSIKTest::Exit() {

}

void GSIKTest::Draw(float dt_s) {
    using namespace GSIKTestInternal;

    {
        glClearColor(0, 0.2f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

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

void GSIKTest::Update(int dt_ms) {

}

void GSIKTest::HandleInput(InputManager::Event evt) {
    using namespace GSIKTestInternal;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        
        break;
    case InputManager::RAW_INPUT_P1_UP: {
        
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
