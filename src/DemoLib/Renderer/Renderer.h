#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Ren/Camera.h>
#include <Ren/RingBuffer.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}
#include <Sys/SpinLock.h>

#include "FrameBuf.h"
#include "../Scene/SceneData.h"

namespace Sys {
    class ThreadPool;
}

class TextureAtlas;

struct DrawableItem {
    const Ren::Mat4f    *clip_from_object, *world_from_object, *sh_clip_from_object[4];
    const Ren::Material *mat;
    const Ren::Mesh     *mesh;
    const Ren::TriGroup *tris;
    const Ren::Texture2D *lm_dir_tex;
    const Ren::Texture2D *lm_indir_tex;
    const Ren::Texture2D *lm_indir_sh_tex[4] = {};

    DrawableItem(const Ren::Mat4f *_clip_from_object, const Ren::Mat4f *_world_from_object, const Ren::Material *_mat,
                 const Ren::Mesh *_mesh, const Ren::TriGroup *_strip, const Ren::Texture2D *_lm_dir_tex, const Ren::Texture2D *_lm_indir_tex) : clip_from_object(_clip_from_object), world_from_object(_world_from_object), sh_clip_from_object{ nullptr },
        mat(_mat), mesh(_mesh), tris(_strip), lm_dir_tex(_lm_dir_tex), lm_indir_tex(_lm_indir_tex) { }

    bool operator<(const DrawableItem& rhs) const {
        return std::tie(mat, mesh, clip_from_object, world_from_object) < std::tie(rhs.mat, rhs.mesh, rhs.clip_from_object, rhs.world_from_object);
    }
};

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

#define REN_GRID_RES_X 16
#define REN_GRID_RES_Y 8
#define REN_GRID_RES_Z 24

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
}

enum eRenderFlags {
    EnableCulling   = (1 << 1),
    EnableSSR       = (1 << 2),
    EnableSSAO      = (1 << 3),
    EnableWireframe = (1 << 4),
    DebugCulling    = (1 << 5),
    DebugShadow     = (1 << 6),
    DebugReduce     = (1 << 7),
    DebugLights     = (1 << 8),
    DebugDeferred   = (1 << 9),
    DebugBlur       = (1 << 10),
    DebugDecals     = (1 << 11),
    DebugSSAO       = (1 << 12),
    DebugTimings    = (1 << 13)
};

class Renderer {
public:
    Renderer(Ren::Context &ctx, std::shared_ptr<Sys::ThreadPool> &threads);
    ~Renderer();

    uint32_t render_flags() const {
        return render_flags_[0];
    }

    void set_render_flags(uint32_t f) {
        render_flags_[0] = f;
    }

    RenderInfo render_info() const {
        return render_infos_[0];
    }

    FrontendInfo frontend_info() const {
        return frontend_infos_[0];
    }

    BackendInfo backend_info() const {
        return backend_info_;
    }

    void DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_index,
                     const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env,
                     const TextureAtlas &decals_atlas);
    void WaitForBackgroundThreadIteration();

    void BlitPixels(const void *data, int w, int h, const Ren::eTexColorFormat format);
    void BlitPixelsTonemap(const void *data, int w, int h, const Ren::eTexColorFormat format);
    void BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf, int first_att, int att_count, float multiplier = 1.0f);
    void BlitTexture(float px, float py, float sx, float sy, uint32_t tex_id, int resx, int resy, bool is_ms = false);
