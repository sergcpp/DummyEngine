#include "Renderer.h"

#include <chrono>

#include <Ren/Context.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>

namespace RendererInternal {
bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]);

extern const uint8_t bbox_indices[];

const bool USE_TWO_THREADS = true;
const int SHADOWMAP_WIDTH = 2048,
          SHADOWMAP_HEIGHT = 1024;

// Sun shadow occupies half of atlas
const int SUN_SHADOW_RES = SHADOWMAP_WIDTH / 2;
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
        shadow_buf_ = FrameBuf(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, nullptr, 0, true, Ren::BilinearNoMipmap);

        // Reserve space for sun shadow
        const int sun_shadow_res[] = { SUN_SHADOW_RES, SUN_SHADOW_RES };
        int sun_shadow_pos[2];
        int id = shadow_splitter_.Allocate(sun_shadow_res, sun_shadow_pos);
        assert(id != -1 && sun_shadow_pos[0] == 0 && sun_shadow_pos[1] == 0);
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

    /*try {
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::RawR32F, Ren::NoFilter, Ren::ClampToEdge, true);
    } catch (std::runtime_error &) {
        LOGI("Cannot create floating-point shadow buffer! Fallback to unsigned byte.");
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::RawRGB888, Ren::NoFilter, Ren::ClampToEdge, true);
    }*/

    if (USE_TWO_THREADS) {
        background_thread_ = std::thread(std::bind(&Renderer::BackgroundProc, this));
    }

    for (int i = 0; i < 2; i++) {
        cells_[i].resize(CELLS_COUNT);
        items_[i].resize(MAX_ITEMS_TOTAL);
    }
}

Renderer::~Renderer() {
    if (background_thread_.joinable()) {
        shutdown_ = notified_ = true;
        thr_notify_.notify_all();
        background_thread_.join();
    }
    DestroyRendererInternal();
    swCullCtxDestroy(&cull_ctx_);
}

void Renderer::DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, uint32_t root_index,
                           const SceneObject *objects, const uint32_t *obj_indices, uint32_t object_count, const Environment &env,
                           const TextureAtlas &decals_atlas) {
    using namespace RendererInternal;

    uint64_t gpu_draw_start = 0;
    if (render_flags_[0] & DebugTimings) {
        gpu_draw_start = GetGpuTimeBlockingUs();
    }
    auto cpu_draw_start = std::chrono::high_resolution_clock::now();

    if (USE_TWO_THREADS) {
        // Delegate gathering to background thread
        SwapDrawLists(cam, nodes, root_index, objects, obj_indices, object_count, env, &decals_atlas);
    } else {
        // Gather objects in main thread
        GatherDrawables(cam, render_flags_[0], env, nodes, root_index, objects, obj_indices, object_count,
                        transforms_[0], draw_lists_[0], light_sources_[0], decals_[0], cells_[0].data(),
                        items_[0].data(), items_count_[0], shadow_cams_[0], shadow_list_[0], frontend_infos_[0]);
    }
    
    {
        size_t transforms_count = transforms_[0].size();
        const auto *transforms = (transforms_count == 0) ? nullptr : &transforms_[0][0];

        size_t drawables_count = draw_lists_[0].size();
        const auto *drawables = (drawables_count == 0) ? nullptr : &draw_lists_[0][0];

        size_t lights_count = light_sources_[0].size();
        const auto *lights = (lights_count == 0) ? nullptr : &light_sources_[0][0];

        size_t decals_count = decals_[0].size();
        const auto *decals = (decals_count == 0) ? nullptr : &decals_[0][0];

        const auto *cells = cells_[0].empty() ? nullptr : &cells_[0][0];

        size_t items_count = items_count_[0];
        const auto *items = (items_count == 0) ? nullptr : &items_[0][0];

        const auto *p_decals_atlas = decals_atlas_[0];

        Ren::Mat4f shadow_transforms[4];
        size_t shadow_drawables_count[4];
        const DrawableItem *shadow_drawables[4];

        for (int i = 0; i < 4; i++) {
            Ren::Mat4f view_from_world = shadow_cams_[0][i].view_matrix(),
                       clip_from_view = shadow_cams_[0][i].proj_matrix();
            shadow_transforms[i] = clip_from_view * view_from_world;
            shadow_drawables_count[i] = shadow_list_[0][i].size();
            shadow_drawables[i] = (shadow_drawables_count[i] == 0) ? nullptr : &shadow_list_[0][i][0];
        }

        if (ctx_.w() != w_ || ctx_.h() != h_) {
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
                ssao_buf_ = FrameBuf(ctx_.w() / 2, ctx_.h() / 2, &desc, 1, false);
            }
            {   // Auxilary buffer for SSR
                FrameBuf::ColorAttachmentDesc desc;
                desc.format = Ren::RawRGB16F;
                desc.filter = Ren::Bilinear;
                desc.repeat = Ren::ClampToEdge;
                down_buf_ = FrameBuf(ctx_.w() / 4, ctx_.h() / 4, &desc, 1, false);
            }
            {   // Auxilary buffers for bloom effect
                FrameBuf::ColorAttachmentDesc desc;
                desc.format = Ren::RawRGBA16F;
                desc.filter = Ren::BilinearNoMipmap;
                desc.repeat = Ren::ClampToEdge;
                blur_buf1_ = FrameBuf(ctx_.w() / 4, ctx_.h() / 4, &desc, 1, false);
                blur_buf2_ = FrameBuf(ctx_.w() / 4, ctx_.h() / 4, &desc, 1, false);
            }
            w_ = ctx_.w();
            h_ = ctx_.h();
            LOGI("CleanBuf resized to %ix%i", w_, h_);
        }

        auto &_cam = USE_TWO_THREADS ? draw_cam_ : cam;
        auto &_env = USE_TWO_THREADS ? env_ : env;

        DrawObjectsInternal(_cam, render_flags_[0], transforms, drawables, drawables_count, lights, lights_count, decals, decals_count,
                            cells, items, items_count, shadow_transforms, shadow_drawables, shadow_drawables_count, _env,
                            p_decals_atlas);
    }
    
    auto cpu_draw_end = std::chrono::high_resolution_clock::now();

    // store values for current frame
    backend_cpu_start_ = (uint64_t)std::chrono::duration<double, std::micro>{ cpu_draw_start.time_since_epoch() }.count();
    backend_cpu_end_ = (uint64_t)std::chrono::duration<double, std::micro>{ cpu_draw_end.time_since_epoch() }.count();
    backend_time_diff_ = int64_t(gpu_draw_start) - int64_t(backend_cpu_start_);
    frame_counter_++;
}

