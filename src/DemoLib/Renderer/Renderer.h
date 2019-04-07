#pragma once

#include <Ren/Camera.h>
#include <Ren/RingBuffer.h>
#include <Ren/TextureSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "FrameBuf.h"
#include "Renderer_GL_Defines.inl"
#include "../Scene/SceneData.h"

namespace Sys {
    class ThreadPool;
}

class TextureAtlas;

struct LightSourceItem {
    float pos[3], radius;
    float col[3], brightness;
    float dir[3], spot;
};
static_assert(sizeof(LightSourceItem) == 48, "!");

struct DecalItem {
    float mat[3][4];
    float diff[4], norm[4], spec[4];
};
static_assert(sizeof(DecalItem) == 24 * sizeof(float), "!");

struct CellData {
    uint32_t item_offset : 24;
    uint32_t light_count : 8;
    uint32_t decal_count : 8;
    uint32_t probe_count : 8;
    uint32_t _unused : 16;
};
static_assert(sizeof(CellData) == 8, "!");

struct ItemData {
    uint32_t light_index : 12;
    uint32_t decal_index : 12;
    uint32_t probe_index : 8;
};
static_assert(sizeof(ItemData) == 4, "!");

struct InstanceData {
    float model_matrix[3][4];
    float lmap_transform[4];
};
static_assert(sizeof(InstanceData) == 64, "!");

struct ShadowDrawBatch {
    uint32_t indices_offset, indices_count;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;

    bool operator<(const ShadowDrawBatch& rhs) const {
        return indices_offset < rhs.indices_offset;
    }
};

struct MainDrawBatch {
    uint32_t prog_id;
    uint32_t mat_id;

    uint32_t indices_offset, indices_count;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;

    bool operator<(const MainDrawBatch& rhs) const {
        return std::tie(prog_id, mat_id, indices_offset) < std::tie(rhs.prog_id, rhs.mat_id, rhs.indices_offset);
    }
};

struct ShadowList {
    uint32_t shadow_batch_start, shadow_batch_count;
};

#define MAX_STACK_SIZE 64

namespace RendererInternal {
    const int GRID_RES_X = REN_GRID_RES_X;
    const int GRID_RES_Y = REN_GRID_RES_Y;
    const int GRID_RES_Z = REN_GRID_RES_Z;

    const int CELLS_COUNT = GRID_RES_X * GRID_RES_Y * GRID_RES_Z;

    const int MAX_LIGHTS_PER_CELL = 255;
    const int MAX_DECALS_PER_CELL = 255;
    const int MAX_PROBES_PER_CELL = 8;
    const int MAX_ITEMS_PER_CELL = 255;

    const int MAX_LIGHTS_TOTAL = 4096;
    const int MAX_DECALS_TOTAL = 4096;
    const int MAX_PROBES_TOTAL = 256;
    const int MAX_ITEMS_TOTAL = (1 << 16);

    const int MAX_INSTANCES_TOTAL = 262144;
}

enum eRenderFlags {
    EnableZFill     = (1 << 0),
    EnableCulling   = (1 << 1),
    EnableSSR       = (1 << 2),
    EnableSSAO      = (1 << 3),
    DebugWireframe  = (1 << 4),
    DebugCulling    = (1 << 5),
    DebugShadow     = (1 << 6),
    DebugReduce     = (1 << 7),
    DebugLights     = (1 << 8),
    DebugDeferred   = (1 << 9),
    DebugBlur       = (1 << 10),
    DebugDecals     = (1 << 11),
    DebugSSAO       = (1 << 12),
    DebugTimings    = (1 << 13),
    DebugBVH        = (1 << 14)
};

class Renderer {
public:
    Renderer(Ren::Context &ctx, std::shared_ptr<Sys::ThreadPool> &threads);
    ~Renderer();

    uint32_t render_flags() const {
        return render_flags_;
    }

    void set_render_flags(uint32_t f) {
        render_flags_ = f;
    }

    RenderInfo render_info() const {
        return drawables_data_[0].render_info;
    }

    FrontendInfo frontend_info() const {
        return drawables_data_[0].frontend_info;
    }

    BackendInfo backend_info() const {
        return backend_info_;
    }

    void SwapDrawLists();
    void PrepareDrawList(int index, const SceneData &scene, const Ren::Camera &cam);
    void ExecuteDrawList(int index);

    void BlitPixels(const void *data, int w, int h, const Ren::eTexColorFormat format);
    void BlitPixelsTonemap(const void *data, int w, int h, const Ren::eTexColorFormat format);
    void BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf, int first_att, int att_count, float multiplier = 1.0f);
    void BlitTexture(float px, float py, float sx, float sy, uint32_t tex_id, int resx, int resy, bool is_ms = false);
