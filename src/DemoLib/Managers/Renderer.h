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
    const Ren::Mat4f    *xform;
    const Ren::Material *mat;
    const Ren::Mesh     *mesh;
    const Ren::TriStrip *strip;

    bool operator<(const DrawableItem& rhs) const {
        return std::tie(mat, mesh, xform) < std::tie(rhs.mat, rhs.mesh, rhs.xform);
    }
};

class Renderer {
public:
    Renderer(Ren::Context &ctx);
    ~Renderer();

    TimingInfo timings() const { return timings_; }
    TimingInfo back_timings() const { return back_timings_[0]; }

    void DrawObjects(const Ren::Camera &cam, const SceneObject *objects, size_t object_count);
    void WaitForCompletion();
private:
    Ren::Context &ctx_;
    SWcull_ctx cull_ctx_;
    Ren::ProgramRef fill_depth_prog_;

    const SceneObject *objects_ = nullptr;
    size_t object_count_ = 0;
    std::vector<Ren::Mat4f> transforms_[2];
    std::vector<DrawableItem> draw_lists_[2];
    Ren::Camera draw_cam_;
    TimingInfo timings_, back_timings_[2];

    //temp
    std::vector<uint8_t> depth_pixels_[2], depth_tiles_[2];

    void SwapDrawLists(const Ren::Camera &cam, const SceneObject *objects, size_t object_count);

    void InitShadersInternal();
    void DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count);

    std::thread background_thread_;
    std::mutex mtx_;
    std::condition_variable thr_notify_;
    bool shutdown_ = false, notified_ = false;
    Sys::SpinlockMutex job_mtx_;

    void BackgroundProc();
};