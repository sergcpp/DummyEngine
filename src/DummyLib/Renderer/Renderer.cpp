#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

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

const Ren::Vec2f HaltonSeq23[] = { { 0.0000000000f, 0.0000000000f }, { 0.5000000000f, 0.3333333430f },
                                   { 0.2500000000f, 0.6666666870f }, { 0.7500000000f, 0.1111111190f },
                                   { 0.1250000000f, 0.4444444780f }, { 0.6250000000f, 0.7777778510f },
                                   { 0.3750000000f, 0.2222222390f }, { 0.8750000000f, 0.5555555820f },
                                   { 0.0625000000f, 0.8888889550f }, { 0.5625000000f, 0.0370370410f },
                                   { 0.3125000000f, 0.3703704180f }, { 0.8125000000f, 0.7037037610f },
                                   { 0.1875000000f, 0.1481481640f }, { 0.6875000000f, 0.4814815220f },
                                   { 0.4375000000f, 0.8148149250f }, { 0.9375000000f, 0.2592592840f },
                                   { 0.0312500000f, 0.5925926570f }, { 0.5312500000f, 0.9259260300f },
                                   { 0.2812500000f, 0.0740740821f }, { 0.7812500000f, 0.4074074630f },
                                   { 0.1562500000f, 0.7407408360f }, { 0.6562500000f, 0.1851852090f },
                                   { 0.4062500000f, 0.5185185670f }, { 0.9062500000f, 0.8518519400f },
                                   { 0.0937500000f, 0.2962963280f }, { 0.5937500000f, 0.6296296720f },
                                   { 0.3437500000f, 0.9629630450f }, { 0.8437500000f, 0.0123456810f },
                                   { 0.2187500000f, 0.3456790750f }, { 0.7187500000f, 0.6790124770f },
                                   { 0.4687500000f, 0.1234568060f }, { 0.9687500000f, 0.4567902090f },
                                   { 0.0156250000f, 0.7901235820f }, { 0.5156250000f, 0.2345679400f },
                                   { 0.2656250000f, 0.5679013130f }, { 0.7656250000f, 0.9012346860f },
                                   { 0.1406250000f, 0.0493827239f }, { 0.6406250000f, 0.3827161190f },
                                   { 0.3906250000f, 0.7160494920f }, { 0.8906250000f, 0.1604938510f },
                                   { 0.0781250000f, 0.4938272240f }, { 0.5781250000f, 0.8271605970f },
                                   { 0.3281250000f, 0.2716049850f }, { 0.8281250000f, 0.6049383880f },
                                   { 0.2031250000f, 0.9382717610f }, { 0.7031250000f, 0.0864197686f },
                                   { 0.4531250000f, 0.4197531640f }, { 0.9531250000f, 0.7530865670f },
                                   { 0.0468750000f, 0.1975308950f }, { 0.5468750000f, 0.5308642980f },
                                   { 0.2968750000f, 0.8641976710f }, { 0.7968750000f, 0.3086420300f },
                                   { 0.1718750000f, 0.6419754030f }, { 0.6718750000f, 0.9753087760f },
                                   { 0.4218750000f, 0.0246913619f }, { 0.9218750000f, 0.3580247460f },
                                   { 0.1093750000f, 0.6913581490f }, { 0.6093750000f, 0.1358024920f },
                                   { 0.3593750000f, 0.4691358800f }, { 0.8593750000f, 0.8024692540f },
                                   { 0.2343750000f, 0.2469136120f }, { 0.7343750000f, 0.5802469850f },
                                   { 0.4843750000f, 0.9135804180f }, { 0.9843750000f, 0.0617284030f } };
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

    {   // shadow map buffer
        shadow_buf_ = FrameBuf(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, nullptr, 0, true, Ren::NoFilter);
    }

    {   // aux buffer which gathers frame luminance
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::RawR16F;
        desc.filter = Ren::BilinearNoMipmap;
        desc.repeat = Ren::ClampToEdge;
        reduced_buf_ = FrameBuf(16, 8, &desc, 1, false);
    }

    {   // buffer used to sample probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::RawRGBA32F;
        desc.filter = Ren::NoFilter;
        desc.repeat = Ren::ClampToEdge;
        probe_sample_buf_ = FrameBuf(24, 8, &desc, 1, false);
    }

    // Compile built-in shadres etc.
    InitRendererInternal();

    uint8_t black[] = { 0, 0, 0, 0 }, white[] = { 255, 255, 255, 255 };

    Ren::Texture2DParams p;
    p.w = p.h = 1;
    p.format = Ren::RawRGBA8888;
    p.filter = Ren::Bilinear;

    Ren::eTexLoadStatus status;
    dummy_black_ = ctx_.LoadTexture2D("dummy_black", black, sizeof(black), p, &status);
    assert(status == Ren::TexCreatedFromData);

    dummy_white_ = ctx_.LoadTexture2D("dummy_white", white, sizeof(white), p, &status);
    assert(status == Ren::TexCreatedFromData);

    p.w = p.h = 8;
    p.format = Ren::RawRG32F;
    p.filter = Ren::NoFilter;
    rand2d_8x8_ = ctx_.LoadTexture2D("rand2d_8x8", &HaltonSeq23[0], sizeof(HaltonSeq23), p, &status);
    assert(status == Ren::TexCreatedFromData);

    temp_sub_frustums_.data.reset(new Ren::Frustum[REN_CELLS_COUNT]);
    temp_sub_frustums_.count = temp_sub_frustums_.capacity = REN_CELLS_COUNT;

    decals_boxes_.data.reset(new BBox[REN_MAX_DECALS_TOTAL]);
    decals_boxes_.capacity = REN_MAX_DECALS_TOTAL;
    decals_boxes_.count = 0;

    litem_to_lsource_.data.reset(new const LightSource *[REN_MAX_LIGHTS_TOTAL]);
    litem_to_lsource_.capacity = REN_MAX_LIGHTS_TOTAL;
    litem_to_lsource_.count = 0;

    ditem_to_decal_.data.reset(new const Decal *[REN_MAX_DECALS_TOTAL]);
    ditem_to_decal_.capacity = REN_MAX_DECALS_TOTAL;
    ditem_to_decal_.count = 0;

    allocated_shadow_regions_.data.reset(new ShadReg[REN_MAX_SHADOWMAPS_TOTAL]);
    allocated_shadow_regions_.capacity = REN_MAX_SHADOWMAPS_TOTAL;
    allocated_shadow_regions_.count = 0;

    for (int i = 0; i < 2; i++) {
        temp_sort_spans_32_[i].data.reset(new SortSpan32[REN_MAX_SHADOW_BATCHES]);
        temp_sort_spans_32_[i].capacity = REN_MAX_SHADOW_BATCHES;
        temp_sort_spans_32_[i].count = 0;

        temp_sort_spans_64_[i].data.reset(new SortSpan64[REN_MAX_MAIN_BATCHES]);
        temp_sort_spans_64_[i].capacity = REN_MAX_MAIN_BATCHES;
        temp_sort_spans_64_[i].count = 0;
    }
}

