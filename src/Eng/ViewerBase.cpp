#include "ViewerBase.h"

#include <random>
#include <thread>

#include <Ren/Context.h>
#include <Snd/Context.h>
#include <Sys/AssetFileIO.h>
#include <Sys/Json.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>
#include <optick/optick.h>

#include "Log.h"
#include "ViewerStateManager.h"
#include "gui/BaseElement.h"
#include "gui/Renderer.h"
#include "renderer/Renderer.h"
#include "scene/PhysicsManager.h"
#include "scene/SceneManager.h"
#include "utils/Cmdline.h"
#include "utils/FlowControl.h"
#include "utils/Random.h"
#include "utils/ShaderLoader.h"

Eng::ViewerBase::ViewerBase(const int w, const int h, const int validation_level, ILog *log, const char *device_name)
    : log_(log), width(w), height(h) {
    terminated = false;

    //Sys::InitWorker();

    ren_ctx_ = std::make_unique<Ren::Context>();
    if (!ren_ctx_->Init(w, h, log_, validation_level, device_name)) {
        throw std::runtime_error("Initialization failed!");
    }
    InitOptickGPUProfiler();

    snd_ctx_ = std::make_unique<Snd::Context>();
    snd_ctx_->Init(log_);

#if !defined(__EMSCRIPTEN__)
    unsigned int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    threads_ = std::make_unique<Sys::ThreadPool>(num_threads, Sys::eThreadPriority::Normal, "worker");
#endif

    input_manager_ = std::make_unique<InputManager>();

    shader_loader_ = std::make_unique<ShaderLoader>();

    flow_control_ = std::make_unique<FlowControl>(2 * NET_UPDATE_DELTA, NET_UPDATE_DELTA);

    random_ = std::make_unique<Random>(std::random_device{}());

    renderer_ = std::make_unique<Renderer>(*ren_ctx_, *shader_loader_, *random_, *threads_);

    cmdline_ = std::make_unique<Cmdline>();

    physics_manager_ = std::make_unique<PhysicsManager>();

    {
        using namespace std::placeholders;

        path_config_t paths;
        scene_manager_ = std::make_unique<SceneManager>(*ren_ctx_, *shader_loader_, snd_ctx_.get(), *threads_, paths);
        scene_manager_->SetPipelineInitializer(
            std::bind(&Renderer::InitPipelinesForProgram, renderer(), _1, _2, _3, _4));
    }

    state_manager_ = std::make_unique<ViewerStateManager>();

    ui_renderer_ = std::make_unique<Gui::Renderer>(*ren_ctx_);
    if (!ui_renderer_->Init()) {
        throw std::runtime_error("Couldn't initialize UI renderer!");
    }

    ui_root_ = std::make_unique<Gui::RootElement>(Gui::Vec2i(w, h));
}

Eng::ViewerBase::~ViewerBase() = default;

void Eng::ViewerBase::Resize(const int w, const int h) {
    width = w;
    height = h;

    ren_ctx_->Resize(width, height);

    ui_root_->set_zone(Ren::Vec2i{width, height});
    ui_root_->Resize(nullptr);
}

void Eng::ViewerBase::Start() {}

void Eng::ViewerBase::Frame() {
    OPTICK_EVENT("ViewerBase::Frame");

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

void Eng::ViewerBase::Quit() { terminated = true; }

#if defined(USE_VK_RENDER)

#include <Ren/VKCtx.h>

void Eng::ViewerBase::InitOptickGPUProfiler() {
    Ren::ApiContext *api_ctx = ren_ctx_->api_ctx();

    Optick::VulkanFunctions functions = {
        api_ctx->vkGetPhysicalDeviceProperties,
        (PFN_vkCreateQueryPool_)api_ctx->vkCreateQueryPool,
        (PFN_vkCreateCommandPool_)api_ctx->vkCreateCommandPool,
        (PFN_vkAllocateCommandBuffers_)api_ctx->vkAllocateCommandBuffers,
        (PFN_vkCreateFence_)api_ctx->vkCreateFence,
        api_ctx->vkCmdResetQueryPool,
        (PFN_vkQueueSubmit_)api_ctx->vkQueueSubmit,
        (PFN_vkWaitForFences_)api_ctx->vkWaitForFences,
        (PFN_vkResetCommandBuffer_)api_ctx->vkResetCommandBuffer,
        (PFN_vkCmdWriteTimestamp_)api_ctx->vkCmdWriteTimestamp,
        (PFN_vkGetQueryPoolResults_)api_ctx->vkGetQueryPoolResults,
        (PFN_vkBeginCommandBuffer_)api_ctx->vkBeginCommandBuffer,
        (PFN_vkEndCommandBuffer_)api_ctx->vkEndCommandBuffer,
        (PFN_vkResetFences_)api_ctx->vkResetFences,
        api_ctx->vkDestroyCommandPool,
        api_ctx->vkDestroyQueryPool,
        api_ctx->vkDestroyFence,
        api_ctx->vkFreeCommandBuffers,
    };

    OPTICK_GPU_INIT_VULKAN(&api_ctx->device, &api_ctx->physical_device, &api_ctx->graphics_queue,
                           &api_ctx->graphics_family_index, 1, &functions);
}

#else

void Eng::ViewerBase::InitOptickGPUProfiler() {}

#endif