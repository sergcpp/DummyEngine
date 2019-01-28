#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>

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
                                        4, 6, 5,    5, 6, 7
                                      };

const int MAX_STACK_SIZE = 64;

const int SHADOWMAP_RES = 2048;
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

Renderer::Renderer(Ren::Context &ctx, std::shared_ptr<Sys::ThreadPool> &threads) : ctx_(ctx), threads_(threads) {
    using namespace RendererInternal;

    {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;
        SWfloat z = FAR_CLIP / (FAR_CLIP - NEAR_CLIP) + (NEAR_CLIP - (2.0f * NEAR_CLIP)) / (0.15f * (FAR_CLIP - NEAR_CLIP));
        swCullCtxInit(&cull_ctx_, 256, 128, z);
    }

    InitRendererInternal();

    {   // Create shadow map buffer
        shadow_buf_ = FrameBuf(SHADOWMAP_RES, SHADOWMAP_RES, nullptr, 0, true, Ren::BilinearNoMipmap);
    }

    {   // Create aux buffer which gathers frame luminance
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::RawR16F;
        desc.filter = Ren::Bilinear;
        desc.repeat = Ren::ClampToEdge;
        reduced_buf_ = FrameBuf(16, 8, &desc, 1, false);
    }

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
    DestroyRendererInternal();
    swCullCtxDestroy(&cull_ctx_);
}

void Renderer::DrawObjects(const Ren::Camera &cam, const bvh_node_t *nodes, size_t root_index,
                           const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env,
                           const TextureAtlas &decals_atlas) {
    SwapDrawLists(cam, nodes, root_index, objects, obj_indices, object_count, env, &decals_atlas);
    auto t1 = std::chrono::high_resolution_clock::now();
    {
        size_t drawables_count = draw_lists_[0].size();
        const auto *drawables = (drawables_count == 0) ? nullptr : &draw_lists_[0][0];

        size_t lights_count = light_sources_[0].size();
        const auto *lights = (lights_count == 0) ? nullptr : &light_sources_[0][0];

        size_t decals_count = decals_[0].size();
        const auto *decals = (decals_count == 0) ? nullptr : &decals_[0][0];

        const auto *cells = cells_[0].empty() ? nullptr : &cells_[0][0];

        size_t items_count = items_count_[0];
        const auto *items = (items_count == 0) ? nullptr : &items_[0][0];

        const auto *p_decals_atlas = decals_atlas_[0];

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
            {   // Main buffer for raw frame before tonemapping
                FrameBuf::ColorAttachmentDesc desc[3];
                {   // Main color
                    desc[0].format = Ren::RawRGBA16F;
                    desc[0].filter = Ren::NoFilter;
                    desc[0].repeat = Ren::ClampToEdge;
                }
                {   // Clip-space normal
                    desc[1].format = Ren::RawRG16F;
                    desc[1].filter = Ren::BilinearNoMipmap;
                    desc[1].repeat = Ren::ClampToEdge;
                }
                {   // 4-component specular (alpha is roughness)
                    desc[2].format = Ren::RawRGBA8888;
                    desc[2].filter = Ren::BilinearNoMipmap;
                    desc[2].repeat = Ren::ClampToEdge;
                }
                clean_buf_ = FrameBuf(ctx_.w(), ctx_.h(), desc, 3, true, Ren::NoFilter, 4);
            }
            {   // Auxilary buffer for SSR
                FrameBuf::ColorAttachmentDesc desc;
                desc.format = Ren::RawRGBA16F;
                desc.filter = Ren::Bilinear;
                desc.repeat = Ren::ClampToEdge;
                down_buf_ = FrameBuf(ctx_.w() / 4, ctx_.h() / 4, &desc, 1, false);
            }
            {   // Auxilary buffers for bloom effect
                FrameBuf::ColorAttachmentDesc desc;
                desc.format = Ren::RawRGBA16F;
                desc.filter = Ren::Bilinear;
                desc.repeat = Ren::ClampToEdge;
                blur_buf1_ = FrameBuf(ctx_.w() / 4, ctx_.h() / 4, &desc, 1, false);
                blur_buf2_ = FrameBuf(ctx_.w() / 4, ctx_.h() / 4, &desc, 1, false);
            }
            w_ = ctx_.w();
            h_ = ctx_.h();
            LOGI("CleanBuf resized to %ix%i", w_, h_);
        }

        DrawObjectsInternal(drawables, drawables_count, lights, lights_count, decals, decals_count,
                            cells, items, items_count, shadow_transforms, shadow_drawables, shadow_drawables_count, env_,
                            p_decals_atlas);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    timings_ = { t1, t2 };
}

