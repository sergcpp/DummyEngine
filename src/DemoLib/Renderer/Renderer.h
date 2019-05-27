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
    float col[3]; int shadowreg_index;
    float dir[3], spot;
};
static_assert(sizeof(LightSourceItem) == 48, "!");

struct DecalItem {
    float mat[3][4];
    float diff[4], norm[4], spec[4];
};
static_assert(sizeof(DecalItem) == 24 * sizeof(float), "!");

struct ProbeItem {
    float position[3], radius;
    float _unused[3], layer;
    float sh_coeffs[3][4];
};
static_assert(sizeof(ProbeItem) == 20 * sizeof(float), "!");

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
    union {
        struct {
            uint32_t _pad1 : 3;
            uint32_t indices_offset  : 28;
            uint32_t alpha_test_bit  : 1;
        };
        uint32_t sort_key = 0;
    };

    uint32_t indices_count, mat_id;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;
};
static_assert(sizeof(ShadowDrawBatch) == sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int) * REN_MAX_BATCH_SIZE + sizeof(int), "!");

struct MainDrawBatch {
    union {
        struct {
            uint32_t _pad1          : 4;
            uint32_t indices_offset : 28;
            uint32_t cam_dist       : 8;
            uint32_t mat_id         : 14;
            uint32_t alpha_test_bit : 1;
            uint32_t prog_id        : 8;
            uint32_t alpha_blend_bit: 1;
        };
        uint64_t sort_key = 0;
    };

    uint32_t indices_count;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;
};
static_assert(sizeof(MainDrawBatch) == sizeof(uint64_t) + sizeof(uint32_t) + sizeof(int) * REN_MAX_BATCH_SIZE + sizeof(int), "!");

struct ShadowList {
    int shadow_map_pos[2], shadow_map_size[2];
    uint32_t shadow_batch_start, shadow_batch_count;
    float cam_near, cam_far; // for debugging
};

struct ShadowMapRegion {
    Ren::Vec4f transform;
    Ren::Mat4f clip_from_world;
};
static_assert(sizeof(ShadowMapRegion) == 80, "!");

#define MAX_STACK_SIZE 64

struct SortSpan32 {
    uint32_t key;
    uint32_t base;
    uint32_t count;
};

struct SortSpan64 {
    uint64_t key;
    uint32_t base;
    uint32_t count;
};

enum eRenderFlags {
    EnableZFill     = (1 << 0),
    EnableCulling   = (1 << 1),
    EnableSSR       = (1 << 2),
    EnableSSAO      = (1 << 3),
    EnableLightmap  = (1 << 4),
    EnableLights    = (1 << 5),
    EnableDecals    = (1 << 6),
    EnableProbes    = (1 << 7),
    EnableShadows   = (1 << 8),
    EnableTonemap   = (1 << 9),
    EnableBloom     = (1 << 10),
    EnableFxaa      = (1 << 11),
    EnableTimers    = (1 << 12),
    DebugWireframe  = (1 << 13),
    DebugCulling    = (1 << 14),
    DebugShadow     = (1 << 15),
    DebugReduce     = (1 << 16),
    DebugLights     = (1 << 17),
    DebugDeferred   = (1 << 18),
    DebugBlur       = (1 << 19),
    DebugDecals     = (1 << 20),
    DebugSSAO       = (1 << 21),
    DebugTimings    = (1 << 22),
    DebugBVH        = (1 << 23),
    DebugProbes     = (1 << 24)
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

    BackendInfo backend_info() const {
        return backend_info_;
    }

    struct ShadReg {
        const LightSource *ls;
        int pos[2], size[2];
        float cam_near, cam_far; // for debugging
        uint32_t last_update, last_visible;
    };

    static const int CELLS_COUNT = REN_GRID_RES_X * REN_GRID_RES_Y * REN_GRID_RES_Z;

    static const int MAX_LIGHTS_PER_CELL = 255;
    static const int MAX_DECALS_PER_CELL = 255;
    static const int MAX_PROBES_PER_CELL = 8;
    static const int MAX_ITEMS_PER_CELL = 255;

    static const int MAX_LIGHTS_TOTAL = 4096;
    static const int MAX_DECALS_TOTAL = 4096;
    static const int MAX_PROBES_TOTAL = 256;
    static const int MAX_ITEMS_TOTAL = (1 << 16);

    struct DrawList {
        uint32_t        render_flags = default_flags;
        Ren::Camera     draw_cam;
        Environment     env;
        FrontendInfo    frontend_info;
        std::vector<InstanceData>       instances;
        std::vector<ShadowDrawBatch>    shadow_batches;
        std::vector<uint32_t>           shadow_batch_indices;
        std::vector<ShadowList>         shadow_lists;
        std::vector<ShadowMapRegion>    shadow_regions;
        std::vector<MainDrawBatch>      main_batches;
        std::vector<uint32_t>           main_batch_indices;
        std::vector<LightSourceItem>    light_sources;
        std::vector<DecalItem>          decals;
        std::vector<ProbeItem>          probes;
        std::vector<CellData>           cells;
        std::vector<ItemData>           items;
        int items_count = 0;
        const Ren::TextureAtlas *decals_atlas = nullptr;
        const ProbeStorage *probe_storage = nullptr;