private:
    Ren::Context &ctx_;
    std::shared_ptr<Sys::ThreadPool> threads_;
    SWcull_ctx cull_ctx_;
    Ren::ProgramRef skydome_prog_, fill_depth_prog_, shadow_prog_, blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_combine_ms_prog_,
        blit_red_prog_, blit_down_prog_, blit_down_ms_prog_, blit_gauss_prog_, blit_debug_prog_, blit_debug_ms_prog_, blit_ssr_ms_prog_, blit_ao_ms_prog_;
    Ren::Texture2DRef default_lightmap_;

    FrameBuf clean_buf_, down_buf_, blur_buf1_, blur_buf2_, shadow_buf_, reduced_buf_, ssao_buf_;
    int w_ = 0, h_ = 0;

    const uint32_t default_flags = (EnableCulling | EnableSSR | EnableSSAO);
    uint32_t render_flags_[2] = { default_flags, default_flags };

    int frame_counter_ = 0;

    const bvh_node_t *nodes_ = nullptr;
    size_t root_node_ = 0;
    const SceneObject *objects_ = nullptr;
    const uint32_t *obj_indices_ = nullptr;
    size_t object_count_ = 0;
    std::vector<Ren::Mat4f> transforms_[2];
    std::vector<DrawableItem> draw_lists_[2], shadow_list_[2][4];
    std::vector<LightSourceItem> light_sources_[2];
    std::vector<DecalItem> decals_[2];
    std::vector<CellData> cells_[2];
    std::vector<ItemData> items_[2];
    const TextureAtlas *decals_atlas_[2] = {};
    int items_count_[2] = {};
    std::vector<uint32_t> object_to_drawable_;
    std::vector<const LightSource *> litem_to_lsource_;
    std::vector<const Decal *> ditem_to_decal_;
    std::vector<BBox> decals_boxes_;
    Ren::Camera draw_cam_, shadow_cam_[2][4];
    Environment env_;
    RenderInfo render_infos_[2];
    FrontendInfo frontend_infos_[2];
    BackendInfo backend_info_;
    uint64_t backend_cpu_start_, backend_cpu_end_;
    int64_t backend_time_diff_;
    float reduced_average_ = 0.0f;
    Ren::Mat4f prev_view_from_world_;

#if defined(USE_GL_RENDER)
    uint32_t temp_tex_;
    Ren::eTexColorFormat temp_tex_format_;
    int temp_tex_w_ = 0, temp_tex_h_ = 0;

    uint32_t unif_matrices_block_;
    uint32_t temp_vao_, shadow_pass_vao_, depth_pass_vao_, draw_pass_vao_, skydome_vao_;
    uint32_t temp_buf_vtx_offset_, temp_buf_ndx_offset_, skydome_vtx_offset_, skydome_ndx_offset_;
    uint32_t last_vertex_buffer_ = 0, last_index_buffer_ = 0;
    uint32_t lights_buf_, lights_tbo_, decals_buf_, decals_tbo_, cells_buf_, cells_tbo_, items_buf_, items_tbo_;
    uint32_t reduce_pbo_;

    enum { TimeDrawStart, TimeShadowMapStart, TimeDepthPassStart, TimeAOPassStart, TimeOpaqueStart,
           TimeReflStart, TimeBlurStart, TimeBlitStart, TimeDrawEnd, TimersCount };
    uint32_t queries_[2][TimersCount];

    void CheckInitVAOs();
#endif

    //temp
    std::vector<uint8_t> depth_pixels_[2], depth_tiles_[2];

    void SwapDrawLists(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_node,
                       const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env,
                       const TextureAtlas *decals_atlas);

    void GatherDrawables(const Ren::Camera &draw_cam, uint32_t render_flags, std::vector<Ren::Mat4f> &tr_list, std::vector<DrawableItem> &dr_list, std::vector<LightSourceItem> &ls_list,
                         std::vector<DecalItem> &de_list, std::vector<CellData> &cells, std::vector<ItemData> &items, std::vector<DrawableItem> sh_dr_list[4], FrontendInfo &info);

    void InitRendererInternal();
    void DestroyRendererInternal();
    void DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count, const LightSourceItem *lights, size_t lights_count,
                             const DecalItem *decals, size_t decals_count,
                             const CellData *cells, const ItemData *items, size_t item_count, const Ren::Mat4f shadow_transforms[4],
                             const DrawableItem *shadow_drawables[4], size_t shadow_drawable_count[4], const Environment &env,
                             const TextureAtlas *decals_atlas);
    uint64_t GetGpuTimeBlockingUs();

    std::thread background_thread_;
    std::mutex mtx_;
    std::condition_variable thr_notify_, thr_done_;
    bool shutdown_ = false, notified_ = false;
    Sys::SpinlockMutex job_mtx_;

    void BackgroundProc();

    // Parallel Jobs
    static void GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                         const DecalItem *decals, int decals_count, const BBox *decals_boxes,
                                         const LightSource * const *litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count);
};