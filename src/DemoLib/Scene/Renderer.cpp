#include "Renderer.h"

#include <Sys/Log.h>

namespace RendererInternal {
    bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]) {
        return p[0] > bbox_min[0] && p[0] < bbox_max[0] &&
               p[1] > bbox_min[1] && p[1] < bbox_max[1] &&
               p[2] > bbox_min[2] && p[2] < bbox_max[2];
    }

    static const uint8_t bbox_indices[] = { 0, 1, 2,    2, 1, 3,
                                            0, 4, 5,    0, 5, 1,
                                            0, 2, 4,    4, 2, 6,
                                            2, 3, 6,    6, 3, 7,
                                            3, 1, 5,    3, 5, 7,
                                            4, 6, 5,    5, 6, 7 };

    const int MAX_STACK_SIZE = 64;

    const int SHADOWMAP_RES = 512;
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

Renderer::Renderer(Ren::Context &ctx) : ctx_(ctx), draw_cam_({}, {}, {}), shadow_cam_{ { {}, {}, {} }, { {}, {}, {} } } {
    using namespace RendererInternal;

    {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;
        SWfloat z = FAR_CLIP / (FAR_CLIP - NEAR_CLIP) + (NEAR_CLIP - (2.0f * NEAR_CLIP)) / (0.15f * (FAR_CLIP - NEAR_CLIP));
        swCullCtxInit(&cull_ctx_, 256, 128, z);
    }
    InitShadersInternal();

    try {
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::RawR32F, Ren::NoFilter, Ren::ClampToEdge, true);
    } catch (std::runtime_error &) {
        LOGI("Cannot create floating-point shadow buffer! Fallback to unsigned byte.");
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::RawRGB888, Ren::NoFilter, Ren::ClampToEdge, true);
    }

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

