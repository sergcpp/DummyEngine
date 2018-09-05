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

#include "SceneData.h"

struct DrawableItem {
    bool invisible = false;
    const Ren::Mat4f    *xform;
    const Ren::Material *mat;
    const Ren::Mesh     *mesh;
    const Ren::TriStrip *strip;

    DrawableItem(const Ren::Mat4f *_xform, const Ren::Material *_mat,
        const Ren::Mesh *_mesh, const Ren::TriStrip *_strip) : xform(_xform), mat(_mat), mesh(_mesh), strip(_strip) { }

    bool operator<(const DrawableItem& rhs) const {
        return std::tie(invisible, mat, mesh, xform) < std::tie(rhs.invisible, rhs.mat, rhs.mesh, rhs.xform);
    }
};

struct OccludeeItem {
    float bbox_points[8][3];
    size_t dr_start, dr_count;
};

class Renderer {
public:
    Renderer(Ren::Context &ctx);
    ~Renderer();

    void toggle_wireframe() { wireframe_mode_ = !wireframe_mode_; }
    void toggle_debug_cull() { debug_cull_ = !debug_cull_; }
    void toggle_culling() { culling_enabled_ = !culling_enabled_; }

    TimingInfo timings() const { return timings_; }
    TimingInfo back_timings() const { return back_timings_[0]; }

    void DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_index,
                     const SceneObject *objects, size_t object_count);
    void WaitForBackgroundThreadIteration();
private:
    Ren::Context &ctx_;
    SWcull_ctx cull_ctx_;
    Ren::ProgramRef fill_depth_prog_;

    bool wireframe_mode_ = false, debug_cull_ = false;
    bool culling_enabled_ = true;

    const bvh_node_t *nodes_ = nullptr;
    size_t root_node_ = 0;
    const SceneObject *objects_ = nullptr;
    size_t object_count_ = 0;
    std::vector<Ren::Mat4f> transforms_[2];
    std::vector<DrawableItem> draw_lists_[2];
    std::vector<OccludeeItem> occludees_;
    Ren::Camera draw_cam_;
    TimingInfo timings_, back_timings_[2];

    //temp
    std::vector<uint8_t> depth_pixels_[2], depth_tiles_[2];

    void SwapDrawLists(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_node,
                       const SceneObject *objects, size_t object_count);

    void InitShadersInternal();
    void DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count);

    std::thread background_thread_;
    std::mutex mtx_;
    std::condition_variable thr_notify_;
    bool shutdown_ = false, notified_ = false;
    Sys::SpinlockMutex job_mtx_;

    void BackgroundProc();
};