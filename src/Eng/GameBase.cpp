#include "GameBase.h"

#include <random>
#include <thread>

#include <Gui/BaseElement.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Json.h>
#include <Sys/Time_.h>
#include <Sys/ThreadPool.h>

#include "FlowControl.h"
#include "GameStateManager.h"
#include "Random.h"

GameBase::GameBase(int w, int h, const char * /*local_dir*/) : width(w), height(h) {
    terminated = false;

    Sys::InitWorker();

    auto ctx = std::make_shared<Ren::Context>();
    ctx->Init(w, h);
    AddComponent(REN_CONTEXT_KEY, ctx);

#if !defined(__EMSCRIPTEN__)
    auto num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    auto pool = std::make_shared<Sys::ThreadPool>(num_threads);
    AddComponent(THREAD_POOL_KEY, pool);
#endif

    auto state_manager = std::make_shared<GameStateManager>();
    AddComponent(STATE_MANAGER_KEY, state_manager);

    auto input_manager = std::make_shared<InputManager>();
    AddComponent(INPUT_MANAGER_KEY, input_manager);

    auto flow_control = std::make_shared<FlowControl>(2 * NET_UPDATE_DELTA, NET_UPDATE_DELTA);
    AddComponent(FLOW_CONTROL_KEY, flow_control);

    auto random_engine = std::make_shared<Random>(std::random_device{}());
    AddComponent(RANDOM_KEY, random_engine);

    JsObject config;
    config[Gui::GL_DEFINES_KEY] = JsString{ "" };
    auto ui_renderer = std::make_shared<Gui::Renderer>(*ctx.get(), config);
    AddComponent(UI_RENDERER_KEY, ui_renderer);

    auto ui_root = std::make_shared<Gui::RootElement>(Gui::Vec2i(w, h));
    AddComponent(UI_ROOT_KEY, ui_root);
}

GameBase::~GameBase() {
    // context should be deleted last
    auto ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    while (ctx->ProcessTasks());
    components_.clear();

    Sys::StopWorker();
}

void GameBase::Resize(int w, int h) {
    width = w;
    height = h;

    auto ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    ctx->Resize(width, height);

    auto ui_root = GetComponent<Gui::RootElement>(UI_ROOT_KEY);
    ui_root->set_zone({ width, height });
    ui_root->Resize(nullptr);
}

void GameBase::Start() {

}

void GameBase::Frame() {
    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);

    //PROFILE_FUNC();

    FrameInfo &fr = fr_info_;

    fr.cur_time = Sys::GetTicks();
    if (fr.cur_time < fr.prev_time)fr.prev_time = 0;
    fr.delta_time = fr.cur_time - fr.prev_time;
    if (fr.delta_time > 200) {
        fr.delta_time = 200;
    }
    fr.prev_time = fr.cur_time;
    fr.time_acc += fr.delta_time;

    Sys::cached_time = fr.cur_time - fr.time_acc;

    {
        //PROFILE_BLOCK(Update);
        while (fr.time_acc >= UPDATE_DELTA) {
            InputManager::Event evt;
            while (input_manager->PollEvent(Sys::cached_time, evt)) {
                state_manager->HandleInput(evt);
            }

            state_manager->Update(UPDATE_DELTA);
            fr.time_acc -= UPDATE_DELTA;

            Sys::cached_time += UPDATE_DELTA;
        }
    }

    fr.time_fract = float(fr.time_acc) / UPDATE_DELTA;

    {
        //PROFILE_BLOCK(Draw);
        state_manager->Draw(0.001f * fr_info_.delta_time);
    }
}

void GameBase::Quit() {
    terminated = true;
}

void GameBase::RegisterCommand(const std::string &cmd, const CommandHandler &handler) {
    command_handers_[cmd] = handler;
}

bool GameBase::ExecuteCommand(const std::string &cmd, const std::vector<std::string> &args) {
    auto it = command_handers_.find(cmd);
    if (it != command_handers_.end()) {
        return it->second(args);
    }
    return false;
}