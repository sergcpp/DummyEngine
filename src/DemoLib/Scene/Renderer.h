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
#include "SceneData.h"

namespace Sys {
    class ThreadPool;
}

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

struct CellData {
    uint32_t item_offset;
    uint32_t light_count;
};
static_assert(sizeof(CellData) == 8, "!");

struct ItemData {
    uint16_t light_index;
};
static_assert(sizeof(ItemData) == 2, "!");

namespace RendererInternal {
    const int GRID_RES_X = 16;
    const int GRID_RES_Y = 8;
    const int GRID_RES_Z = 24;

    const int CELLS_COUNT = GRID_RES_X * GRID_RES_Y * GRID_RES_Z;

    const int MAX_LIGHTS_PER_CELL = 256;
    const int MAX_LIGHTS_PER_CELL_EXC = 16;

    const int MAX_LIGHTS_COUNT = CELLS_COUNT * MAX_LIGHTS_PER_CELL_EXC;
}

class Renderer {
public:
    Renderer(Ren::Context &ctx, std::shared_ptr<Sys::ThreadPool> &threads);
    ~Renderer();

    void toggle_wireframe() {
        wireframe_mode_ = !wireframe_mode_;
    }
    void toggle_culling() {
        culling_enabled_ = !culling_enabled_;
    }
    void toggle_debug_cull() {
        debug_cull_ = !debug_cull_;
    }
    void toggle_debug_shadow() {
        debug_shadow_ = !debug_shadow_;
    }
    void toggle_debug_reduce() {
        debug_reduce_ = !debug_reduce_;
    }
    void toggle_debug_lights() {
        debug_lights_ = !debug_lights_;
    }

    TimingInfo timings() const {
        return timings_;
    }

    TimingInfo back_timings() const {
        return back_timings_[0];
    }

    RenderInfo render_info() const {
        return render_infos_[0];
    }

    void DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_index,
                     const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env);
    void WaitForBackgroundThreadIteration();

    void BlitPixels(const void *data, int w, int h, const Ren::eTexColorFormat format);
private:
    Ren::Context &ctx_;
    std::shared_ptr<Sys::ThreadPool> threads_;
    SWcull_ctx cull_ctx_;
    Ren::ProgramRef fill_depth_prog_, shadow_prog_, blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_combine_ms_prog_,
        blit_red_prog_, blit_down_prog_, blit_down_ms_prog_, blit_gauss_prog_;
    Ren::Texture2DRef default_lightmap_;

    FrameBuf clean_buf_, blur_buf1_, blur_buf2_, shadow_buf_, reduced_buf_;
    int w_ = 0, h_ = 0;

    bool wireframe_mode_ = false, debug_cull_ = false, debug_shadow_ = false, debug_reduce_ = false, debug_lights_ = false;
    bool culling_enabled_ = true;

    const bvh_node_t *nodes_ = nullptr;
    size_t root_node_ = 0;
    const SceneObject *objects_ = nullptr;
    const uint32_t *obj_indices_ = nullptr;
    size_t object_count_ = 0;
    std::vector<Ren::Mat4f> transforms_[2];
    std::vector<DrawableItem> draw_lists_[2], shadow_list_[2][4];
    std::vector<LightSourceItem> light_sources_[2];
    std::vector<CellData> cells_[2];
    std::vector<ItemData> items_[2];
    int items_count_[2] = {};
    std::vector<uint32_t> object_to_drawable_;
    std::vector<const LightSource *> litem_to_lsource_;
    Ren::Camera draw_cam_, shadow_cam_[2][4];
    Environment env_;
    TimingInfo timings_, back_timings_[2];
    RenderInfo render_infos_[2];
    std::vector<float> reduced_pixels_;
    float reduced_average_ = 0.0f;

#if defined(USE_GL_RENDER)
    uint32_t temp_tex_;
    Ren::eTexColorFormat temp_tex_format_;
    int temp_tex_w_ = 0, temp_tex_h_ = 0;

    uint32_t unif_matrices_block_;
    uint32_t temp_vao_, shadow_pass_vao_, depth_pass_vao_, draw_pass_vao_;
    uint32_t temp_buf_vtx_offset_, temp_buf_ndx_offset_;
    uint32_t last_vertex_buffer_ = 0, last_index_buffer_ = 0;
    uint32_t lights_ssbo_, lights_tbo_, cells_ssbo_, cells_tbo_, items_ssbo_, items_tbo_;

    void CheckInitVAOs();
#endif

    //temp
    std::vector<uint8_t> depth_pixels_[2], depth_tiles_[2];

    void SwapDrawLists(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_node,
                       const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env);

    void InitRendererInternal();
    void DestroyRendererInternal();
    void DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count, const LightSourceItem *lights, size_t lights_count,
                             const CellData *cells, const ItemData *items, size_t item_count, const Ren::Mat4f shadow_transforms[4],
                             const DrawableItem *shadow_drawables[4], size_t shadow_drawable_count[4], const Environment &env);

    std::thread background_thread_;
    std::mutex mtx_;
    std::condition_variable thr_notify_, thr_done_;
    bool shutdown_ = false, notified_ = false;
    Sys::SpinlockMutex job_mtx_;

    void BackgroundProc();

    // Parallel Jobs
    static void GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                         const LightSource * const *litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count);
};