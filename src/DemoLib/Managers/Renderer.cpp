#include "Renderer.h"

#include <Sys/Log.h>

Renderer::Renderer(Ren::Context &ctx) : ctx_(ctx), draw_cam_({}, {}, {}) {
    InitShadersInternal();
    background_thread_ = std::thread(std::bind(&Renderer::BackgroundProc, this));
}

Renderer::~Renderer() {
    if (background_thread_.joinable()) {
        shutdown_ = notified_ = true;
        thr_notify_.notify_all();
        background_thread_.join();
    }
}

void Renderer::DrawObjects(const Ren::Camera &cam, const SceneObject *objects, size_t object_count) {
    SwapDrawLists(cam, objects, object_count);
    auto t1 = std::chrono::high_resolution_clock::now();
    if (!draw_lists_[0].empty()) {
        DrawObjectsInternal(cam, &draw_lists_[0][0], draw_lists_[0].size());
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    timings_ = { t1, t2 };
}

void Renderer::WaitForCompletion() {
    SwapDrawLists(draw_cam_, nullptr, 0);
}

void Renderer::BackgroundProc() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (!shutdown_) {
        while (!notified_) {
            thr_notify_.wait(lock);
        }

        {
            std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
            auto t1 = std::chrono::high_resolution_clock::now();

            auto &dr_list = draw_lists_[1];
            dr_list.clear();

            for (size_t i = 0; i < object_count_; i++) {
                const auto &obj = objects_[i];
                if (obj.flags & (HasDrawable | HasTransform)) {
                    const auto *dr = obj.dr.get();
                    const auto *tr = obj.tr.get();

                    const float bbox[8][3] = { tr->bbox_min_ws[0], tr->bbox_min_ws[1], tr->bbox_min_ws[2],
                                               tr->bbox_max_ws[0], tr->bbox_min_ws[1], tr->bbox_min_ws[2],
                                               tr->bbox_min_ws[0], tr->bbox_min_ws[1], tr->bbox_max_ws[2],
                                               tr->bbox_max_ws[0], tr->bbox_min_ws[1], tr->bbox_max_ws[2],

                                               tr->bbox_min_ws[0], tr->bbox_max_ws[1], tr->bbox_min_ws[2],
                                               tr->bbox_max_ws[0], tr->bbox_max_ws[1], tr->bbox_min_ws[2],
                                               tr->bbox_min_ws[0], tr->bbox_max_ws[1], tr->bbox_max_ws[2],
                                               tr->bbox_max_ws[0], tr->bbox_max_ws[1], tr->bbox_max_ws[2] };

                    if (!draw_cam_.IsInFrustum(bbox)) continue;

                    const auto *mesh = dr->mesh.get();
                    const Ren::TriStrip *s = &mesh->strip(0);
                    while (s->offset != -1) {
                        const Ren::Material *mat = s->mat.get();
                        dr_list.push_back({ &obj.tr->mat, mat, mesh, s });
                        ++s;
                    }
                }
            }

            std::sort(std::begin(dr_list), std::end(dr_list));

            auto t2 = std::chrono::high_resolution_clock::now();
            back_timings_[1] = { t1, t2 };
        }

        notified_ = false;
    }
}

void Renderer::SwapDrawLists(const Ren::Camera &cam, const SceneObject *objects, size_t object_count) {
    bool should_notify = false;
    {
        std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
        std::swap(draw_lists_[0], draw_lists_[1]);
        objects_ = objects;
        object_count_ = object_count;
        back_timings_[0] = back_timings_[1];
        draw_cam_ = cam;
        should_notify = object_count > 0;
    }
    if (should_notify) {
        notified_ = true;
        thr_notify_.notify_all();
    }
}