void Renderer::DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_index,
                           const SceneObject *objects, size_t object_count) {
    SwapDrawLists(cam, nodes, root_index, objects, object_count);
    auto t1 = std::chrono::high_resolution_clock::now();
    {
        size_t drawables_count = draw_lists_[0].size();
        const auto *drawables = (drawables_count == 0) ? nullptr : &draw_lists_[0][0];

        size_t shadow_drawables_count = shadow_list_[0].size();
        const auto *shadow_drawables = (shadow_drawables_count == 0) ? nullptr : &shadow_list_[0][0];

        DrawObjectsInternal(drawables, drawables_count, shadow_drawables, shadow_drawables_count);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    timings_ = { t1, t2 };
}

void Renderer::WaitForBackgroundThreadIteration() {
    SwapDrawLists(draw_cam_, nullptr, 0, nullptr, 0);
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
            auto t1 = std::chrono::high_resolution_clock::now();

            auto &tr_list = transforms_[1];
            tr_list.clear();
            tr_list.reserve(object_count_ * 2);

            auto &dr_list = draw_lists_[1];
            dr_list.clear();
            dr_list.reserve(object_count_ * 16);

            auto &sh_dr_list = shadow_list_[1];
            sh_dr_list.clear();
            sh_dr_list.reserve(object_count_ * 16);

            occludees_.clear();

            Ren::Mat4f view_from_world = draw_cam_.view_matrix(),
                       proj_from_view = draw_cam_.projection_matrix();

            swCullCtxClear(&cull_ctx_);

            Ren::Mat4f view_from_identity = view_from_world * Ren::Mat4f{ 1.0f },
                       proj_from_identity = proj_from_view * view_from_identity;

            uint32_t stack[MAX_STACK_SIZE];
            uint32_t stack_size = 0;

            stack[stack_size++] = (uint32_t)root_node_;

            while (stack_size && culling_enabled_) {
                uint32_t cur = stack[--stack_size];
                const auto *n = &nodes_[cur];

                if (!draw_cam_.IsInFrustum(n->bbox[0], n->bbox[1])) continue;

                if (!n->prim_count) {
                    stack[stack_size++] = n->left_child;
                    stack[stack_size++] = n->right_child;
                } else {
                    for (uint32_t i = n->prim_index; i < n->prim_index + n->prim_count; i++) {
                        const auto &obj = objects_[i];

                        uint32_t occluder_flags = HasMesh | HasTransform | HasOccluder;
                        if ((obj.flags & occluder_flags) == occluder_flags) {
                            const auto *tr = obj.tr.get();

                            if (!draw_cam_.IsInFrustum(tr->bbox_min_ws, tr->bbox_max_ws)) continue;

                            const Ren::Mat4f &world_from_object = tr->mat;

                            Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                       proj_from_object = proj_from_view * view_from_object;

                            const auto *mesh = obj.mesh.get();

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
                                surf[surf_count].xform = Ren::ValuePtr(proj_from_object);
                                surf[surf_count].dont_skip = nullptr;
                                surf_count++;

                                ++s;
                            }

                            swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                        }
                    }
                }
            }

            stack_size = 0;
            stack[stack_size++] = (uint32_t)root_node_;

            while (stack_size) {
                uint32_t cur = stack[--stack_size];
                const auto *n = &nodes_[cur];

                const float bbox_points[8][3] = { BBOX_POINTS(n->bbox[0], n->bbox[1]) };
                if (!draw_cam_.IsInFrustum(bbox_points)) continue;

                if (culling_enabled_) {
                    const auto &cam_pos = draw_cam_.world_position();

                    // do not question visibility of the node in which we are inside
                    if (cam_pos[0] < n->bbox[0][0] - 0.5f || cam_pos[1] < n->bbox[0][1] - 0.5f || cam_pos[2] < n->bbox[0][2] - 0.5f ||
                        cam_pos[0] > n->bbox[1][0] + 0.5f || cam_pos[1] > n->bbox[1][1] + 0.5f || cam_pos[2] > n->bbox[1][2] + 0.5f) {
                        SWcull_surf surf;

                        surf.type = SW_OCCLUDEE;
                        surf.prim_type = SW_TRIANGLES;
                        surf.index_type = SW_UNSIGNED_BYTE;
                        surf.attribs = &bbox_points[0][0];
                        surf.indices = &bbox_indices[0];
                        surf.stride = 3 * sizeof(float);
                        surf.count = 36;
                        surf.xform = Ren::ValuePtr(proj_from_identity);
                        surf.dont_skip = nullptr;

                        swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                        if (surf.visible == 0) continue;
                    }
                }

                if (!n->prim_count) {
                    stack[stack_size++] = n->left_child;
                    stack[stack_size++] = n->right_child;
                } else {
                    for (uint32_t i = n->prim_index; i < n->prim_index + n->prim_count; i++) {
                        const auto &obj = objects_[i];

                        uint32_t drawable_flags = HasMesh | HasTransform;
                        if ((obj.flags & drawable_flags) == drawable_flags) {
                            const auto *tr = obj.tr.get();

                            const float bbox_points[8][3] = { BBOX_POINTS(tr->bbox_min_ws, tr->bbox_max_ws) };
                            if (!draw_cam_.IsInFrustum(bbox_points)) continue;

                            if (culling_enabled_) {
                                const auto &cam_pos = draw_cam_.world_position();

                                // do not question visibility of the object in which we are inside
                                if (cam_pos[0] < tr->bbox_min_ws[0] - 0.5f || cam_pos[1] < tr->bbox_min_ws[1] - 0.5f || cam_pos[2] < tr->bbox_min_ws[2] - 0.5f ||
                                    cam_pos[0] > tr->bbox_max_ws[0] + 0.5f || cam_pos[1] > tr->bbox_max_ws[1] + 0.5f || cam_pos[2] > tr->bbox_max_ws[2] + 0.5f) {
                                    SWcull_surf surf;

                                    surf.type = SW_OCCLUDEE;
                                    surf.prim_type = SW_TRIANGLES;
                                    surf.index_type = SW_UNSIGNED_BYTE;
                                    surf.attribs = &bbox_points[0][0];
                                    surf.indices = &bbox_indices[0];
                                    surf.stride = 3 * sizeof(float);
                                    surf.count = 36;
                                    surf.xform = Ren::ValuePtr(proj_from_identity);
                                    surf.dont_skip = nullptr;

                                    swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                                    if (surf.visible == 0) continue;
                                }
                            }

                            const Ren::Mat4f &world_from_object = tr->mat;

                            Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                       proj_from_object = proj_from_view * view_from_object;

                            tr_list.push_back(proj_from_object);

                            const auto *mesh = obj.mesh.get();

                            size_t dr_start = dr_list.size();

                            const Ren::TriStrip *s = &mesh->strip(0);
                            while (s->offset != -1) {
                                dr_list.push_back({ &tr_list.back(), s->mat.get(), mesh, s });
                                ++s;
                            }
                        }
                    }
                }
            }

            std::sort(std::begin(dr_list), std::end(dr_list));

            while (!dr_list.empty() && dr_list.back().invisible) {
                dr_list.pop_back();
            }

            // gather lists for shadow map
            auto &shadow_cam = shadow_cam_[1];

            //shadow_cam = draw_cam_;
            shadow_cam.SetupView(draw_cam_.world_position() + Ren::Vec3f{ 0.0f, 10.0f, 0.0f }, draw_cam_.world_position(), Ren::Vec3f{ 0.0f, 0.0f, 1.0f });
            shadow_cam.Orthographic(-10.0f, 10.0f, 10.0f, -10.0f, 0.5f, 10.0f);
            shadow_cam.UpdatePlanes();

            view_from_world = shadow_cam.view_matrix(),
            proj_from_view = shadow_cam.projection_matrix();

            stack_size = 0;
            stack[stack_size++] = (uint32_t)root_node_;

            while (stack_size) {
                uint32_t cur = stack[--stack_size];
                const auto *n = &nodes_[cur];

                if (!shadow_cam.IsInFrustum(n->bbox[0], n->bbox[1])) continue;

                if (!n->prim_count) {
                    stack[stack_size++] = n->left_child;
                    stack[stack_size++] = n->right_child;
                } else {
                    for (uint32_t i = n->prim_index; i < n->prim_index + n->prim_count; i++) {
                        const auto &obj = objects_[i];

                        uint32_t drawable_flags = HasMesh | HasTransform;
                        if ((obj.flags & drawable_flags) == drawable_flags) {
                            const auto *tr = obj.tr.get();

                            if (!shadow_cam.IsInFrustum(tr->bbox_min_ws, tr->bbox_max_ws)) continue;

                            const Ren::Mat4f &world_from_object = tr->mat;

                            Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                       proj_from_object = proj_from_view * view_from_object;

                            tr_list.push_back(proj_from_object);

                            const auto *mesh = obj.mesh.get();

                            const Ren::TriStrip *s = &mesh->strip(0);
                            while (s->offset != -1) {
                                sh_dr_list.push_back({ &tr_list.back(), s->mat.get(), mesh, s });
                                ++s;
                            }
                        }
                    }
                }
            }

            if (debug_cull_ && culling_enabled_) {
                const float NEAR_CLIP = 0.5f;
                const float FAR_CLIP = 10000.0f;

                int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
                depth_pixels_[1].resize(w * h * 4);
                for (int x = 0; x < w; x++) {
                    for (int y = 0; y < h; y++) {
                        float z = cull_ctx_.zbuf.depth[(h - y - 1) * w + x];
                        z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                        depth_pixels_[1][4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                        depth_pixels_[1][4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                        depth_pixels_[1][4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                        depth_pixels_[1][4 * (y * w + x) + 3] = 255;
                    }
                }

                depth_tiles_[1].resize(w * h * 4);
                for (int x = 0; x < w; x++) {
                    for (int y = 0; y < h; y++) {
                        const auto *zr = swZbufGetTileRange(&cull_ctx_.zbuf, x, (h - y - 1));

                        float z = zr->min;
                        z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                        depth_tiles_[1][4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                        depth_tiles_[1][4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                        depth_tiles_[1][4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                        depth_tiles_[1][4 * (y * w + x) + 3] = 255;
                    }
                }
            }

            auto t2 = std::chrono::high_resolution_clock::now();
            back_timings_[1] = { t1, t2 };
        }

        notified_ = false;
    }
}

void Renderer::SwapDrawLists(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_node,
                             const SceneObject *objects, size_t object_count) {
    bool should_notify = false;
    {
        std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
        std::swap(transforms_[0], transforms_[1]);
        std::swap(draw_lists_[0], draw_lists_[1]);
        std::swap(shadow_list_[0], shadow_list_[1]);
        nodes_ = nodes;
        root_node_ = root_node;
        objects_ = objects;
        object_count_ = object_count;
        back_timings_[0] = back_timings_[1];
        draw_cam_ = cam;
        std::swap(shadow_cam_[0], shadow_cam_[1]);
        std::swap(depth_pixels_[0], depth_pixels_[1]);
        std::swap(depth_tiles_[0], depth_tiles_[1]);
        should_notify = (nodes != nullptr);
    }
    if (should_notify) {
        notified_ = true;
        thr_notify_.notify_all();
    }
}

#undef BBOX_POINTS