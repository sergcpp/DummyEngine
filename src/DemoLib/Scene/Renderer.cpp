#include "Renderer.h"

#include <Ren/Context.h>
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

    const int SHADOWMAP_RES = 4096;
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

Renderer::Renderer(Ren::Context &ctx) : ctx_(ctx) {
    using namespace RendererInternal;

    {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;
        SWfloat z = FAR_CLIP / (FAR_CLIP - NEAR_CLIP) + (NEAR_CLIP - (2.0f * NEAR_CLIP)) / (0.15f * (FAR_CLIP - NEAR_CLIP));
        swCullCtxInit(&cull_ctx_, 256, 128, z);
    }
    InitShadersInternal();

    shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::None, Ren::NoFilter, Ren::ClampToEdge, true);

    uint8_t data[] = { 0, 0, 0, 0 };

    Ren::Texture2DParams p;
    p.w = p.h = 1;
    p.format = Ren::RawRGBA8888;
    p.filter = Ren::Bilinear;

    Ren::eTexLoadStatus status;
    default_lightmap_ = ctx_.LoadTexture2D("default_lightmap", data, sizeof(data), p, &status);
    assert(status == Ren::TexCreatedFromData);

    /*try {
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::RawR32F, Ren::NoFilter, Ren::ClampToEdge, true);
    } catch (std::runtime_error &) {
        LOGI("Cannot create floating-point shadow buffer! Fallback to unsigned byte.");
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, Ren::RawRGB888, Ren::NoFilter, Ren::ClampToEdge, true);
    }*/

    background_thread_ = std::thread(std::bind(&Renderer::BackgroundProc, this));
}

Renderer::~Renderer() {
    if (background_thread_.joinable()) {
        shutdown_ = notified_ = true;
        thr_notify_.notify_all();
        background_thread_.join();
    }
    swCullCtxDestroy(&cull_ctx_);
}