        // for debugging only, backend does not require nodes for drawing
        std::vector<bvh_node_t> temp_nodes;
        uint32_t root_index;
        std::vector<ShadReg> cached_shadow_regions;
    };

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const FrameBuf *target = nullptr);

    void BlitPixels(const void *data, int w, int h, const Ren::eTexColorFormat format);
    void BlitPixelsTonemap(const void *data, int w, int h, const Ren::eTexColorFormat format);
    void BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf, int first_att, int att_count, float multiplier = 1.0f);
    void BlitTexture(float px, float py, float sx, float sy, uint32_t tex_id, int resx, int resy, bool is_ms = false);

    void BlitToLightProbeFace(const FrameBuf &src_buf, const ProbeStorage &dst_store, int probe_index, int face);
    bool BlitProjectSH(const ProbeStorage &store, int probe_index, int iteration, LightProbe &probe);
private:
    Ren::Context &ctx_;
    std::shared_ptr<Sys::ThreadPool> threads_;
    SWcull_ctx cull_ctx_;
    Ren::ProgramRef skydome_prog_, fill_depth_prog_, shadow_prog_, blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_combine_ms_prog_,
        blit_red_prog_, blit_down_prog_, blit_down_ms_prog_, blit_gauss_prog_, blit_gauss_sep_prog_, blit_debug_prog_, blit_debug_ms_prog_, blit_ssr_prog_, blit_ssr_ms_prog_,
        blit_ao_prog_, blit_ao_ms_prog_, blit_multiply_prog_, blit_multiply_ms_prog_, blit_debug_bvh_prog_, blit_debug_bvh_ms_prog_, blit_depth_prog_,
        blit_rgbm_prog_, blit_mipmap_prog_, blit_project_sh_prog_, blit_fxaa_prog_, probe_prog_, skinning_prog_;
    Ren::Texture2DRef dummy_black_, dummy_white_, rand2d_8x8_;

    FrameBuf clean_buf_, refl_buf_, down_buf_, blur_buf1_, blur_buf2_, shadow_buf_, reduced_buf_, ssao_buf_, probe_sample_buf_, combined_buf_;
    int scr_w_ = 0, scr_h_ = 0, act_w_ = 0, act_h_ = 0;

    Ren::TextureSplitter shadow_splitter_;

    static const uint32_t default_flags =
#if !defined(__ANDROID__)
        (EnableZFill | EnableCulling | EnableSSR | EnableSSAO | EnableLightmap | EnableLights | EnableDecals | EnableShadows | EnableTonemap | EnableBloom | EnableFxaa | EnableTimers);
#else
        (EnableZFill | EnableCulling | EnableSSR | EnableLightmap | EnableLights | EnableDecals | EnableShadows | EnableTonemap | EnableTimers);
#endif
    uint32_t render_flags_ = default_flags;

    int frame_counter_ = 0;

    std::vector<const LightSource *> litem_to_lsource_;
    std::vector<const Decal *> ditem_to_decal_;
    std::vector<uint32_t> obj_to_instance_;
    std::vector<BBox> decals_boxes_;
    BackendInfo backend_info_;
    uint64_t backend_cpu_start_, backend_cpu_end_;
    int64_t backend_time_diff_;
    float reduced_average_ = 0.0f;
    Ren::Mat4f down_buf_view_from_world_;

    std::vector<Ren::Frustum> temp_sub_frustums_;
    std::vector<SortSpan32> temp_sort_spans_32_[2];
    std::vector<SortSpan64> temp_sort_spans_64_[2];

#if defined(USE_GL_RENDER)
    uint32_t temp_tex_;
    Ren::eTexColorFormat temp_tex_format_;
    int temp_tex_w_ = 0, temp_tex_h_ = 0;

    uint32_t temp_framebuf_, skydome_framebuf_ = 0, depth_fill_framebuf_ = 0, refl_comb_framebuf_ = 0;

    uint32_t unif_shared_data_block_, unif_batch_data_block_;
    uint32_t temp_vao_, shadow_pass_vao_, depth_pass_vao_, draw_pass_vao_, skydome_vao_, sphere_vao_;
    uint32_t temp_buf_vtx_offset_, temp_buf_ndx_offset_, skydome_vtx_offset_, skydome_ndx_offset_, sphere_vtx_offset_, sphere_ndx_offset_;
    uint32_t last_vertex_buffer_ = 0, last_index_buffer_ = 0;
    uint32_t instances_buf_, instances_tbo_;
    uint32_t lights_buf_, lights_tbo_, decals_buf_, decals_tbo_, cells_buf_, cells_tbo_, items_buf_, items_tbo_;
    uint32_t reduce_pbo_, probe_sample_pbo_;

    uint32_t nodes_buf_ = 0, nodes_tbo_ = 0;

    enum { TimeDrawStart, TimeShadowMapStart, TimeDepthOpaqueStart, TimeAOPassStart, TimeOpaqueStart, TimeTranspStart,
           TimeReflStart, TimeBlurStart, TimeBlitStart, TimeDrawEnd, TimersCount };
    uint32_t queries_[2][TimersCount];

    void CheckInitVAOs();
#endif

    std::vector<ShadReg> allocated_shadow_regions_;

    //temp
    std::vector<uint8_t> depth_pixels_[2], depth_tiles_[2];
    float debug_roughness_ = 0.0f;

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, DrawList &list);

    void InitRendererInternal();
    bool InitFramebuffersInternal();
    void DestroyRendererInternal();
    void DrawObjectsInternal(const DrawList &list, const FrameBuf *target);
    uint64_t GetGpuTimeBlockingUs();

    // Parallel Jobs
    static void GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                         const DecalItem *decals, int decals_count, const BBox *decals_boxes, const ProbeItem *probes, int probes_count,
                                         const LightSource * const *litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count);
};