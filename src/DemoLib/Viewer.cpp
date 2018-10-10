#include "Viewer.h"

#include <sstream>

#include <Eng/GameStateManager.h>
#include <Ray/RendererFactory.h>
#include <Ren/Context.h>
#include <Ren/MVec.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Log.h>

#include "Gui/FontStorage.h"
#include "Scene/Renderer.h"
#include "Scene/SceneManager.h"
#include "States/GSCreate.h"

Viewer::Viewer(int w, int h, const char *local_dir) : GameBase(w, h, local_dir) {
    LOGI("Viewer 1 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    auto ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    JsObject main_config;

    {
        // load config
        Sys::AssetFile config_file("assets/config.json", Sys::AssetFile::IN);
        LOGI("Viewer retrieving size !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        size_t config_file_size = config_file.size();
        LOGI("Viewer s=%i !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", int(config_file_size));
        std::unique_ptr<char[]> buf(new char[config_file_size]);
        config_file.Read(buf.get(), config_file_size);

        std::stringstream ss;
        ss.write(buf.get(), config_file_size);

        if (!main_config.Read(ss)) {
            throw std::runtime_error("Unable to load main config!");
        }
    }

    LOGI("Viewer 2 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    const JsObject &ui_settings = main_config.at("ui_settings");

    {
        // load fonts
        auto font_storage = std::make_shared<FontStorage>();
        AddComponent(UI_FONTS_KEY, font_storage);

        const JsObject &fonts = ui_settings.at("fonts");
        for (auto &el : fonts.elements) {
            const std::string &name = el.first;
            const JsString &file_name = el.second;

            font_storage->LoadFont(name, file_name.val, ctx.get());
        }
    }

    LOGI("Viewer 3 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    {
        auto renderer = std::make_shared<Renderer>(*ctx);
        AddComponent(RENDERER_KEY, renderer);

        Ray::settings_t s;
        s.w = w;
        s.h = h;
        auto ray_renderer = Ray::CreateRenderer(s, Ray::RendererOCL);
        AddComponent(RAY_RENDERER_KEY, ray_renderer);

        auto scene_manager = std::make_shared<SceneManager>(*ctx, *renderer, *ray_renderer);
        AddComponent(SCENE_MANAGER_KEY, scene_manager);
    }

    LOGI("Viewer 4 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);
    input_manager->SetConverter(InputManager::RAW_INPUT_P1_MOVE, nullptr);
    input_manager->SetConverter(InputManager::RAW_INPUT_P2_MOVE, nullptr);

    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    state_manager->Push(GSCreate(GS_DRAW_TEST, this));
}

void Viewer::Resize(int w, int h) {
    GameBase::Resize(w, h);
}

