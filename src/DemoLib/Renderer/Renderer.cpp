#include "Renderer.h"

#include <chrono>

#include <Ren/Context.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>

namespace RendererInternal {
bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]);

extern const uint8_t bbox_indices[];

#if defined(__ANDROID__)
const int SHADOWMAP_WIDTH = REN_SHAD_RES_ANDROID;
#else
const int SHADOWMAP_WIDTH = REN_SHAD_RES_PC;
#endif
const int SHADOWMAP_HEIGHT = SHADOWMAP_WIDTH / 2;

// Sun shadow occupies half of atlas
const int SUN_SHADOW_RES = SHADOWMAP_WIDTH / 2;

int upper_power_of_two(int v) {
    int res = 1;
    while (res < v) {
        res *= 2;
    }
    return res;
}

}

#define BBOX_POINTS(min, max) \
    (min)[0], (min)[1], (min)[2],     \
    (max)[0], (min)[1], (min)[2],     \
    (min)[0], (min)[1], (max)[2],     \
    (max)[0], (min)[1], (max)[2],     \
    (min)[0], (max)[1], (min)[2],     \
    (max)[0], (max)[1], (min)[2],     \
    (min)[0], (max)[1], (max)[2],     \
    (max)[0], (max)[1], (max)[2]

Renderer::Renderer(Ren::Context &ctx, std::shared_ptr<Sys::ThreadPool> &threads)
    : ctx_(ctx), threads_(threads), shadow_splitter_(RendererInternal::SHADOWMAP_WIDTH, RendererInternal::SHADOWMAP_HEIGHT) {
    using namespace RendererInternal;

    {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;
        SWfloat z = FAR_CLIP / (FAR_CLIP - NEAR_CLIP) + (NEAR_CLIP - (2.0f * NEAR_CLIP)) / (0.15f * (FAR_CLIP - NEAR_CLIP));
        swCullCtxInit(&cull_ctx_, 256, 128, z);
    }

    {   // Create shadow map buffer
        shadow_buf_ = FrameBuf(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, nullptr, 0, true, Ren::NoFilter);
    }

    {   // Create aux buffer which gathers frame luminance
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::RawR16F;
        desc.filter = Ren::BilinearNoMipmap;
        desc.repeat = Ren::ClampToEdge;
        reduced_buf_ = FrameBuf(16, 8, &desc, 1, false);
    }

    // Compile built-in shadres etc.
    InitRendererInternal();

    uint8_t black[] = { 0, 0, 0, 0 }, white[] = { 255, 255, 255, 255 };

    Ren::Texture2DParams p;
    p.w = p.h = 1;
    p.format = Ren::RawRGBA8888;
    p.filter = Ren::Bilinear;

    Ren::eTexLoadStatus status;
    default_lightmap_ = ctx_.LoadTexture2D("default_lightmap", black, sizeof(black), p, &status);
    assert(status == Ren::TexCreatedFromData);

    default_ao_ = ctx_.LoadTexture2D("default_ao", white, sizeof(white), p, &status);
    assert(status == Ren::TexCreatedFromData);
}

Renderer::~Renderer() {
    DestroyRendererInternal();
    swCullCtxDestroy(&cull_ctx_);
}

void Renderer::PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, DrawList &list) {
    GatherDrawables(scene, cam, list);
}