private:
    Ren::Context &ctx_;
    std::shared_ptr<Sys::ThreadPool> threads_;
    SWcull_ctx cull_ctx_;
    Ren::ProgramRef skydome_prog_, fill_depth_prog_, shadow_prog_, blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_combine_ms_prog_,
        blit_red_prog_, blit_down_prog_, blit_down_ms_prog_, blit_gauss_prog_, blit_debug_prog_, blit_debug_ms_prog_, blit_ssr_ms_prog_,
        blit_ao_ms_prog_, blit_multiply_prog_, blit_multiply_ms_prog_, blit_debug_bvh_prog_, blit_debug_bvh_ms_prog_;
    Ren::Texture2DRef default_lightmap_, default_ao_;

    FrameBuf clean_buf_, refl_buf_, down_buf_, blur_buf1_, blur_buf2_, shadow_buf_, reduced_buf_, ssao_buf_;
    int w_ = 0, h_ = 0;

    Ren::TextureSplitter shadow_splitter_;

    static const uint32_t default_flags =
#if !defined(__ANDROID__)
        (EnableZFill | EnableCulling | EnableSSR | EnableSSAO /*| DebugBVH*/);
#else
        (EnableZFill | EnableCulling);
#endif
    uint32_t render_flags_ = default_flags;

    int frame_counter_ = 0;

    struct DrawablesData {
        Ren::Camera draw_cam, shadow_cams[4];
        std::vector<InstanceData> instances;
        std::vector<ShadowDrawBatch> shadow_batches;
        ShadowList shadow_lists[4] = {};
        std::vector<MainDrawBatch> main_batches;
        std::vector<LightSourceItem> light_sources;
        std::vector<DecalItem> decals;
        std::vector<CellData> cells;
        std::vector<ItemData> items;
        int items_count = 0;
        const Ren::TextureAtlas *decals_atlas = nullptr;
        Environment env;
        RenderInfo render_info;
        FrontendInfo frontend_info;
        uint32_t render_flags = default_flags;

        // for debugging only, backend does not require nodes for drawing
        std::vector<bvh_node_t> temp_nodes;
        uint32_t root_index;
    } drawables_data_[2];

    std::vector<const LightSource *> litem_to_lsource_;
    std::vector<const Decal *> ditem_to_decal_;
    std::vector<BBox> decals_boxes_;
    BackendInfo backend_info_;
    uint64_t backend_cpu_start_, backend_cpu_end_;
    int64_t backend_time_diff_;
    float reduced_average_ = 0.0f;
    Ren::Mat4f prev_view_from_world_;

#if defined(USE_GL_RENDER)
    uint32_t temp_tex_;
    Ren::eTexColorFormat temp_tex_format_;
    int temp_tex_w_ = 0, temp_tex_h_ = 0;

    uint32_t unif_shared_data_block_;
    uint32_t temp_vao_, shadow_pass_vao_, depth_pass_vao_, draw_pass_vao_, skydome_vao_;
    uint32_t temp_buf_vtx_offset_, temp_buf_ndx_offset_, skydome_vtx_offset_, skydome_ndx_offset_;
    uint32_t last_vertex_buffer_ = 0, last_index_buffer_ = 0;
    uint32_t instances_buf_, instances_tbo_;
    uint32_t lights_buf_, lights_tbo_, decals_buf_, decals_tbo_, cells_buf_, cells_tbo_, items_buf_, items_tbo_;
    uint32_t reduce_pbo_;

    uint32_t nodes_buf_ = 0, nodes_tbo_ = 0;

    enum { TimeDrawStart, TimeShadowMapStart, TimeDepthPassStart, TimeAOPassStart, TimeOpaqueStart,
           TimeReflStart, TimeBlurStart, TimeBlitStart, TimeDrawEnd, TimersCount };
    uint32_t queries_[2][TimersCount];

    void CheckInitVAOs();
#endif

    //temp
    std::vector<uint8_t> depth_pixels_[2], depth_tiles_[2];

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, DrawablesData &data);

    void InitRendererInternal();
    void DestroyRendererInternal();
    void DrawObjectsInternal(const DrawablesData &data);
    uint64_t GetGpuTimeBlockingUs();

    // Parallel Jobs
    static void GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                         const DecalItem *decals, int decals_count, const BBox *decals_boxes,
                                         const LightSource * const *litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count);
};