void Renderer::WaitForBackgroundThreadIteration() {
    SwapDrawLists(draw_cam_, nullptr, 0, nullptr, nullptr, 0, env_, nullptr);
}

void Renderer::BackgroundProc() {
    using namespace RendererInternal;

    std::unique_lock<std::mutex> lock(mtx_);
    while (!shutdown_) {
        while (!notified_) {
            thr_notify_.wait(lock);
        }

        if (nodes_ && objects_) {
            std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
            
            GatherDrawables(draw_cam_, render_flags_[1], env_, nodes_, root_node_, objects_, obj_indices_, object_count_,
                            transforms_[1], draw_lists_[1], light_sources_[1], decals_[1], cells_[1].data(),
                            items_[1].data(), items_count_[1], shadow_cams_[1], shadow_list_[1], frontend_infos_[1]);
        }

        notified_ = false;
    }
}

void Renderer::SwapDrawLists(const Ren::Camera &cam, const bvh_node_t *nodes, uint32_t root_node,
                             const SceneObject *objects, const uint32_t *obj_indices, uint32_t object_count, const Environment &env,
                             const TextureAtlas *decals_atlas) {
    using namespace RendererInternal;

    bool should_notify = false;

    if (USE_TWO_THREADS) {
        std::unique_lock<std::mutex> lock(mtx_);
        while (notified_) {
            thr_done_.wait(lock);
        }
    }

    {
        std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
        std::swap(transforms_[0], transforms_[1]);
        std::swap(draw_lists_[0], draw_lists_[1]);
        std::swap(light_sources_[0], light_sources_[1]);
        std::swap(decals_[0], decals_[1]);
        std::swap(cells_[0], cells_[1]);
        std::swap(items_[0], items_[1]);
        std::swap(items_count_[0], items_count_[1]);
        std::swap(decals_atlas_[0], decals_atlas_[1]);
        decals_atlas_[1] = decals_atlas;
        nodes_ = nodes;
        root_node_ = root_node;
        objects_ = objects;
        obj_indices_ = obj_indices;
        object_count_ = object_count;
        render_flags_[1] = render_flags_[0];
        frontend_infos_[0] = frontend_infos_[1];
        render_infos_[0] = render_infos_[1];
        draw_cam_ = cam;
        for (int i = 0; i < 4; i++) {
            std::swap(shadow_list_[0][i], shadow_list_[1][i]);
            std::swap(shadow_cams_[0][i], shadow_cams_[1][i]);
        }
        env_ = env;
        std::swap(depth_pixels_[0], depth_pixels_[1]);
        std::swap(depth_tiles_[0], depth_tiles_[1]);
        if (nodes != nullptr) {
            should_notify = true;
        } else {
            draw_lists_[1].clear();
        }
    }

    if (USE_TWO_THREADS && should_notify) {
        notified_ = true;
        thr_notify_.notify_all();
    }
}

#undef BBOX_POINTS