void Renderer::WaitForBackgroundThreadIteration() {
    SwapDrawLists(draw_cam_, nullptr, 0, nullptr, nullptr, 0, env_, nullptr);
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

            auto &ls_list = light_sources_[1];
            ls_list.clear();

            auto &de_list = decals_[1];
            de_list.clear();

            auto &cells = cells_[1];
            cells.resize(CELLS_COUNT);

            auto &items = items_[1];
            items.resize(MAX_ITEMS_TOTAL);

            object_to_drawable_.clear();
            object_to_drawable_.resize(object_count_, 0xffffffff);

            litem_to_lsource_.clear();
            ditem_to_decal_.clear();
            decals_boxes_.clear();

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

            tr_list.push_back(clip_from_identity);

            const uint32_t skip_check_bit = (1 << 31);
            const uint32_t index_bits = ~skip_check_bit;

            uint32_t stack[MAX_STACK_SIZE];
            uint32_t stack_size = 0;

            {   // Rasterize occluder meshes into a small framebuffer
                stack[stack_size++] = (uint32_t)root_node_;

                while (stack_size && culling_enabled_) {
                    uint32_t cur = stack[--stack_size] & index_bits;
                    uint32_t skip_check = (stack[stack_size] & skip_check_bit);
                    const auto *n = &nodes_[cur];

                    if (!skip_check) {
                        auto res = draw_cam_.CheckFrustumVisibility(n->bbox[0], n->bbox[1]);
                        if (res == Ren::Invisible) continue;
                        else if (res == Ren::FullyVisible) skip_check = skip_check_bit;
                    }

                    if (!n->prim_count) {
                        stack[stack_size++] = skip_check | n->left_child;
                        stack[stack_size++] = skip_check | n->right_child;
                    } else {
                        for (uint32_t i = n->prim_index; i < n->prim_index + n->prim_count; i++) {
                            const auto &obj = objects_[obj_indices_[i]];

                            const uint32_t occluder_flags = HasTransform | HasOccluder;
                            if ((obj.flags & occluder_flags) == occluder_flags) {
                                const auto *tr = obj.tr.get();

                                if (!skip_check &&
                                    draw_cam_.CheckFrustumVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                                const Ren::Mat4f &world_from_object = tr->mat;

                                Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                           clip_from_object = clip_from_view * view_from_object;

                                const auto *mesh = obj.mesh.get();

                                SWcull_surf surf[16];
                                int surf_count = 0;

                                const Ren::TriGroup *s = &mesh->group(0);
                                while (s->offset != -1) {
                                    SWcull_surf *_surf = &surf[surf_count++];

                                    _surf->type = SW_OCCLUDER;
                                    _surf->prim_type = SW_TRIANGLES;
                                    _surf->index_type = SW_UNSIGNED_INT;
                                    _surf->attribs = mesh->attribs();
                                    _surf->indices = ((const uint8_t *)mesh->indices() + s->offset);
                                    _surf->stride = 13 * sizeof(float);
                                    _surf->count = (SWuint)s->num_indices;
                                    _surf->base_vertex = -SWint(mesh->attribs_offset() / _surf->stride);
                                    _surf->xform = Ren::ValuePtr(clip_from_object);
                                    _surf->dont_skip = nullptr;

                                    ++s;
                                }

                                swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                            }
                        }
                    }
                }
            }

            {   // Gather meshes and lights, skip occluded and frustum culled
                const uint32_t skip_check_bit = (1 << 31);
                const uint32_t index_bits = ~skip_check_bit;

                stack_size = 0;
                stack[stack_size++] = (uint32_t)root_node_;

                while (stack_size) {
                    uint32_t cur = stack[--stack_size] & index_bits;
                    uint32_t skip_check = stack[stack_size] & skip_check_bit;
                    const auto *n = &nodes_[cur];

                    if (!skip_check) {
                        const float bbox_points[8][3] = { BBOX_POINTS(n->bbox[0], n->bbox[1]) };
                        auto res = draw_cam_.CheckFrustumVisibility(bbox_points);
                        if (res == Ren::Invisible) continue;
                        else if (res == Ren::FullyVisible) skip_check = skip_check_bit;

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
                                surf.base_vertex = 0;
                                surf.xform = Ren::ValuePtr(clip_from_identity);
                                surf.dont_skip = nullptr;

                                swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                                if (surf.visible == 0) continue;
                            }
                        }
                    }

                    if (!n->prim_count) {
                        stack[stack_size++] = skip_check | n->left_child;
                        stack[stack_size++] = skip_check | n->right_child;
                    } else {
                        for (uint32_t i = n->prim_index; i < n->prim_index + n->prim_count; i++) {
                            const auto &obj = objects_[obj_indices_[i]];

                            const uint32_t drawable_flags = HasMesh | HasTransform;
                            const uint32_t lightsource_flags = HasLightSource | HasTransform;
                            if ((obj.flags & drawable_flags) == drawable_flags ||
                                (obj.flags & lightsource_flags) == lightsource_flags) {
                                const auto *tr = obj.tr.get();

                                if (!skip_check) {
                                    const float bbox_points[8][3] = { BBOX_POINTS(tr->bbox_min_ws, tr->bbox_max_ws) };
                                    if (draw_cam_.CheckFrustumVisibility(bbox_points) == Ren::Invisible) continue;

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
                                            surf.base_vertex = 0;
                                            surf.xform = Ren::ValuePtr(clip_from_identity);
                                            surf.dont_skip = nullptr;

                                            swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                                            if (surf.visible == 0) continue;
                                        }
                                    }
                                }

                                const Ren::Mat4f &world_from_object = tr->mat;

                                Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                           clip_from_object = clip_from_view * view_from_object;

                                if (obj.flags & HasMesh) {
                                    tr_list.push_back(clip_from_object);

                                    const auto *mesh = obj.mesh.get();
                                    const auto *lm_dir_tex = obj.lm_dir_tex ? obj.lm_dir_tex.get() : nullptr;
                                    const auto *lm_indir_tex = obj.lm_indir_tex ? obj.lm_indir_tex.get() : nullptr;

                                    size_t dr_start = dr_list.size();

                                    object_to_drawable_[i] = (uint32_t)dr_list.size();

                                    const Ren::TriGroup *s = &mesh->group(0);
                                    while (s->offset != -1) {
                                        dr_list.push_back({ &tr_list.back(), &world_from_object, s->mat.get(), mesh, s, lm_dir_tex, lm_indir_tex });

                                        if (obj.lm_indir_sh_tex[0]) {
                                            dr_list.back().lm_indir_sh_tex[0] = obj.lm_indir_sh_tex[0].get();
                                            dr_list.back().lm_indir_sh_tex[1] = obj.lm_indir_sh_tex[1].get();
                                            dr_list.back().lm_indir_sh_tex[2] = obj.lm_indir_sh_tex[2].get();
                                            dr_list.back().lm_indir_sh_tex[3] = obj.lm_indir_sh_tex[3].get();
                                        }

                                        ++s;
                                    }
                                }

                                if (obj.flags & HasLightSource) {
                                    for (int i = 0; i < LIGHTS_PER_OBJECT; i++) {
                                        if (!obj.ls[i]) break;

                                        const auto *light = obj.ls[i].get();

                                        Ren::Vec4f pos = { light->offset[0], light->offset[1], light->offset[2], 1.0f };
                                        pos = world_from_object * pos;
                                        pos /= pos[3];

                                        Ren::Vec4f dir = { -light->dir[0], -light->dir[1], -light->dir[2], 0.0f };
                                        dir = world_from_object * dir;

                                        
                                        if (!skip_check) {
                                            auto res = Ren::FullyVisible;

                                            for (int k = 0; k < 6; k++) {
                                                const auto &plane = draw_cam_.frustum_plane(k);

                                                float dist = plane.n[0] * pos[0] +
                                                             plane.n[1] * pos[1] +
                                                             plane.n[2] * pos[2] + plane.d;

                                                if (dist < -light->influence) {
                                                    res = Ren::Invisible;
                                                    break;
                                                } else if (std::abs(dist) < light->influence) {
                                                    res = Ren::PartiallyVisible;
                                                }
                                            }

                                            if (res == Ren::Invisible) continue;
                                        }

                                        ls_list.emplace_back();
                                        litem_to_lsource_.push_back(light);

                                        auto &ls = ls_list.back();

                                        memcpy(&ls.pos[0], &pos[0], 3 * sizeof(float));
                                        ls.radius = light->radius;
                                        memcpy(&ls.col[0], &light->col[0], 3 * sizeof(float));
                                        ls.brightness = light->brightness;
                                        memcpy(&ls.dir[0], &dir[0], 3 * sizeof(float));
                                        ls.spot = light->spot;
                                    }
                                }

                                if (obj.flags & HasDecal) {
                                    Ren::Mat4f object_from_world = Ren::Inverse(world_from_object);

                                    for (int i = 0; i < DECALS_PER_OBJECT; i++) {
                                        if (!obj.de[i]) break;

                                        const auto *decal = obj.de[i].get();

                                        const Ren::Mat4f &view_from_object = decal->view,
                                                         &clip_from_view = decal->proj;

                                        Ren::Mat4f view_from_world = view_from_object * object_from_world,
                                                   clip_from_world = clip_from_view * view_from_world;

                                        Ren::Mat4f world_from_clip = Ren::Inverse(clip_from_world);

                                        Ren::Vec4f bbox_points[] = {
                                            { -1.0f, -1.0f, -1.0f, 1.0f }, { -1.0f, 1.0f, -1.0f, 1.0f },
                                            { 1.0f, 1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, -1.0f, 1.0f },

                                            { -1.0f, -1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f, 1.0f },
                                            { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f, 1.0f }
                                        };

                                        Ren::Vec3f bbox_min = Ren::Vec3f{ std::numeric_limits<float>::max() },
                                                   bbox_max = Ren::Vec3f{ std::numeric_limits<float>::lowest() };

                                        for (int k = 0; k < 8; k++) {
                                            bbox_points[k] = world_from_clip * bbox_points[k];
                                            bbox_points[k] /= bbox_points[k][3];

                                            bbox_min = Ren::Min(bbox_min, Ren::Vec3f{ bbox_points[k] });
                                            bbox_max = Ren::Max(bbox_max, Ren::Vec3f{ bbox_points[k] });
                                        }

                                        auto res = Ren::FullyVisible;

                                        if (!skip_check) {
                                            for (int p = Ren::LeftPlane; p <= Ren::FarPlane; p++) {
                                                const auto &plane = draw_cam_.frustum_plane(p);

                                                int in_count = 8;

                                                for (int k = 0; k < 8; k++) {
                                                    float dist = plane.n[0] * bbox_points[k][0] +
                                                        plane.n[1] * bbox_points[k][1] +
                                                        plane.n[2] * bbox_points[k][2] + plane.d;
                                                    if (dist < 0.0f) {
                                                        in_count--;
                                                    }
                                                }

                                                if (in_count == 0) {
                                                    res = Ren::Invisible;
                                                    break;
                                                } else if (in_count != 8) {
                                                    res = Ren::PartiallyVisible;
                                                }
                                            }
                                        }

                                        if (res != Ren::Invisible) {
                                            de_list.emplace_back();
                                            ditem_to_decal_.push_back(decal);
                                            decals_boxes_.push_back({ bbox_min, bbox_max });

                                            Ren::Mat4f clip_from_world_transposed = Ren::Transpose(clip_from_world);

                                            auto &de = de_list.back();
                                            memcpy(&de.mat[0][0], &clip_from_world_transposed[0][0], 12 * sizeof(float));
                                            memcpy(&de.diff[0], &decal->diff[0], 4 * sizeof(float));
                                            memcpy(&de.norm[0], &decal->norm[0], 4 * sizeof(float));
                                        }
                                    }
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

                {
                    // Snap camera movement to shadow map pixels
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

                const uint32_t skip_check_bit = (1 << 31);
                const uint32_t index_bits = ~skip_check_bit;

                stack_size = 0;
                stack[stack_size++] = (uint32_t)root_node_;

                while (stack_size) {
                    uint32_t cur = stack[--stack_size] & index_bits;
                    uint32_t skip_check = stack[stack_size] & skip_check_bit;
                    const auto *n = &nodes_[cur];

                    auto res = shadow_cam.CheckFrustumVisibility(n->bbox[0], n->bbox[1]);
                    if (res == Ren::Invisible) continue;
                    else if (res == Ren::FullyVisible) skip_check = skip_check_bit;

                    if (!n->prim_count) {
                        stack[stack_size++] = skip_check | n->left_child;
                        stack[stack_size++] = skip_check | n->right_child;
                    } else {
                        for (uint32_t i = n->prim_index; i < n->prim_index + n->prim_count; i++) {
                            const auto &obj = objects_[obj_indices_[i]];

                            const uint32_t drawable_flags = HasMesh | HasTransform;
                            if ((obj.flags & drawable_flags) == drawable_flags) {
                                const auto *tr = obj.tr.get();

                                if (!skip_check &&
                                    shadow_cam.CheckFrustumVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                                const Ren::Mat4f &world_from_object = tr->mat;

                                Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                           clip_from_object = clip_from_view * view_from_object;

                                tr_list.push_back(clip_from_object);

                                auto dr_index = object_to_drawable_[i];
                                if (dr_index != 0xffffffff) {
                                    auto *dr = &dr_list[dr_index];
                                    const auto *mesh = dr->mesh;

                                    const Ren::TriGroup *s = &mesh->group(0);
                                    while (s->offset != -1) {
                                        dr->sh_clip_from_object[casc] = &tr_list.back();
                                        ++dr;
                                        ++s;
                                    }
                                }

                                const auto *mesh = obj.mesh.get();

                                const Ren::TriGroup *s = &mesh->group(0);
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

            if (!ls_list.empty() || !de_list.empty()) {
                std::vector<Ren::Frustum> sub_frustums;
                sub_frustums.resize(CELLS_COUNT);

                draw_cam_.ExtractSubFrustums(GRID_RES_X, GRID_RES_Y, GRID_RES_Z, &sub_frustums[0]);

                const int lights_count = (int)ls_list.size();
                const auto *lights = lights_count ? &ls_list[0] : nullptr;
                const auto *litem_to_lsource = lights_count ? &litem_to_lsource_[0] : nullptr;

                const int decals_count = (int)de_list.size();
                const auto *decals = decals_count ? &de_list[0] : nullptr;
                const auto *decals_boxes = decals_count ? &decals_boxes_[0] : nullptr;

                std::vector<std::future<void>> futures;
                std::atomic_int items_count = {};

                for (int i = 0; i < GRID_RES_Z; i++) {
                    futures.push_back(
                        threads_->enqueue(GatherItemsForZSlice_Job, i, &sub_frustums[0], lights, lights_count, decals, decals_count, decals_boxes, litem_to_lsource, &cells[0], &items[0], std::ref(items_count))
                    );
                }

                for (int i = 0; i < GRID_RES_Z; i++) {
                    futures[i].wait();
                }

                items_count_[1] = std::min(items_count.load(), MAX_ITEMS_TOTAL);
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
                             const SceneObject *objects, const uint32_t *obj_indices, size_t object_count, const Environment &env,
                             const TextureAtlas *decals_atlas) {
    bool should_notify = false;

    std::unique_lock<std::mutex> lock(mtx_);
    while (notified_) {
        thr_done_.wait(lock);
    }

    {
        std::lock_guard<Sys::SpinlockMutex> _(job_mtx_);
        std::swap(transforms_[0], transforms_[1]);
        std::swap(draw_lists_[0], draw_lists_[1]);
        std::swap(light_sources_[0], light_sources_[1]);
        std::swap(decals_[0], decals_[1]);
        std::swap(cells_[0], cells_[1]);
        std::swap(items_[0], items_[1]);
        std::swap(items_count_[0], items_count_[1]);
        std::swap(decals_atlas_[0], decals_atlas_[1]);
        decals_atlas_[1] = decals_atlas;
        nodes_ = nodes;
        root_node_ = root_node;
        objects_ = objects;
        obj_indices_ = obj_indices;
        object_count_ = object_count;
        back_timings_[0] = back_timings_[1];
        render_infos_[0] = render_infos_[1];
        for (int i = 0; i < TimersCount; i++) {
            std::swap(queries_[0][i], queries_[1][i]);
        }
        draw_cam_ = cam;
        for (int i = 0; i < 4; i++) {
            std::swap(shadow_list_[0][i], shadow_list_[1][i]);
            std::swap(shadow_cam_[0][i], shadow_cam_[1][i]);
        }
        env_ = env;
        std::swap(depth_pixels_[0], depth_pixels_[1]);
        std::swap(depth_tiles_[0], depth_tiles_[1]);
        if (nodes != nullptr) {
            should_notify = true;
        } else {
            draw_lists_[1].clear();
        }
    }

    if (should_notify) {
        notified_ = true;
        thr_notify_.notify_all();
    }
}

void Renderer::GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                        const DecalItem *decals, int decals_count, const BBox *decals_boxes,
                                        const LightSource * const*litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count) {
    using namespace RendererInternal;

    const int frustums_per_slice = GRID_RES_X * GRID_RES_Y;
    const int i = slice * frustums_per_slice;
    const auto *first_sf = &sub_frustums[i];

    // Reset cells information for slice
    for (int s = 0; s < frustums_per_slice; s++) {
        auto &cell = cells[i + s];
        cell = {};
    }

    // Gather to local list first
    ItemData local_items[GRID_RES_X * GRID_RES_Y][MAX_ITEMS_PER_CELL];

    for (int j = 0; j < lights_count; j++) {
        const auto &l = lights[j];
        const float influence = litem_to_lsource[j]->influence;
        const float *l_pos = &l.pos[0];

        auto visible_to_slice = Ren::FullyVisible;

        // Check if light is inside of a whole slice
        for (int k = Ren::NearPlane; k <= Ren::FarPlane; k++) {
            float dist = first_sf->planes[k].n[0] * l_pos[0] +
                first_sf->planes[k].n[1] * l_pos[1] +
                first_sf->planes[k].n[2] * l_pos[2] + first_sf->planes[k].d;
            if (dist < -influence) {
                visible_to_slice = Ren::Invisible;
            }
        }

        // Skip light for whole slice
        if (visible_to_slice == Ren::Invisible) continue;

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += GRID_RES_X) {
            const auto *first_line_sf = first_sf + row_offset;

            auto visible_to_line = Ren::FullyVisible;

            // Check if light is inside of grid line
            for (int k = Ren::TopPlane; k <= Ren::BottomPlane; k++) {
                float dist = first_line_sf->planes[k].n[0] * l_pos[0] +
                    first_line_sf->planes[k].n[1] * l_pos[1] +
                    first_line_sf->planes[k].n[2] * l_pos[2] + first_line_sf->planes[k].d;
                if (dist < -influence) {
                    visible_to_line = Ren::Invisible;
                }
            }

            // Skip light for whole line
            if (visible_to_line == Ren::Invisible) continue;

            for (int col_offset = 0; col_offset < GRID_RES_X; col_offset++) {
                const auto *sf = first_line_sf + col_offset;

                auto res = Ren::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = Ren::LeftPlane; k <= Ren::RightPlane; k++) {
                    float dist = sf->planes[k].n[0] * l_pos[0] +
                        sf->planes[k].n[1] * l_pos[1] +
                        sf->planes[k].n[2] * l_pos[2] + sf->planes[k].d;

                    if (dist < -influence) {
                        res = Ren::Invisible;
                    }
                }

                if (res != Ren::Invisible) {
                    const int index = i + row_offset + col_offset;
                    auto &cell = cells[index];
                    if (cell.light_count < MAX_LIGHTS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.light_count].light_index = (uint16_t)j;
                        cell.light_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < decals_count; j++) {
        const auto &de = decals[j];

        const float bbox_points[8][3] = { BBOX_POINTS(decals_boxes[j].bmin, decals_boxes[j].bmax) };

        auto visible_to_slice = Ren::FullyVisible;

        // Check if decal is inside of a whole slice
        for (int k = Ren::NearPlane; k <= Ren::FarPlane; k++) {
            int in_count = 8;

            for (int i = 0; i < 8; i++) {
                float dist = first_sf->planes[k].n[0] * bbox_points[i][0] +
                             first_sf->planes[k].n[1] * bbox_points[i][1] +
                             first_sf->planes[k].n[2] * bbox_points[i][2] + first_sf->planes[k].d;
                if (dist < 0.0f) {
                    in_count--;
                }
            }

            if (in_count == 0) {
                visible_to_slice = Ren::Invisible;
                break;
            }
        }

        // Skip decal for whole slice
        if (visible_to_slice == Ren::Invisible) continue;

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += GRID_RES_X) {
            const auto *first_line_sf = first_sf + row_offset;

            auto visible_to_line = Ren::FullyVisible;

            // Check if decal is inside of grid line
            for (int k = Ren::TopPlane; k <= Ren::BottomPlane; k++) {
                int in_count = 8;

                for (int i = 0; i < 8; i++) {
                    float dist = first_line_sf->planes[k].n[0] * bbox_points[i][0] +
                                 first_line_sf->planes[k].n[1] * bbox_points[i][1] +
                                 first_line_sf->planes[k].n[2] * bbox_points[i][2] + first_line_sf->planes[k].d;
                    if (dist < 0.0f) {
                        in_count--;
                    }
                }

                if (in_count == 0) {
                    visible_to_line = Ren::Invisible;
                    break;
                }
            }

            // Skip decal for whole line
            if (visible_to_line == Ren::Invisible) continue;

            for (int col_offset = 0; col_offset < GRID_RES_X; col_offset++) {
                const auto *sf = first_line_sf + col_offset;

                auto res = Ren::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = Ren::LeftPlane; k <= Ren::RightPlane; k++) {
                    int in_count = 8;

                    for (int i = 0; i < 8; i++) {
                        float dist = sf->planes[k].n[0] * bbox_points[i][0] +
                                     sf->planes[k].n[1] * bbox_points[i][1] +
                                     sf->planes[k].n[2] * bbox_points[i][2] + sf->planes[k].d;
                        if (dist < 0.0f) {
                            in_count--;
                        }
                    }

                    if (in_count == 0) {
                        res = Ren::Invisible;
                        break;
                    }
                }

                if (res != Ren::Invisible) {
                    const int index = i + row_offset + col_offset;
                    auto &cell = cells[index];
                    if (cell.decal_count < MAX_DECALS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.decal_count].decal_index = (uint16_t)j;
                        cell.decal_count++;
                    }
                }
            }
        }
    }

    // Pack gathered local item data to total list
    for (int s = 0; s < frustums_per_slice; s++) {
        auto &cell = cells[i + s];

        if (cell.decal_count == 2) {
            volatile int ii = 0;
        }

        int local_items_count = std::max((int)cell.light_count, (int)cell.decal_count);

        if (local_items_count) {
            cell.item_offset = items_count.fetch_add(local_items_count);
            if (cell.item_offset > MAX_ITEMS_TOTAL) {
                cell.item_offset = 0;
                cell.light_count = 0;
                cell.decal_count = 0;
            } else {
                int free_items_left = MAX_ITEMS_TOTAL - cell.item_offset;

                if ((int)cell.light_count > free_items_left) cell.light_count = free_items_left;
                if ((int)cell.decal_count > free_items_left) cell.decal_count = free_items_left;

                memcpy(&items[cell.item_offset], &local_items[s][0], local_items_count * sizeof(ItemData));
            }
        }
    }
}

#undef BBOX_POINTS