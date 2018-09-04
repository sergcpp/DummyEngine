#include "Renderer.h"

#include <Sys/Log.h>

Renderer::Renderer(Ren::Context &ctx) : ctx_(ctx), draw_cam_({}, {}, {}) {
    {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;
        SWfloat z = FAR_CLIP / (FAR_CLIP - NEAR_CLIP) + (NEAR_CLIP - (2.0f * NEAR_CLIP)) / (0.15f * (FAR_CLIP - NEAR_CLIP));
        swCullCtxInit(&cull_ctx_, 256, 128, z);
    }
    InitShadersInternal();
    background_thread_ = std::thread(std::bind(&Renderer::BackgroundProc, this));
}

Renderer::~Renderer() {
    swCullCtxDestroy(&cull_ctx_);
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

            transforms_.clear();
            transforms_.reserve(object_count_);

            auto &dr_list = draw_lists_[1];
            dr_list.clear();
            dr_list.reserve(object_count_);

            Ren::Mat4f view_from_world = draw_cam_.view_matrix(),
                       proj_from_view = draw_cam_.projection_matrix();

            swCullCtxClear(&cull_ctx_);

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

                    const Ren::Mat4f &world_from_object = tr->mat;

                    Ren::Mat4f view_from_object = view_from_world * world_from_object,
                               proj_from_object = proj_from_view * view_from_object;

                    transforms_.push_back(proj_from_object);

                    const auto *mesh = dr->mesh.get();

                    if (obj.flags & IsOccluder) {
                        SWcull_surf surf[16];
                        int surf_count = 0;

                        const Ren::TriStrip *s = &mesh->strip(0);
                        while (s->offset != -1) {
                            surf[surf_count].type = SW_OCCLUDER;
                            surf[surf_count].prim_type = SW_TRIANGLES;
                            surf[surf_count].index_type = SW_UNSIGNED_INT;
                            surf[surf_count].attribs = mesh->attribs();
                            surf[surf_count].indices = ((const uint32_t *)mesh->indices() + s->offset);
                            surf[surf_count].stride = 13 * sizeof(float);
                            surf[surf_count].count = (SWuint)s->num_indices;
                            surf[surf_count].xform = Ren::ValuePtr(transforms_.back());
                            surf[surf_count].dont_skip = nullptr;
                            surf_count++;

                            const Ren::Material *mat = s->mat.get();
                            dr_list.push_back({ &transforms_.back(), mat, mesh, s });
                            ++s;
                        }

                        swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                    } else {
                        const Ren::TriStrip *s = &mesh->strip(0);
                        while (s->offset != -1) {
                            const Ren::Material *mat = s->mat.get();
                            dr_list.push_back({ &transforms_.back(), mat, mesh, s });
                            ++s;
                        }
                    }
                }
            }

            std::sort(std::begin(dr_list), std::end(dr_list));

            {
                const float NEAR_CLIP = 0.5f;
                const float FAR_CLIP = 10000.0f;

                int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
                depth_pixels_.resize(w * h * 4);
                for (int x = 0; x < w; x++) {
                    for (int y = 0; y < h; y++) {
                        float z = cull_ctx_.zbuf.depth[(h - y - 1) * w + x];
                        z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                        depth_pixels_[4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                        depth_pixels_[4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                        depth_pixels_[4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                        depth_pixels_[4 * (y * w + x) + 3] = 255;
                    }
                }
            }

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