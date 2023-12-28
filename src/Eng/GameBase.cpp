#include "GameBase.h"

#include <random>
#include <thread>

#include <Ren/Context.h>
#include <Snd/Context.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Json.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>
#include <optick/optick.h>

#include "FlowControl.h"
#include "GameStateManager.h"
#include "gui/BaseElement.h"
#include "gui/Renderer.h"
#include "Log.h"
#include "Random.h"
#include "renderer/Renderer.h"
#include "scene/PhysicsManager.h"
#include "scene/SceneManager.h"
#include "utils/Cmdline.h"
#include "utils/ShaderLoader.h"

Eng::GameBase::GameBase(const int w, const int h, const int validation_level, const char *device_name)
    : width(w), height(h) {
    terminated = false;

    Sys::InitWorker();

    log_ =
#if !defined(__ANDROID__)
        std::make_unique<LogStdout>();
#else
        std::make_unique<LogAndroid>("APP_JNI");
#endif

    ren_ctx_ = std::make_unique<Ren::Context>();
    if (!ren_ctx_->Init(w, h, log_.get(), validation_level, device_name)) {
        throw std::runtime_error("Initialization failed!");
    }
    InitOptickGPUProfiler();

    snd_ctx_ = std::make_unique<Snd::Context>();
    snd_ctx_->Init(log_.get());

#if !defined(__EMSCRIPTEN__)
    unsigned int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    threads_ = std::make_unique<Sys::ThreadPool>(num_threads, Sys::eThreadPriority::Normal, "worker");
#endif

    input_manager_ = std::make_unique<InputManager>();

    shader_loader_ = std::make_unique<ShaderLoader>();

    flow_control_ = std::make_unique<FlowControl>(2 * NET_UPDATE_DELTA, NET_UPDATE_DELTA);

    random_ = std::make_unique<Random>(std::random_device{}());

    renderer_ = std::make_unique<Eng::Renderer>(*ren_ctx_, *shader_loader_, *random_, *threads_);

    cmdline_ = std::make_unique<Eng::Cmdline>();

    physics_manager_ = std::make_unique<Eng::PhysicsManager>();

    state_manager_ = std::make_unique<GameStateManager>();

    ui_renderer_ = std::make_unique<Gui::Renderer>(*ren_ctx_);
    if (!ui_renderer_->Init()) {
        throw std::runtime_error("Couldn't initialize UI renderer!");
    }

    ui_root_ = std::make_unique<Gui::RootElement>(Gui::Vec2i(w, h));
}

Eng::GameBase::~GameBase() = default;

void Eng::GameBase::Resize(const int w, const int h) {
    width = w;
    height = h;

    ren_ctx_->Resize(width, height);

    ui_root_->set_zone(Ren::Vec2i{width, height});
    ui_root_->Resize(nullptr);
}

void Eng::GameBase::Start() {}

void Eng::GameBase::Frame() {
    OPTICK_EVENT("GameBase::Frame");

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
        while (input_manager_->PollEvent(poll_time_point, evt)) {
            state_manager_->HandleInput(evt);
        }

        state_manager_->UpdateFixed(UPDATE_DELTA);
        fr.time_acc_us -= UPDATE_DELTA;

        poll_time_point += UPDATE_DELTA;
    }

    fr.time_fract = double(fr.time_acc_us) / UPDATE_DELTA;

    {
        OPTICK_EVENT("state_manager->UpdateAnim");
        state_manager_->UpdateAnim(fr_info_.delta_time_us);
    }
    {
        OPTICK_EVENT("state_manager->Draw");
        state_manager_->Draw();
    }
}

void Eng::GameBase::Quit() { terminated = true; }

#if defined(USE_VK_RENDER)

#include <Ren/VKCtx.h>

void Eng::GameBase::InitOptickGPUProfiler() {
    Ren::ApiContext *api_ctx = ren_ctx_->api_ctx();

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