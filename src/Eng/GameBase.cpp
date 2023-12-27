#include "GameBase.h"

#include <random>
#include <thread>

#include <Eng/Gui/BaseElement.h>
#include <Eng/Gui/Renderer.h>
#include <Ren/Context.h>
#include <Snd/Context.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Json.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>
#include <optick/optick.h>

#include "FlowControl.h"
#include "GameStateManager.h"
#include "Log.h"
#include "Random.h"
#include "Utils/ShaderLoader.h"

Eng::GameBase::GameBase(const int w, const int h, const int validation_level, const char *device_name)
    : width(w), height(h) {
    terminated = false;

    Sys::InitWorker();

    auto log =
#if !defined(__ANDROID__)
        std::make_shared<LogStdout>();
#else
        std::make_shared<LogAndroid>("APP_JNI");
#endif
    AddComponent(LOG_KEY, log);

    auto ren_ctx = std::make_shared<Ren::Context>();
    if (!ren_ctx->Init(w, h, log.get(), validation_level, device_name)) {
        throw std::runtime_error("Initialization failed!");
    }
    AddComponent(REN_CONTEXT_KEY, ren_ctx);
    InitOptickGPUProfiler();

    auto snd_ctx = std::make_shared<Snd::Context>();
    snd_ctx->Init(log.get());
    AddComponent(SND_CONTEXT_KEY, snd_ctx);

#if !defined(__EMSCRIPTEN__)
    unsigned int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    threads_ = std::make_unique<Sys::ThreadPool>(num_threads, Sys::eThreadPriority::Normal, "worker");
#endif

    auto state_manager = std::make_shared<GameStateManager>();
    AddComponent(STATE_MANAGER_KEY, state_manager);

    auto input_manager = std::make_shared<InputManager>();
    AddComponent(INPUT_MANAGER_KEY, input_manager);

    auto shader_loader = std::make_shared<ShaderLoader>();
    AddComponent(SHADER_LOADER_KEY, shader_loader);

    auto flow_control = std::make_shared<FlowControl>(2 * NET_UPDATE_DELTA, NET_UPDATE_DELTA);
    AddComponent(FLOW_CONTROL_KEY, flow_control);

    auto random_engine = std::make_shared<Random>(std::random_device{}());
    AddComponent(RANDOM_KEY, random_engine);

    auto ui_renderer = std::make_shared<Gui::Renderer>(*ren_ctx);
    if (!ui_renderer->Init()) {
        throw std::runtime_error("Couldn't initialize UI renderer!");
    }
    AddComponent(UI_RENDERER_KEY, ui_renderer);

    auto ui_root = std::make_shared<Gui::RootElement>(Gui::Vec2i(w, h));
    AddComponent(UI_ROOT_KEY, ui_root);
}

Eng::GameBase::~GameBase() {
    // keep log alive during destruction
    auto log = GetComponent<Ren::ILog>(LOG_KEY);
    { // contexts should be deleted last
        auto ren_ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);
        auto snd_ctx = GetComponent<Snd::Context>(SND_CONTEXT_KEY);
        // finish file IO tasks
        while (!Sys::StopWorker())
            ren_ctx->ProcessTasks();
        // finish remaining tasks in queue
        while (ren_ctx->ProcessTasks())
            ;
        components_.clear();
    }
}

void Eng::GameBase::Resize(const int w, const int h) {
    width = w;
    height = h;

    auto ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    ctx->Resize(width, height);

    auto ui_root = GetComponent<Gui::RootElement>(UI_ROOT_KEY);
    ui_root->set_zone(Ren::Vec2i{width, height});
    ui_root->Resize(nullptr);
}

void Eng::GameBase::Start() {}

void Eng::GameBase::Frame() {
    OPTICK_EVENT("GameBase::Frame");

    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);

    FrameInfo &fr = fr_info_;

    fr.cur_time_us = Sys::GetTimeUs();
    if (fr.cur_time_us < fr.prev_time_us) {
        fr.prev_time_us = 0;
    }
    fr.delta_time_us = fr.cur_time_us - fr.prev_time_us;
    if (fr.delta_time_us > 200000) {
        fr.delta_time_us = 200000;
    }
    fr.prev_time_us = fr.cur_time_us;
    fr.time_acc_us += fr.delta_time_us;

    uint64_t poll_time_point = fr.cur_time_us - fr.time_acc_us;

    while (fr.time_acc_us >= UPDATE_DELTA) {
        InputManager::Event evt;
        while (input_manager->PollEvent(poll_time_point, evt)) {
            state_manager->HandleInput(evt);
        }

        state_manager->UpdateFixed(UPDATE_DELTA);
        fr.time_acc_us -= UPDATE_DELTA;

        poll_time_point += UPDATE_DELTA;
    }

    fr.time_fract = double(fr.time_acc_us) / UPDATE_DELTA;

    {
        OPTICK_EVENT("state_manager->UpdateAnim");
        state_manager->UpdateAnim(fr_info_.delta_time_us);
    }
    {
        OPTICK_EVENT("state_manager->Draw");
        state_manager->Draw();
    }
}

void Eng::GameBase::Quit() { terminated = true; }

#if defined(USE_VK_RENDER)

#include <Ren/VKCtx.h>

void Eng::GameBase::InitOptickGPUProfiler() {
    auto ren_ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    Ren::ApiContext *api_ctx = ren_ctx->api_ctx();

    Optick::VulkanFunctions functions = {
        vkGetPhysicalDeviceProperties,
        (PFN_vkCreateQueryPool_)vkCreateQueryPool,
        (PFN_vkCreateCommandPool_)vkCreateCommandPool,
        (PFN_vkAllocateCommandBuffers_)vkAllocateCommandBuffers,
        (PFN_vkCreateFence_)vkCreateFence,
        vkCmdResetQueryPool,
        (PFN_vkQueueSubmit_)vkQueueSubmit,
        (PFN_vkWaitForFences_)vkWaitForFences,
        (PFN_vkResetCommandBuffer_)vkResetCommandBuffer,
        (PFN_vkCmdWriteTimestamp_)vkCmdWriteTimestamp,
        (PFN_vkGetQueryPoolResults_)vkGetQueryPoolResults,
        (PFN_vkBeginCommandBuffer_)vkBeginCommandBuffer,
        (PFN_vkEndCommandBuffer_)vkEndCommandBuffer,
        (PFN_vkResetFences_)vkResetFences,
        vkDestroyCommandPool,
        vkDestroyQueryPool,
        vkDestroyFence,
        vkFreeCommandBuffers,
    };

    OPTICK_GPU_INIT_VULKAN(&api_ctx->device, &api_ctx->physical_device, &api_ctx->graphics_queue,
                           &api_ctx->graphics_family_index, 1, &functions);
}

#else

void Eng::GameBase::InitOptickGPUProfiler() {}

#endif