void Renderer::DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_index,
                           const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env) {
    SwapDrawLists(cam, nodes, root_index, objects, obj_indices, object_count, env);
    auto t1 = std::chrono::high_resolution_clock::now();
    {
        size_t drawables_count = draw_lists_[0].size();
        const auto *drawables = (drawables_count == 0) ? nullptr : &draw_lists_[0][0];

        Ren::Mat4f shadow_transforms[4];
        size_t shadow_drawables_count[4];
        const DrawableItem *shadow_drawables[4];

        for (int i = 0; i < 4; i++) {
            Ren::Mat4f view_from_world = shadow_cam_[0][i].view_matrix(),
                       clip_from_view = shadow_cam_[0][i].projection_matrix();
            shadow_transforms[i] = clip_from_view * view_from_world;
            shadow_drawables_count[i] = shadow_list_[0][i].size();
            shadow_drawables[i] = (shadow_drawables_count[i] == 0) ? nullptr : &shadow_list_[0][i][0];
        }

        if (ctx_.w() != w_ || ctx_.h() != h_) {
            clean_buf_ = FrameBuf(ctx_.w(), ctx_.h(), Ren::RawRGB32F, Ren::NoFilter, Ren::ClampToEdge, true, 4);
            w_ = ctx_.w();
            h_ = ctx_.h();
            LOGI("CleanBuf resized to %ix%i", w_, h_);
        }

        DrawObjectsInternal(drawables, drawables_count, shadow_transforms, shadow_drawables, shadow_drawables_count, env_);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    timings_ = { t1, t2 };
}

void Renderer::WaitForBackgroundThreadIteration() {
    SwapDrawLists(draw_cam_, nullptr, 0, nullptr, nullptr, 0, env_);
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
            tr_list.reserve(object_count_ * 6);

            auto &dr_list = draw_lists_[1];
            dr_list.clear();
            dr_list.reserve(object_count_ * 16);

            object_to_drawable_.clear();
            object_to_drawable_.resize(object_count_, 0xffffffff);

            auto *sh_dr_list = shadow_list_[1];
            for (int i = 0; i < 4; i++) {
                sh_dr_list[i].clear();
                sh_dr_list[i].reserve(object_count_ * 16);
            }

            Ren::Mat4f view_from_world = draw_cam_.view_matrix(),
                       clip_from_view = draw_cam_.projection_matrix();

            swCullCtxClear(&cull_ctx_);

            Ren::Mat4f view_from_identity = view_from_world * Ren::Mat4f{ 1.0f },
                       clip_from_identity = clip_from_view * view_from_identity;

            uint32_t stack[MAX_STACK_SIZE];
            uint32_t stack_size = 0;

            {   // Rasterize occluder meshes into a small framebuffer
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
                            const auto &obj = objects_[obj_indices_[i]];

                            const uint32_t occluder_flags = HasMesh | HasTransform | HasOccluder;
                            if ((obj.flags & occluder_flags) == occluder_flags) {
                                const auto *tr = obj.tr.get();

                                if (!draw_cam_.IsInFrustum(tr->bbox_min_ws, tr->bbox_max_ws)) continue;

                                const Ren::Mat4f &world_from_object = tr->mat;

                                Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                           clip_from_object = clip_from_view * view_from_object;

                                const auto *mesh = obj.mesh.get();

                                SWcull_surf surf[16];
                                int surf_count = 0;

                                const Ren::TriStrip *s = &mesh->strip(0);
                                while (s->offset != -1) {
                                    surf[surf_count].type = SW_OCCLUDER;
                                    surf[surf_count].prim_type = SW_TRIANGLES;
                                    surf[surf_count].index_type = SW_UNSIGNED_INT;
                                    surf[surf_count].attribs = mesh->attribs();
                                    surf[surf_count].indices = ((const uint8_t *)mesh->indices() + s->offset);
                                    surf[surf_count].stride = 13 * sizeof(float);
                                    surf[surf_count].count = (SWuint)s->num_indices;
                                    surf[surf_count].xform = Ren::ValuePtr(clip_from_object);
                                    surf[surf_count].dont_skip = nullptr;
                                    surf_count++;

                                    ++s;
                                }

                                swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                            }
                        }
                    }
                }
            }

            {   // Gather drawable meshes, skip occluded and frustum culled
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
                            surf.xform = Ren::ValuePtr(clip_from_identity);
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
                            const auto &obj = objects_[obj_indices_[i]];

                            const uint32_t drawable_flags = HasMesh | HasTransform;
                            if ((obj.flags & drawable_flags) == drawable_flags) {
                                const auto *tr = obj.tr.get();

                                const float bbox_points[8][3] = { BBOX_POINTS(tr->bbox_min_ws, tr->bbox_max_ws) };
                                if (!draw_cam_.IsInFrustum(bbox_points)) continue;

                                if (culling_enabled_ && n->prim_count > 1) {
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
                                        surf.xform = Ren::ValuePtr(clip_from_identity);
                                        surf.dont_skip = nullptr;

                                        swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                                        if (surf.visible == 0) continue;
                                    }
                                }

                                const Ren::Mat4f &world_from_object = tr->mat;

                                Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                           clip_from_object = clip_from_view * view_from_object;

                                tr_list.push_back(clip_from_object);

                                const auto *mesh = obj.mesh.get();
                                const auto *lm_dir_tex = obj.lm_dir_tex ? obj.lm_dir_tex.get() : nullptr;
                                const auto *lm_indir_tex = obj.lm_indir_tex ? obj.lm_indir_tex.get() : nullptr;

                                size_t dr_start = dr_list.size();

                                object_to_drawable_[i] = (uint32_t)dr_list.size();

                                const Ren::TriStrip *s = &mesh->strip(0);
                                while (s->offset != -1) {
                                    dr_list.push_back({ &tr_list.back(), &world_from_object, s->mat.get(), mesh, s, lm_dir_tex, lm_indir_tex });
                                    ++s;
                                }
                            }
                        }
                    }
                }
            }

            // Planes, that define shadow map splits
            const float far_planes[] = { 8.0f, 24.0f, 56.0f, 120.0f };
            const float near_planes[] = { draw_cam_.near(), far_planes[0], far_planes[1], far_planes[2] };

            // Choose up vector for shadow camera
            auto light_dir = env_.sun_dir;
            auto cam_up = Ren::Vec3f{ 0.0f, 0.0, 1.0f };
            if (light_dir[0] <= light_dir[1] && light_dir[0] <= light_dir[2]) {
                cam_up = Ren::Vec3f{ 1.0f, 0.0, 0.0f };
            } else if (light_dir[1] <= light_dir[0] && light_dir[1] <= light_dir[2]) {
                cam_up = Ren::Vec3f{ 0.0f, 1.0, 0.0f };
            }
            // Calculate side vector of shadow camera
            auto cam_side = Normalize(Cross(light_dir, cam_up));
            cam_up = Cross(cam_side, light_dir);

            const Ren::Vec3f scene_dims = Ren::Vec3f{ nodes_[root_node_].bbox[1] } - Ren::Vec3f{ nodes_[root_node_].bbox[0] };
            const float max_dist = Ren::Length(scene_dims);

            // Gather drawables for each cascade
            for (int casc = 0; casc < 4; casc++) {
                auto &shadow_cam = shadow_cam_[1][casc];

                auto temp_cam = draw_cam_;
                temp_cam.Perspective(draw_cam_.angle(), draw_cam_.aspect(), near_planes[casc], far_planes[casc]);
                temp_cam.UpdatePlanes();

                const Ren::Mat4f &_view_from_world = temp_cam.view_matrix(),
                                 &_clip_from_view = temp_cam.projection_matrix();

                const Ren::Mat4f _clip_from_world = _clip_from_view * _view_from_world;
                const Ren::Mat4f _world_from_clip = Ren::Inverse(_clip_from_world);

                Ren::Vec3f bounding_center;
                const float bounding_radius = temp_cam.GetBoundingSphere(bounding_center);

                auto cam_target = bounding_center;
                
                {   // Snap camera movement to shadow map pixels
                    const float move_step = (2 * bounding_radius) / (0.5f * SHADOWMAP_RES);
                    //                      |_shadow map extent_|   |_res of one cascade_|

                    // Project target on shadow cam view matrix
                    float _dot_f = Ren::Dot(cam_target, light_dir),
                          _dot_s = Ren::Dot(cam_target, cam_side),
                          _dot_u = Ren::Dot(cam_target, cam_up);

                    // Snap coordinates to pixels
                    _dot_f = std::round(_dot_f / move_step) * move_step;
                    _dot_s = std::round(_dot_s / move_step) * move_step;
                    _dot_u = std::round(_dot_u / move_step) * move_step;

                    // Update target coordinates in world space
                    cam_target = _dot_f * light_dir + _dot_s * cam_side + _dot_u * cam_up;
                }

                auto cam_center = cam_target + max_dist * light_dir;

                shadow_cam.SetupView(cam_center, cam_target, cam_up);
                shadow_cam.Orthographic(-bounding_radius, bounding_radius, bounding_radius, -bounding_radius, 0.0f, max_dist + bounding_radius);
                shadow_cam.UpdatePlanes();

                view_from_world = shadow_cam.view_matrix(),
                clip_from_view = shadow_cam.projection_matrix();

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
                            const auto &obj = objects_[obj_indices_[i]];

                            const uint32_t drawable_flags = HasMesh | HasTransform;
                            if ((obj.flags & drawable_flags) == drawable_flags) {
                                const auto *tr = obj.tr.get();

                                if (!shadow_cam.IsInFrustum(tr->bbox_min_ws, tr->bbox_max_ws)) continue;

                                const Ren::Mat4f &world_from_object = tr->mat;

                                Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                           clip_from_object = clip_from_view * view_from_object;

                                tr_list.push_back(clip_from_object);

                                auto dr_index = object_to_drawable_[i];
                                if (dr_index != 0xffffffff) {
                                    auto *dr = &dr_list[dr_index];
                                    const auto *mesh = dr->mesh;

                                    const Ren::TriStrip *s = &mesh->strip(0);
                                    while (s->offset != -1) {
                                        dr->sh_clip_from_object[casc] = &tr_list.back();
                                        ++dr; ++s;
                                    }
                                }

                                const auto *mesh = obj.mesh.get();

                                const Ren::TriStrip *s = &mesh->strip(0);
                                while (s->offset != -1) {
                                    sh_dr_list[casc].push_back({ &tr_list.back(), nullptr, s->mat.get(), mesh, s, nullptr, nullptr });
                                    ++s;
                                }
                            }
                        }
                    }
                }
            }

            // Sort drawables to optimize state switches
            std::sort(std::begin(dr_list), std::end(dr_list));

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
                             const SceneObject *objects, const uint32_t *obj_indcies, size_t object_count, const Environment &env) {
    bool should_notify = false;
    {
        std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
        std::swap(transforms_[0], transforms_[1]);
        std::swap(draw_lists_[0], draw_lists_[1]);
        nodes_ = nodes;
        root_node_ = root_node;
        objects_ = objects;
        obj_indices_ = obj_indcies;
        object_count_ = object_count;
        back_timings_[0] = back_timings_[1];
        draw_cam_ = cam;
        for (int i = 0; i < 4; i++) {
            std::swap(shadow_list_[0][i], shadow_list_[1][i]);
            std::swap(shadow_cam_[0][i], shadow_cam_[1][i]);
        }
        env_ = env;
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