Renderer::~Renderer() {
    DestroyRendererInternal();
    swCullCtxDestroy(&cull_ctx_);
}

void Renderer::PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, DrawList &list) {
    GatherDrawables(scene, cam, list);
}

void Renderer::ExecuteDrawList(const DrawList &list, const FrameBuf *target) {
    using namespace RendererInternal;

    uint64_t gpu_draw_start = 0;
    if (list.render_flags & DebugTimings) {
        gpu_draw_start = GetGpuTimeBlockingUs();
    }
    uint64_t cpu_draw_start_us = Sys::GetTimeUs();
    
    if (ctx_.w() != scr_w_ || ctx_.h() != scr_h_) {
        {   // Main buffer for raw frame before tonemapping
            FrameBuf::ColorAttachmentDesc desc[3];
            {   // Main color
                desc[0].format = Ren::RawRGB16F;
                desc[0].filter = Ren::NoFilter;
                desc[0].repeat = Ren::ClampToEdge;
            }
            {   // 4-component world-space normal (alpha is 'ssr' flag)
                desc[1].format = Ren::RawRGB10_A2;
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

        {   // Buffer that holds tonemapped ldr frame before fxaa applied
            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawRGB888;
            desc.filter = Ren::BilinearNoMipmap;
            desc.repeat = Ren::ClampToEdge;
            combined_buf_ = FrameBuf(ctx_.w(), ctx_.h(), &desc, 1, false);
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
            desc.format = Ren::RawRGB10_A2;
            desc.filter = Ren::NoFilter;
            desc.repeat = Ren::ClampToEdge;
            refl_buf_ = FrameBuf(clean_buf_.w / 2, clean_buf_.h / 2, &desc, 1, false);
        }
        {   // Buffer that holds previous frame (used for SSR)
            int base_res = upper_power_of_two(clean_buf_.w) / 4;

            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawRGB16F;
            desc.filter = Ren::BilinearNoMipmap;
            desc.repeat = Ren::ClampToEdge;
            down_buf_ = FrameBuf(base_res, base_res / 2, &desc, 1, false);
        }
        {   // Auxilary buffers for bloom effect
            FrameBuf::ColorAttachmentDesc desc;
            desc.format = Ren::RawRGB16F;
            desc.filter = Ren::BilinearNoMipmap;
            desc.repeat = Ren::ClampToEdge;
            blur_buf1_ = FrameBuf(clean_buf_.w / 4, clean_buf_.h / 4, &desc, 1, false);
            blur_buf2_ = FrameBuf(clean_buf_.w / 4, clean_buf_.h / 4, &desc, 1, false);
        }

        // Memory consumption for FullHD frame (except clean_buf_):
        // combined_buf_    : ~5.93 Mb
        // ssao_buf_        : ~0.49 Mb
        // refl_buf_        : ~1.97 Mb
        // down_buf_        : ~0.75 Mb
        // blur_buf1_       : ~0.74 Mb
        // blur_buf2_       : ~0.74 Mb
        // Total            : ~10.62 Mb

        InitFramebuffersInternal();

        scr_w_ = ctx_.w();
        scr_h_ = ctx_.h();
        LOGI("CleanBuf resized to %ix%i", scr_w_, scr_h_);
    }

    if (!target) {
        // TODO: Change actual resolution dynamically
#if defined(__ANDROID__)
        act_w_ = int(scr_w_ * 0.4f);
        act_h_ = int(scr_h_ * 0.6f);
#else
        act_w_ = int(scr_w_ * 1.0f);
        act_h_ = int(scr_h_ * 1.0f);
#endif
    } else {
        act_w_ = target->w;
        act_h_ = target->h;
        assert(act_w_ <= scr_w_ && act_h_ <= scr_h_);
    }

    DrawObjectsInternal(list, target);
    
    uint64_t cpu_draw_end_us = Sys::GetTimeUs();

    // store values for current frame
    backend_cpu_start_ = cpu_draw_start_us;
    backend_cpu_end_ = cpu_draw_end_us;
    backend_time_diff_ = int64_t(gpu_draw_start) - int64_t(backend_cpu_start_);
    frame_counter_++;
}

#undef BBOX_POINTS