void Renderer::ExecuteDrawList(DrawList &list) {
    using namespace RendererInternal;

    uint64_t gpu_draw_start = 0;
    if (list.render_flags & DebugTimings) {
        gpu_draw_start = GetGpuTimeBlockingUs();
    }
    auto cpu_draw_start = std::chrono::high_resolution_clock::now();
    
    if (ctx_.w() != scr_w_ || ctx_.h() != scr_h_) {
        {   // Main buffer for raw frame before tonemapping
            FrameBuf::ColorAttachmentDesc desc[3];
            {   // Main color
                desc[0].format = Ren::RawRGBA16F;
                desc[0].filter = Ren::NoFilter;
                desc[0].repeat = Ren::ClampToEdge;
            }
            {   // View-space normal
                desc[1].format = Ren::RawRG88;
                desc[1].filter = Ren::BilinearNoMipmap;
                desc[1].repeat = Ren::ClampToEdge;
            }
            {   // 4-component specular (alpha is roughness)
                desc[2].format = Ren::RawRGBA8888;
                desc[2].filter = Ren::BilinearNoMipmap;
                desc[2].repeat = Ren::ClampToEdge;
            }
            clean_buf_ = FrameBuf(ctx_.w(), ctx_.h(), desc, 3, true, Ren::NoFilter, 4);
        }
        {   // Buffer for SSAO
            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawR8;
            desc.filter = Ren::BilinearNoMipmap;
            desc.repeat = Ren::ClampToEdge;
            ssao_buf_ = FrameBuf(clean_buf_.w / REN_SSAO_BUF_RES_DIV, clean_buf_.h / REN_SSAO_BUF_RES_DIV, &desc, 1, false);
        }
        {   // Auxilary buffer for reflections
            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawRGB16F;
            desc.filter = Ren::Bilinear;
            desc.repeat = Ren::ClampToEdge;
            refl_buf_ = FrameBuf(clean_buf_.w / 2, clean_buf_.h / 2, &desc, 1, false);
        }
        {   // Buffer that holds previous frame (used for SSR)
            int base_res = upper_power_of_two(clean_buf_.w) / 4;

            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawRGB16F;
            desc.filter = Ren::Bilinear;
            desc.repeat = Ren::ClampToEdge;
            down_buf_ = FrameBuf(base_res, base_res / 2, &desc, 1, false);
        }
        {   // Auxilary buffers for bloom effect
            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawRGBA16F;
            desc.filter = Ren::BilinearNoMipmap;
            desc.repeat = Ren::ClampToEdge;
            blur_buf1_ = FrameBuf(clean_buf_.w / 4, clean_buf_.h / 4, &desc, 1, false);
            blur_buf2_ = FrameBuf(clean_buf_.w / 4, clean_buf_.h / 4, &desc, 1, false);
        }
        scr_w_ = ctx_.w();
        scr_h_ = ctx_.h();
        LOGI("CleanBuf resized to %ix%i", scr_w_, scr_h_);
    }

    // TODO: Change actual resolution dynamically
#if defined(__ANDROID__)
    act_w_ = int(scr_w_ * 0.4f);
    act_h_ = int(scr_h_ * 0.6f);
#else
    act_w_ = int(scr_w_ * 1.0f);
    act_h_ = int(scr_h_ * 1.0f);
#endif


    list.render_info.lights_count = (uint32_t)list.light_sources.size();
    list.render_info.lights_data_size = (uint32_t)list.light_sources.size() * sizeof(LightSourceItem);
    list.render_info.decals_count = (uint32_t)list.decals.size();
    list.render_info.decals_data_size = (uint32_t)list.decals.size() * sizeof(DecalItem);
    list.render_info.cells_data_size = (uint32_t)CELLS_COUNT * sizeof(CellData);
    list.render_info.items_data_size = (uint32_t)list.items.size() * sizeof(ItemData);

    DrawObjectsInternal(list);
    
    auto cpu_draw_end = std::chrono::high_resolution_clock::now();

    // store values for current frame
    backend_cpu_start_ = (uint64_t)std::chrono::duration<double, std::micro>{ cpu_draw_start.time_since_epoch() }.count();
    backend_cpu_end_ = (uint64_t)std::chrono::duration<double, std::micro>{ cpu_draw_end.time_since_epoch() }.count();
    backend_time_diff_ = int64_t(gpu_draw_start) - int64_t(backend_cpu_start_);
    frame_counter_++;
}

#undef BBOX_POINTS