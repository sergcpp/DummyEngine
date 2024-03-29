#include "Renderer.h"

#include <cfloat>

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace RendererInternal {
bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]) {
    return p[0] > bbox_min[0] && p[0] < bbox_max[0] && p[1] > bbox_min[1] && p[1] < bbox_max[1] && p[2] > bbox_min[2] &&
           p[2] < bbox_max[2];
}

static const uint32_t bbox_indices[] = {0, 1, 2, 2, 1, 3, 0, 4, 5, 0, 5, 1, 0, 2, 4, 4, 2, 6,
                                        2, 3, 6, 6, 3, 7, 3, 1, 5, 3, 5, 7, 4, 6, 5, 5, 6, 7};

template <typename T> void RadixSort_LSB_Step(const unsigned shift, const T *begin, const T *end, T *begin1) {
    size_t count[0x100] = {};
    for (const T *p = begin; p != end; p++) {
        count[(p->key >> shift) & 0xFFu]++;
    }
    T *bucket[0x100], *q = begin1;
    for (int i = 0; i < 0x100; q += count[i++]) {
        bucket[i] = q;
    }
    for (const T *p = begin; p != end; p++) {
        *bucket[(p->key >> shift) & 0xFFu]++ = *p;
    }
}

template <typename T> void RadixSort_LSB(T *begin, T *end, T *begin1) {
    T *end1 = begin1 + (end - begin);

    for (unsigned shift = 0; shift < sizeof(T::key) * 8; shift += 8) {
        RadixSort_LSB_Step(shift, begin, end, begin1);

        std::swap(begin, begin1);
        std::swap(end, end1);
    }
}

#if defined(USE_VK_RENDER)
static const Ren::Vec4f ClipFrustumPoints[] = {
    Ren::Vec4f{-1.0f, -1.0f, 0.0f, 1.0f}, Ren::Vec4f{1.0f, -1.0f, 0.0f, 1.0f},
    Ren::Vec4f{1.0f, 1.0f, 0.0f, 1.0f},   Ren::Vec4f{-1.0f, 1.0f, 0.0f, 1.0f},

    Ren::Vec4f{-1.0f, -1.0f, 1.0f, 1.0f}, Ren::Vec4f{1.0f, -1.0f, 1.0f, 1.0f},
    Ren::Vec4f{1.0f, 1.0f, 1.0f, 1.0f},   Ren::Vec4f{-1.0f, 1.0f, 1.0f, 1.0f}};
#else
static const Ren::Vec4f ClipFrustumPoints[] = {
    Ren::Vec4f{-1.0f, -1.0f, -1.0f, 1.0f}, Ren::Vec4f{1.0f, -1.0f, -1.0f, 1.0f},
    Ren::Vec4f{1.0f, 1.0f, -1.0f, 1.0f},   Ren::Vec4f{-1.0f, 1.0f, -1.0f, 1.0f},

    Ren::Vec4f{-1.0f, -1.0f, 1.0f, 1.0f},  Ren::Vec4f{1.0f, -1.0f, 1.0f, 1.0f},
    Ren::Vec4f{1.0f, 1.0f, 1.0f, 1.0f},    Ren::Vec4f{-1.0f, 1.0f, 1.0f, 1.0f}};
#endif

Ren::Vec3f FindSupport(const Ren::Vec3f &bbox_min, const Ren::Vec3f &bbox_max, const Ren::Vec3f &dir) {
    const Ren::Vec3f points[8] = {bbox_min,
                                  Ren::Vec3f{bbox_min[0], bbox_min[1], bbox_max[2]},
                                  Ren::Vec3f{bbox_min[0], bbox_max[1], bbox_min[2]},
                                  Ren::Vec3f{bbox_min[0], bbox_max[1], bbox_max[2]},
                                  Ren::Vec3f{bbox_max[0], bbox_min[1], bbox_min[2]},
                                  Ren::Vec3f{bbox_max[0], bbox_min[1], bbox_max[2]},
                                  Ren::Vec3f{bbox_max[0], bbox_max[1], bbox_min[2]},
                                  bbox_max};

    Ren::Vec3f ret = points[0];
    float max_dist = Ren::Dot(dir, bbox_max);

    for (int i = 1; i < 8; ++i) {
        const float dist = Dot(dir, points[i]);
        if (dist > max_dist) {
            max_dist = dist;
            ret = points[i];
        }
    }

    return ret;
}

static const uint8_t SunShadowUpdatePattern[4] = {
    0b11111111, // update cascade 0 every frame
    0b11111111, // update cascade 1 every frame
    0b01010101, // update cascade 2 once in two frames
    0b00000010  // update cascade 3 once in eight frames
};

static const bool EnableSunCulling = true;

static const uint32_t SkipFrustumCheckBit = (1u << 31u);
static const uint32_t IndexBits = ~SkipFrustumCheckBit;

void __push_ellipsoids(const Eng::Drawable &dr, const Ren::Mat4f &world_from_object, Eng::DrawList &list);
uint32_t __push_skeletal_mesh(uint32_t skinned_buf_vtx_offset, const Eng::AnimState &as, const Ren::Mesh *mesh,
                              Eng::DrawList &list);
uint32_t __record_texture(std::vector<Eng::TexEntry> &storage, const Ren::Tex2DRef &tex, int prio, uint16_t distance);
void __record_textures(std::vector<Eng::TexEntry> &storage, const Ren::Material *mat, bool is_animated,
                       uint16_t distance);

__itt_string_handle *itt_gather_str = __itt_string_handle_create("GatherDrawables");
__itt_string_handle *itt_proc_occluders_str = __itt_string_handle_create("ProcessOccluders");
} // namespace RendererInternal

#define REN_UNINITIALIZE_X2(t)                                                                                         \
    t{Ren::Uninitialize}, t { Ren::Uninitialize }
#define REN_UNINITIALIZE_X4(t) REN_UNINITIALIZE_X2(t), REN_UNINITIALIZE_X2(t)
#define REN_UNINITIALIZE_X8(t) REN_UNINITIALIZE_X4(t), REN_UNINITIALIZE_X4(t)

#define BBOX_POINTS(min, max)                                                                                          \
    (min)[0], (min)[1], (min)[2], (max)[0], (min)[1], (min)[2], (min)[0], (min)[1], (max)[2], (max)[0], (min)[1],      \
        (max)[2], (min)[0], (max)[1], (min)[2], (max)[0], (max)[1], (min)[2], (min)[0], (max)[1], (max)[2], (max)[0],  \
        (max)[1], (max)[2]

#define _CROSS(x, y)                                                                                                   \
    { (x)[1] * (y)[2] - (x)[2] * (y)[1], (x)[2] * (y)[0] - (x)[0] * (y)[2], (x)[0] * (y)[1] - (x)[1] * (y)[0] }

void Eng::Renderer::GatherDrawables(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam,
                                    DrawList &list) {
    using namespace RendererInternal;
    using namespace Ren;

    OPTICK_EVENT();
    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_gather_str);

    const uint64_t iteration_start = Sys::GetTimeUs();

    list.frame_index = frame_index_;
    list.draw_cam = cam;
    list.ext_cam = ext_cam;
    list.env = EnvironmentWeak{scene.env};

    list.materials = &scene.materials;
    list.decals_atlas = &scene.decals_atlas;
    list.probe_storage = &scene.probe_storage;

    // mask render settings with what renderer itself is capable of
    list.render_settings &= settings;

    if (list.render_settings.debug_bvh) {
        // copy nodes list for debugging
        list.temp_nodes = scene.nodes;
        list.root_index = scene.root_node;
    } else {
        // free allocated memory
        list.temp_nodes = {};
    }

    if (list.render_settings.debug_freeze) {
        return;
    }

    list.lights.clear();
    list.decals.clear();
    list.probes.clear();
    list.ellipsoids.clear();

    list.instance_indices.clear();
    list.shadow_batches.clear();
    list.basic_batches.clear();
    list.custom_batches.clear();

    list.shadow_lists.count = 0;
    list.shadow_regions.count = 0;

    list.skin_transforms.clear();
    list.skin_regions.clear();
    list.shape_keys_data.count = 0;
    list.skin_vertices_count = 0;

    list.rt_geo_instances.count = 0;
    for (auto &rt : list.rt_obj_instances) {
        rt.count = 0;
    }

    list.visible_textures.clear();
    list.desired_textures.clear();

    const bool culling_enabled = list.render_settings.enable_culling;
    const bool lighting_enabled = list.render_settings.enable_lights;
    const bool decals_enabled = list.render_settings.enable_decals;
    const bool shadows_enabled = (list.render_settings.shadows_quality != eShadowsQuality::Off);
    const bool rt_shadows_enabled = (list.render_settings.shadows_quality != eShadowsQuality::Raytraced);

    if (!lighting_enabled) {
        list.env.sun_col = {};
    }

    const auto render_mask = Ren::Bitmask<Drawable::eVisibility>(list.draw_cam.render_mask());

    int pipeline_index = int(eFwdPipeline::FrontfaceDraw);
    if (list.render_settings.debug_wireframe) {
        pipeline_index = int(eFwdPipeline::Wireframe);
    }

    if (!list.render_settings.enable_lightmap) {
        pipeline_index += int(eFwdPipeline::_Count);
    }

    const bool deferred_shading =
        list.render_settings.render_mode == eRenderMode::Deferred && !list.render_settings.debug_wireframe;

    litem_to_lsource_.clear();
    ditem_to_decal_.count = 0;
    decals_boxes_.count = 0;

    if (proc_objects_capacity_ < scene.objects.size()) {
        proc_objects_capacity_ = 1;
        while (proc_objects_capacity_ < scene.objects.size()) {
            proc_objects_capacity_ *= 2;
        }
        proc_objects_ = std::make_unique<ProcessedObjData[]>(proc_objects_capacity_);
    }
    memset(proc_objects_.get(), 0xff, sizeof(ProcessedObjData) * scene.objects.size());

    Ren::Frustum rt_z_frustums[ITEM_GRID_RES_Z];
    list.ext_cam.ExtractSubFrustums(1, 1, ITEM_GRID_RES_Z, rt_z_frustums);

    // retrieve pointers to components for fast access
    const auto *transforms = (Transform *)scene.comp_store[CompTransform]->SequentialData();
    const auto *drawables = (Drawable *)scene.comp_store[CompDrawable]->SequentialData();
    const auto *occluders = (Occluder *)scene.comp_store[CompOccluder]->SequentialData();
    const auto *lightmaps = (Lightmap *)scene.comp_store[CompLightmap]->SequentialData();
    const auto *lights_src = (LightSource *)scene.comp_store[CompLightSource]->SequentialData();
    const auto *decals = (Decal *)scene.comp_store[CompDecal]->SequentialData();
    const auto *probes = (LightProbe *)scene.comp_store[CompProbe]->SequentialData();
    const auto *anims = (AnimState *)scene.comp_store[CompAnimState]->SequentialData();
    const auto *acc_structs = (AccStructure *)scene.comp_store[CompAccStructure]->SequentialData();

    const uint32_t skinned_buf_vtx_offset = skinned_buf1_vtx_.offset / 16;

    const Mat4f &view_from_world = list.draw_cam.view_matrix(), &clip_from_view = list.draw_cam.proj_matrix();

    swCullCtxResize(&cull_ctx_, SW_CULL_SUBTILE_X * (ctx_.w() / SW_CULL_SUBTILE_X), 4 * (ctx_.h() / 4),
                    list.draw_cam.near());
    swCullCtxClear(&cull_ctx_);

    const Mat4f view_from_identity = view_from_world * Mat4f{1.0f},
                clip_from_identity = clip_from_view * view_from_identity;

    uint32_t stack[MAX_STACK_SIZE];
    uint32_t stack_size = 0;

    /**********************************************************************************/
    /*                                OCCLUDERS PROCESSING                            */
    /**********************************************************************************/

    const uint64_t occluders_start = Sys::GetTimeUs();

    OPTICK_PUSH("OCCLUDERS");
    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_proc_occluders_str);

    const Mat4f &cull_view_from_world = view_from_world, &cull_clip_from_view = list.draw_cam.proj_matrix();

    if (scene.root_node != 0xffffffff) {
        // Rasterize occluder meshes
        stack[stack_size++] = scene.root_node;

        while (stack_size && culling_enabled) {
            const uint32_t cur = stack[--stack_size] & IndexBits;
            uint32_t skip_frustum_check = (stack[stack_size] & SkipFrustumCheckBit);
            const bvh_node_t *n = &scene.nodes[cur];

            if (!skip_frustum_check) {
                const eVisResult res = list.draw_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                if (res == eVisResult::Invisible) {
                    continue;
                } else if (res == eVisResult::FullyVisible) {
                    skip_frustum_check = SkipFrustumCheckBit;
                }
            }

            if (!n->leaf_node) {
                stack[stack_size++] = skip_frustum_check | n->left_child;
                stack[stack_size++] = skip_frustum_check | n->right_child;
            } else {
                const SceneObject &obj = scene.objects[n->prim_index];

                const uint32_t occluder_flags = CompTransformBit | CompOccluderBit;
                if ((obj.comp_mask & occluder_flags) == occluder_flags) {
                    const Transform &tr = transforms[obj.components[CompTransform]];

                    // Node has slightly enlarged bounds, so we need to check object's bounding box here
                    if (!skip_frustum_check &&
                        list.draw_cam.CheckFrustumVisibility(tr.bbox_min_ws, tr.bbox_max_ws) == eVisResult::Invisible) {
                        continue;
                    }

                    const Mat4f view_from_object = cull_view_from_world * tr.world_from_object,
                                clip_from_object = cull_clip_from_view * view_from_object;

                    const Occluder &occ = occluders[obj.components[CompOccluder]];
                    const Mesh *mesh = occ.mesh.get();

                    SWcull_surf surf[64];
                    int surf_count = 0;

                    for (const auto &grp : mesh->groups()) {
                        SWcull_surf *_surf = &surf[surf_count++];

                        _surf->type = SW_OCCLUDER;
                        _surf->prim_type = SW_TRIANGLES;
                        _surf->index_type = SW_UNSIGNED_INT;
                        _surf->attribs = mesh->attribs();
                        _surf->indices = ((const uint8_t *)mesh->indices() + grp.offset);
                        _surf->stride = 13 * sizeof(float);

                        _surf->count = SWuint(grp.num_indices);
                        _surf->xform = ValuePtr(clip_from_object);
                    }

                    swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                }
            }
        }
    }

    OPTICK_POP();
    __itt_task_end(__g_itt_domain);

    /**********************************************************************************/
    /*                        MESHES/LIGHTS/DECALS/PROBES GATHERING                   */
    /**********************************************************************************/

    OPTICK_PUSH("MAIN GATHERING");

    const uint64_t main_gather_start = Sys::GetTimeUs();

    if (scene.root_node != 0xffffffff) {
        Ren::Frustum z_frustums[ITEM_GRID_RES_Z];
        list.draw_cam.ExtractSubFrustums(1, 1, ITEM_GRID_RES_Z, z_frustums);

        std::future<void> futures[2 * ITEM_GRID_RES_Z];

        const uint64_t CompMask = (CompDrawableBit | CompDecalBit | CompLightSourceBit | CompProbeBit);
        for (auto it = temp_visible_objects_.begin(); it != temp_visible_objects_.end(); ++it) {
            it->val.objects.clear();
            it->val.count = 0;
        }
        for (auto it = scene.object_counts.cbegin(); it != scene.object_counts.cend(); ++it) {
            VisObjStorage &s = temp_visible_objects_[it->key & CompMask];
            s.objects.resize(s.objects.size() + it->val);
        }
        for (int i = 0; i < ITEM_GRID_RES_Z; ++i) {
            futures[i] = threads_.Enqueue(GatherObjectsForZSlice_Job, std::ref(z_frustums[i]), std::ref(scene),
                                          list.draw_cam.world_position(), clip_from_identity, CompMask, &cull_ctx_,
                                          0b00000001, proc_objects_.get(), std::ref(temp_visible_objects_));
        }

        const uint64_t RTCompMask = (CompAccStructureBit | CompLightSourceBit);
        for (auto it = temp_rt_visible_objects_.begin(); it != temp_rt_visible_objects_.end(); ++it) {
            it->val.objects.clear();
            it->val.count = 0;
        }
        for (auto it = scene.object_counts.cbegin(); it != scene.object_counts.cend(); ++it) {
            VisObjStorage &s = temp_rt_visible_objects_[it->key & RTCompMask];
            s.objects.resize(s.objects.size() + it->val);
        }
        for (int i = 0; i < ITEM_GRID_RES_Z; ++i) {
            futures[ITEM_GRID_RES_Z + i] = threads_.Enqueue(
                GatherObjectsForZSlice_Job, std::ref(rt_z_frustums[i]), std::ref(scene), list.draw_cam.world_position(),
                Ren::Mat4f{}, RTCompMask, nullptr, 0b00000010, proc_objects_.get(), std::ref(temp_rt_visible_objects_));
        }

        /////

        for (int i = 0; i < ITEM_GRID_RES_Z; ++i) {
            futures[i].wait();
        }

        for (auto it = temp_visible_objects_.begin(); it != temp_visible_objects_.end(); ++it) {
            if (it->key & CompDrawableBit) {
                for (const VisObj i : Ren::Span<VisObj>{it->val.objects.data(), it->val.count.load()}) {
                    const SceneObject &obj = scene.objects[i.index];

                    const Transform &tr = transforms[obj.components[CompTransform]];
                    const Drawable &dr = drawables[obj.components[CompDrawable]];
                    if (!bool(dr.vis_mask & render_mask)) {
                        continue;
                    }
                    const Mesh *mesh = dr.mesh.get();

                    const float cam_dist = Distance(cam.world_position(), 0.5f * (tr.bbox_min_ws + tr.bbox_max_ws));
                    const auto cam_dist_u8 = (uint8_t)std::min(255 * cam_dist / 500.0f, 255.0f);
                    const uint16_t cam_dist_u16 = uint16_t(0xffffu * (cam_dist / 500.0f));

                    uint32_t base_vertex = mesh->attribs_buf1().sub.offset / 16;

                    if (obj.comp_mask & CompAnimStateBit) {
                        const AnimState &as = anims[obj.components[CompAnimState]];
                        base_vertex = __push_skeletal_mesh(skinned_buf_vtx_offset, as, mesh, list);
                    }
                    proc_objects_[i.index].base_vertex = base_vertex;

                    __push_ellipsoids(dr, tr.world_from_object, list);

                    const uint32_t indices_start = mesh->indices_buf().sub.offset;
                    const Ren::Span<const Ren::TriGroup> groups = mesh->groups();
                    for (int j = 0; j < int(groups.size()); ++j) {
                        const Ren::TriGroup &grp = groups[j];

                        const MaterialRef &front_mat =
                            (j >= dr.material_override.size()) ? grp.front_mat : dr.material_override[j].first;
                        const Bitmask<eMatFlags> front_mat_flags = front_mat->flags();

                        __record_textures(list.visible_textures, front_mat.get(), (obj.comp_mask & CompAnimStateBit),
                                          cam_dist_u16);

                        if (!deferred_shading || (front_mat_flags & eMatFlags::CustomShaded) ||
                            (front_mat_flags & eMatFlags::AlphaBlend)) {
                            CustomDrawBatch &fwd_batch = list.custom_batches.emplace_back();

                            fwd_batch.alpha_blend_bit = (front_mat_flags & eMatFlags::AlphaBlend) ? 1 : 0;
                            fwd_batch.pipe_id = front_mat->pipelines[pipeline_index].index();
                            fwd_batch.alpha_test_bit = (front_mat_flags & eMatFlags::AlphaTest) ? 1 : 0;
                            fwd_batch.depth_write_bit = (front_mat_flags & eMatFlags::DepthWrite) ? 1 : 0;
                            fwd_batch.two_sided_bit = (front_mat_flags & eMatFlags::TwoSided) ? 1 : 0;
                            if (!ctx_.capabilities.bindless_texture) {
                                fwd_batch.mat_id = uint32_t(front_mat.index());
                            } else {
                                fwd_batch.mat_id = 0;
                            }
                            fwd_batch.cam_dist = (front_mat_flags & eMatFlags::AlphaBlend) ? uint32_t(cam_dist_u8) : 0;
                            fwd_batch.indices_offset = (indices_start + grp.offset) / sizeof(uint32_t);
                            fwd_batch.base_vertex = base_vertex;
                            fwd_batch.indices_count = grp.num_indices;
                            fwd_batch.instance_index = i.index;
                            fwd_batch.material_index = int32_t(front_mat.index());
                            fwd_batch.instance_count = 1;
                        }

                        if (!(front_mat_flags & eMatFlags::AlphaBlend) ||
                            ((front_mat_flags & eMatFlags::AlphaBlend) && (front_mat_flags & eMatFlags::AlphaTest))) {
                            BasicDrawBatch &base_batch = list.basic_batches.emplace_back();

                            base_batch.type_bits = BasicDrawBatch::TypeSimple;
                            if (obj.comp_mask & CompAnimStateBit) {
                                base_batch.type_bits = BasicDrawBatch::TypeSkinned;
                            } else if (obj.comp_mask & CompVegStateBit) {
                                base_batch.type_bits = BasicDrawBatch::TypeVege;
                            }

                            base_batch.alpha_test_bit = (front_mat_flags & eMatFlags::AlphaTest) ? 1 : 0;
                            base_batch.moving_bit = (obj.last_change_mask & CompTransformBit) ? 1 : 0;
                            base_batch.two_sided_bit = (front_mat_flags & eMatFlags::TwoSided) ? 1 : 0;
                            base_batch.custom_shaded = (front_mat_flags & eMatFlags::CustomShaded) ? 1 : 0;
                            base_batch.indices_offset = (indices_start + grp.offset) / sizeof(uint32_t);
                            base_batch.base_vertex = base_vertex;
                            base_batch.indices_count = grp.num_indices;
                            base_batch.instance_index = i.index;
                            base_batch.material_index = int32_t(front_mat.index());
                            base_batch.instance_count = 1;

                            const MaterialRef &back_mat =
                                (j >= dr.material_override.size()) ? grp.back_mat : dr.material_override[j].second;
                            if (front_mat != back_mat) {
                                __record_textures(list.visible_textures, back_mat.get(),
                                                  (obj.comp_mask & CompAnimStateBit), cam_dist_u16);

                                BasicDrawBatch &back_batch = list.basic_batches.emplace_back(base_batch);
                                back_batch.back_sided_bit = 1;
                                back_batch.alpha_test_bit = (back_mat->flags() & eMatFlags::AlphaTest) ? 1 : 0;
                                back_batch.material_index = int32_t(back_mat.index());
                            }
                        }
                    }
                }
            }
            if (lighting_enabled && (it->key & CompLightSourceBit) && litem_to_lsource_.size() < MAX_LIGHTS_TOTAL) {
                for (const VisObj i : Ren::Span<VisObj>{it->val.objects.data(), it->val.count.load()}) {
                    const SceneObject &obj = scene.objects[i.index];

                    const Transform &tr = transforms[obj.components[CompTransform]];
                    const LightSource &light = lights_src[obj.components[CompLightSource]];

                    if (light.power > 0.0f) {
                        auto pos = Vec4f{light.offset[0], light.offset[1], light.offset[2], 1.0f};
                        pos = tr.world_from_object * pos;
                        pos /= pos[3];

                        auto dir = Vec4f{-light.dir[0], -light.dir[1], -light.dir[2], 0.0f};
                        dir = tr.world_from_object * dir;

                        const auto u = tr.world_from_object * Vec4f{0.5f * light.width, 0.0f, 0.0f, 0.0f};
                        const auto v = tr.world_from_object * Vec4f{0.0f, 0.0f, 0.5f * light.height, 0.0f};

                        litem_to_lsource_.emplace_back(obj.components[CompLightSource]);
                        proc_objects_[i.index].li_index = int32_t(list.lights.size());
                        LightItem &ls = list.lights.emplace_back();

                        ls.col[0] = light.power * light.col[0] / light.area;
                        ls.col[1] = light.power * light.col[1] / light.area;
                        ls.col[2] = light.power * light.col[2] / light.area;
                        ls.type = int(light.type);
                        memcpy(ls.pos, &pos[0], 3 * sizeof(float));
                        ls.radius = light.radius;
                        memcpy(ls.dir, &dir[0], 3 * sizeof(float));
                        ls.spot = light.spot_angle;
                        memcpy(ls.u, ValuePtr(u), 3 * sizeof(float));
                        ls.shadowreg_index = -1;
                        memcpy(ls.v, ValuePtr(v), 3 * sizeof(float));
                        ls.blend = light.spot_blend * light.spot_blend;
                    }
                }
            }
            if (decals_enabled && (it->key & CompDecalBit) && ditem_to_decal_.count < MAX_DECALS_TOTAL) {
                for (const VisObj i : Ren::Span<VisObj>{it->val.objects.data(), it->val.count.load()}) {
                    const SceneObject &obj = scene.objects[i.index];

                    const Transform &tr = transforms[obj.components[CompTransform]];
                    const Decal &decal = decals[obj.components[CompDecal]];

                    const Mat4f &view_from_object = decal.view, &clip_from_view = decal.proj;

                    const Mat4f view_from_world = view_from_object * tr.object_from_world,
                                clip_from_world = clip_from_view * view_from_world;

                    const Mat4f world_from_clip = Inverse(clip_from_world);

                    Vec3f bbox_min = Vec3f{std::numeric_limits<float>::max()},
                          bbox_max = Vec3f{std::numeric_limits<float>::lowest()};

                    for (int k = 0; k < 8; k++) {
                        Vec4f p = world_from_clip * ClipFrustumPoints[k];
                        p /= p[3];

                        bbox_min = Min(bbox_min, Vec3f{p});
                        bbox_max = Max(bbox_max, Vec3f{p});
                    }

                    ditem_to_decal_.data[ditem_to_decal_.count++] = &decal;
                    decals_boxes_.data[decals_boxes_.count++] = {bbox_min, bbox_max};

                    const Mat4f clip_from_world_transposed = Transpose(clip_from_world);

                    DecalItem &de = list.decals.emplace_back();
                    memcpy(&de.mat[0][0], &clip_from_world_transposed[0][0], 12 * sizeof(float));
                    memcpy(&de.mask[0], &decal.mask[0], 4 * sizeof(float));
                    memcpy(&de.diff[0], &decal.diff[0], 4 * sizeof(float));
                    memcpy(&de.norm[0], &decal.norm[0], 4 * sizeof(float));
                    memcpy(&de.spec[0], &decal.spec[0], 4 * sizeof(float));
                }
            }
            if ((it->key & CompProbeBit) && list.probes.size() < MAX_PROBES_TOTAL) {
                for (const VisObj i : Ren::Span<VisObj>{it->val.objects.data(), it->val.count.load()}) {
                    const SceneObject &obj = scene.objects[i.index];

                    const Transform &tr = transforms[obj.components[CompTransform]];
                    const LightProbe &probe = probes[obj.components[CompProbe]];

                    auto pos = Vec4f{probe.offset[0], probe.offset[1], probe.offset[2], 1.0f};
                    pos = tr.world_from_object * pos;
                    pos /= pos[3];

                    ProbeItem &pr = list.probes.emplace_back();
                    pr.layer = float(probe.layer_index);
                    pr.radius = probe.radius;
                    memcpy(&pr.position[0], &pos[0], 3 * sizeof(float));
                    for (int k = 0; k < 4; k++) {
                        pr.sh_coeffs[0][k] = probe.sh_coeffs[k][0];
                        pr.sh_coeffs[1][k] = probe.sh_coeffs[k][1];
                        pr.sh_coeffs[2][k] = probe.sh_coeffs[k][2];
                    }
                }
            }
        }

        for (int i = ITEM_GRID_RES_Z; i < 2 * ITEM_GRID_RES_Z; ++i) {
            futures[i].wait();
        }

        VisObjStorage &rt_objects = temp_rt_visible_objects_[CompAccStructureBit];
        if (rt_objects.count > MAX_RT_OBJ_INSTANCES) {
            OPTICK_EVENT("SORT RT INSTANCES");

            const Vec3f &cam_pos = list.draw_cam.world_position();
            std::partial_sort(rt_objects.objects.data(), rt_objects.objects.data() + MAX_RT_OBJ_INSTANCES,
                              rt_objects.objects.data() + rt_objects.count.load(),
                              [&](const VisObj lhs, const VisObj rhs) {
                                  const SceneObject &lhs_obj = scene.objects[lhs.index];
                                  const SceneObject &rhs_obj = scene.objects[rhs.index];

                                  return acc_structs[lhs_obj.components[CompAccStructure]].surf_area / lhs.dist2 >
                                         acc_structs[rhs_obj.components[CompAccStructure]].surf_area / rhs.dist2;
                              });
        }

        rt_objects.objects.resize(std::min(rt_objects.count.load(), MAX_RT_OBJ_INSTANCES));

        for (const VisObj i : Ren::Span<VisObj>{rt_objects.objects.data(), rt_objects.count.load()}) {
            const SceneObject &obj = scene.objects[i.index];

            const Transform &tr = transforms[obj.components[CompTransform]];
            const AccStructure &acc = acc_structs[obj.components[CompAccStructure]];

            RTObjInstance &new_instance = list.rt_obj_instances[0].data[list.rt_obj_instances[0].count++];
            memcpy(new_instance.xform, ValuePtr(Transpose(tr.world_from_object)), 12 * sizeof(float));
            memcpy(new_instance.bbox_min_ws, ValuePtr(tr.bbox_min_ws), 3 * sizeof(float));
            new_instance.geo_index = acc.mesh->blas->geo_index;
            new_instance.geo_count = acc.mesh->blas->geo_count;
            new_instance.mask = uint8_t(acc.vis_mask);
            memcpy(new_instance.bbox_max_ws, ValuePtr(tr.bbox_max_ws), 3 * sizeof(float));
            new_instance.blas_ref = acc.mesh->blas.get();
        }

        if (lighting_enabled) {
            VisObjStorage &rt_lights = temp_rt_visible_objects_[CompLightSourceBit];
            for (const VisObj i : Ren::Span<VisObj>{rt_lights.objects.data(), rt_lights.count.load()}) {
                const SceneObject &obj = scene.objects[i.index];

                const Transform &tr = transforms[obj.components[CompTransform]];
                const LightSource &light = lights_src[obj.components[CompLightSource]];

                if (proc_objects_[i.index].li_index != -1) {
                    continue;
                }

                if (light.power > 0.0f) {
                    auto pos = Vec4f{light.offset[0], light.offset[1], light.offset[2], 1.0f};
                    pos = tr.world_from_object * pos;
                    pos /= pos[3];

                    auto dir = Vec4f{-light.dir[0], -light.dir[1], -light.dir[2], 0.0f};
                    dir = tr.world_from_object * dir;

                    const auto u = tr.world_from_object * Vec4f{0.5f * light.width, 0.0f, 0.0f, 0.0f};
                    const auto v = tr.world_from_object * Vec4f{0.0f, 0.0f, 0.5f * light.height, 0.0f};

                    litem_to_lsource_.emplace_back(obj.components[CompLightSource]);
                    LightItem &ls = list.lights.emplace_back();

                    ls.col[0] = light.power * light.col[0] / light.area;
                    ls.col[1] = light.power * light.col[1] / light.area;
                    ls.col[2] = light.power * light.col[2] / light.area;
                    ls.type = int(light.type);
                    memcpy(ls.pos, &pos[0], 3 * sizeof(float));
                    ls.radius = light.radius;
                    memcpy(ls.dir, &dir[0], 3 * sizeof(float));
                    ls.spot = light.spot_angle;
                    memcpy(ls.u, ValuePtr(u), 3 * sizeof(float));
                    ls.shadowreg_index = -1;
                    memcpy(ls.v, ValuePtr(v), 3 * sizeof(float));
                    ls.blend = light.spot_blend * light.spot_blend;
                }
            }
        }
    }

    OPTICK_POP();

    /**********************************************************************************/
    /*                                SHADOWMAP GATHERING                             */
    /**********************************************************************************/

    OPTICK_PUSH("SHADOW GATHERING");
    const uint64_t shadow_gather_start = Sys::GetTimeUs();

    if (lighting_enabled && scene.root_node != 0xffffffff && shadows_enabled && Length2(list.env.sun_dir) > 0.9f &&
        Length2(list.env.sun_col) > FLT_EPSILON) {
        // Reserve space for sun shadow
        int sun_shadow_pos[2] = {0, 0};
        int sun_shadow_res[2];
        if (shadow_splitter_.FindNode(sun_shadow_pos, sun_shadow_res) == -1 || sun_shadow_res[0] != SUN_SHADOW_RES ||
            sun_shadow_res[1] != SUN_SHADOW_RES) {
            shadow_splitter_.Clear();

            sun_shadow_res[0] = SUN_SHADOW_RES;
            sun_shadow_res[1] = SUN_SHADOW_RES;

            const int id = shadow_splitter_.Allocate(sun_shadow_res, sun_shadow_pos);
            assert(id != -1 && sun_shadow_pos[0] == 0 && sun_shadow_pos[1] == 0);
        }

        // Planes, that define shadow map splits
        const float far_planes[] = {float(SHADOWMAP_CASCADE0_DIST), float(SHADOWMAP_CASCADE1_DIST),
                                    float(SHADOWMAP_CASCADE2_DIST)};
        const float near_planes[] = {list.draw_cam.near(), 0.9f * far_planes[0], 0.9f * far_planes[1]};

        // Reserved positions for sun shadowmap
        const int OneCascadeRes = SUN_SHADOW_RES / 2;
        const int map_positions[][2] = {{0, 0}, {OneCascadeRes, 0}, {0, OneCascadeRes}, {OneCascadeRes, OneCascadeRes}};

        // Choose up vector for shadow camera
        const Vec3f &light_dir = -list.env.sun_dir;
        auto cam_up = Vec3f{1.0f, 0.0f, 0.0f};
        if (fabsf(light_dir[1]) < 0.999f) {
            cam_up = Vec3f{0.0f, 1.0f, 0.0f};
        }

        // Calculate side vector of shadow camera
        const Vec3f cam_side = Normalize(Cross(light_dir, cam_up));
        cam_up = Cross(cam_side, light_dir);

        const Vec3f scene_dims = scene.nodes[scene.root_node].bbox_max - scene.nodes[scene.root_node].bbox_min;
        const float max_dist = Distance(scene.nodes[0].bbox_min, scene.nodes[0].bbox_max);

        const Vec3f view_dir = list.draw_cam.view_dir();

        // Gather drawables for each cascade
        for (int casc = 0; casc < 4; ++casc) {
            Mat4f tmp_cam_world_from_clip;
            Vec3f bounding_center;
            float bounding_radius;
            if (casc == 3) {
                bounding_center = 0.5f * (scene.nodes[0].bbox_min + scene.nodes[0].bbox_max);
                bounding_radius = 0.5f * max_dist;
            } else {
                Camera temp_cam = list.draw_cam;
                temp_cam.Perspective(list.draw_cam.angle(), list.draw_cam.aspect(), near_planes[casc],
                                     far_planes[casc]);
                temp_cam.UpdatePlanes();

                const Mat4f &tmp_cam_view_from_world = temp_cam.view_matrix(),
                            &tmp_cam_clip_from_view = temp_cam.proj_matrix();

                const Mat4f tmp_cam_clip_from_world = tmp_cam_clip_from_view * tmp_cam_view_from_world;
                tmp_cam_world_from_clip = Inverse(tmp_cam_clip_from_world);

                bounding_radius = temp_cam.GetBoundingSphere(bounding_center);
                if (casc == 0 && bounding_radius > 0.5f * max_dist) {
                    // scene is very small, just use the whole bounds
                    bounding_center = 0.5f * (scene.nodes[0].bbox_min + scene.nodes[0].bbox_max);
                    bounding_radius = 0.5f * max_dist;
                }
            }
            float object_dim_thres = 0.0f;

            Vec3f cam_target = bounding_center;

            { // Snap camera movement to shadow map pixels
                const float move_step = (2 * bounding_radius) / (0.5f * SUN_SHADOW_RES);
                //                      |_shadow map extent_|   |_res of one cascade_|

                // Project target on shadow cam view matrix
                float _dot_f = Dot(cam_target, light_dir), _dot_s = Dot(cam_target, cam_side),
                      _dot_u = Dot(cam_target, cam_up);

                // Snap coordinates to pixels
                _dot_f = std::round(_dot_f / move_step) * move_step;
                _dot_s = std::round(_dot_s / move_step) * move_step;
                _dot_u = std::round(_dot_u / move_step) * move_step;

                // Update target coordinates in world space
                cam_target = _dot_f * light_dir + _dot_s * cam_side + _dot_u * cam_up;

                // Set object size requirenment
                object_dim_thres = 2.0f * move_step;
            }

            const Vec3f cam_support1 = FindSupport(scene.nodes[0].bbox_min, scene.nodes[0].bbox_max, light_dir);
            const Vec3f cam_support2 = FindSupport(scene.nodes[0].bbox_min, scene.nodes[0].bbox_max, -light_dir);
            const Vec3f cam_center = cam_target + fabsf(Dot(cam_support1 - cam_target, light_dir)) * light_dir;
            const float cam_extents = fabsf(Dot(cam_center, light_dir) - Dot(cam_support2, light_dir));

            Camera shadow_cam;
            shadow_cam.SetupView(cam_center, cam_target, cam_up);
            shadow_cam.Orthographic(-bounding_radius, bounding_radius, bounding_radius, -bounding_radius, 0.0f,
                                    2 * bounding_radius);
            shadow_cam.UpdatePlanes();

            const Mat4f sh_clip_from_world = shadow_cam.proj_matrix() * shadow_cam.view_matrix();

            ShadowList &sh_list = list.shadow_lists.data[list.shadow_lists.count++];

            sh_list.shadow_map_pos[0] = map_positions[casc][0];
            sh_list.shadow_map_pos[1] = map_positions[casc][1];
            sh_list.shadow_map_size[0] = OneCascadeRes;
            sh_list.shadow_map_size[1] = OneCascadeRes;
            sh_list.shadow_batch_start = uint32_t(list.shadow_batches.size());
            sh_list.shadow_batch_count = 0;
            sh_list.cam_near = shadow_cam.near();
            sh_list.cam_far = shadow_cam.far();
            sh_list.bias[0] = scene.env.sun_shadow_bias[0];
            sh_list.bias[1] = scene.env.sun_shadow_bias[1];

            Frustum sh_clip_frustum;
            if (casc == 3) {
                sh_clip_frustum = shadow_cam.frustum();

                sh_list.scissor_test_pos[0] = sh_list.shadow_map_pos[0];
                sh_list.scissor_test_pos[1] = sh_list.shadow_map_pos[1];
                sh_list.scissor_test_size[0] = sh_list.shadow_map_size[0];
                sh_list.scissor_test_size[1] = sh_list.shadow_map_size[1];
            } else {
                Vec4f frustum_points[8] = {REN_UNINITIALIZE_X8(Vec4f)};

                for (int k = 0; k < 8; k++) {
                    frustum_points[k] = tmp_cam_world_from_clip * ClipFrustumPoints[k];
                    frustum_points[k] /= frustum_points[k][3];
                }

                Vec2f fr_points_proj[8] = {REN_UNINITIALIZE_X8(Vec2f)};

                for (int k = 0; k < 8; k++) {
                    Vec4f projected_p = sh_clip_from_world * frustum_points[k];
                    projected_p /= projected_p[3];

                    fr_points_proj[k] = Vec2f{projected_p};
                }

                Vec2i frustum_edges[] = {Vec2i{0, 1}, Vec2i{1, 2}, Vec2i{2, 3}, Vec2i{3, 0}, Vec2i{4, 5}, Vec2i{5, 6},
                                         Vec2i{6, 7}, Vec2i{7, 4}, Vec2i{0, 4}, Vec2i{1, 5}, Vec2i{2, 6}, Vec2i{3, 7}};

                int silhouette_edges[12], silhouette_edges_count = 0;

                for (int i = 0; i < 12; i++) {
                    const int k1 = frustum_edges[i][0], k2 = frustum_edges[i][1];

                    int last_sign = 0;
                    bool is_silhouette = true;

                    for (int k = 0; k < 8; k++) {
                        if (k == k1 || k == k2) {
                            continue;
                        }

                        const float d = (fr_points_proj[k][0] - fr_points_proj[k1][0]) *
                                            (fr_points_proj[k2][1] - fr_points_proj[k1][1]) -
                                        (fr_points_proj[k][1] - fr_points_proj[k1][1]) *
                                            (fr_points_proj[k2][0] - fr_points_proj[k1][0]);

                        const int sign = (d > 0.0f) ? 1 : -1;

                        if (last_sign && sign != last_sign) {
                            is_silhouette = false;
                            break;
                        }

                        last_sign = sign;
                    }

                    if (is_silhouette) {
                        silhouette_edges[silhouette_edges_count++] = i;
                        if (last_sign == 1) {
                            std::swap(frustum_edges[i][0], frustum_edges[i][1]);
                        }

                        const float x_diff0 = (fr_points_proj[k1][0] - fr_points_proj[k2][0]);
                        const bool is_vertical0 = std::abs(x_diff0) < FLT_EPSILON;
                        const float slope0 =
                                        is_vertical0 ? 0.0f : (fr_points_proj[k1][1] - fr_points_proj[k2][1]) / x_diff0,
                                    b0 = is_vertical0 ? fr_points_proj[k1][0]
                                                      : (fr_points_proj[k1][1] - slope0 * fr_points_proj[k1][0]);

                        // Check if it is a duplicate
                        for (int k = 0; k < silhouette_edges_count - 1; k++) {
                            const int j = silhouette_edges[k];

                            const float x_diff1 =
                                (fr_points_proj[frustum_edges[j][0]][0] - fr_points_proj[frustum_edges[j][1]][0]);
                            const bool is_vertical1 = std::abs(x_diff1) < FLT_EPSILON;
                            const float slope1 = is_vertical1 ? 0.0f
                                                              : (fr_points_proj[frustum_edges[j][0]][1] -
                                                                 fr_points_proj[frustum_edges[j][1]][1]) /
                                                                    x_diff1,
                                        b1 = is_vertical1 ? fr_points_proj[frustum_edges[j][0]][0]
                                                          : fr_points_proj[frustum_edges[j][0]][1] -
                                                                slope1 * fr_points_proj[frustum_edges[j][0]][0];

                            if (is_vertical1 == is_vertical0 && std::abs(slope1 - slope0) < 0.001f &&
                                std::abs(b1 - b0) < 0.001f) {
                                silhouette_edges_count--;
                                break;
                            }
                        }
                    }
                }

                assert(silhouette_edges_count <= 6);

                sh_clip_frustum.planes_count = silhouette_edges_count;
                sh_list.view_frustum_outline_count = 2 * silhouette_edges_count;

                auto scissor_min = Vec2i{SHADOWMAP_WIDTH}, scissor_max = Vec2i{0};

                for (int i = 0; i < silhouette_edges_count; i++) {
                    const Vec2i edge = frustum_edges[silhouette_edges[i]];

                    const auto p1 = Vec3f{frustum_points[edge[0]]}, p2 = Vec3f{frustum_points[edge[1]]};

                    // Extrude edge in direction of light
                    const Vec3f p3 = p2 + light_dir;

                    // Construct clipping plane
                    sh_clip_frustum.planes[i] = Plane{p1, p2, p3};

                    // Store projected points for debugging
                    sh_list.view_frustum_outline[2 * i + 0] = fr_points_proj[edge[0]];
                    sh_list.view_frustum_outline[2 * i + 1] = fr_points_proj[edge[1]];

                    // Find region for scissor test
                    const auto p1i = Vec2i{
                        sh_list.shadow_map_pos[0] + int((0.5f * sh_list.view_frustum_outline[2 * i + 0][0] + 0.5f) *
                                                        float(sh_list.shadow_map_size[0])),
                        sh_list.shadow_map_pos[1] + int((0.5f * sh_list.view_frustum_outline[2 * i + 0][1] + 0.5f) *
                                                        float(sh_list.shadow_map_size[1]))};

                    const auto p2i = Vec2i{
                        sh_list.shadow_map_pos[0] + int((0.5f * sh_list.view_frustum_outline[2 * i + 1][0] + 0.5f) *
                                                        float(sh_list.shadow_map_size[0])),
                        sh_list.shadow_map_pos[1] + int((0.5f * sh_list.view_frustum_outline[2 * i + 1][1] + 0.5f) *
                                                        float(sh_list.shadow_map_size[1]))};

                    const auto scissor_margin = Vec2i{2}; // shadow uses 5x5 filter

                    scissor_min = Min(scissor_min, Min(p1i - scissor_margin, p2i - scissor_margin));
                    scissor_max = Max(scissor_max, Max(p1i + scissor_margin, p2i + scissor_margin));
                }

                scissor_min = Max(scissor_min, Vec2i{0});
                scissor_max = Min(
                    scissor_max, Vec2i{map_positions[casc][0] + OneCascadeRes, map_positions[casc][1] + OneCascadeRes});

                sh_list.scissor_test_pos[0] = scissor_min[0];
                sh_list.scissor_test_pos[1] = scissor_min[1];
                sh_list.scissor_test_size[0] = scissor_max[0] - scissor_min[0];
                sh_list.scissor_test_size[1] = scissor_max[1] - scissor_min[1];

                // add near and far planes
                sh_clip_frustum.planes[sh_clip_frustum.planes_count++] = shadow_cam.frustum_plane(eCamPlane::Near);
                sh_clip_frustum.planes[sh_clip_frustum.planes_count++] = shadow_cam.frustum_plane(eCamPlane::Far);
            }

            ShadowMapRegion &reg = list.shadow_regions.data[list.shadow_regions.count++];

            reg.transform = Vec4f{float(sh_list.shadow_map_pos[0]) / SHADOWMAP_WIDTH,
                                  float(sh_list.shadow_map_pos[1]) / SHADOWMAP_HEIGHT,
                                  float(sh_list.shadow_map_size[0]) / SHADOWMAP_WIDTH,
                                  float(sh_list.shadow_map_size[1]) / SHADOWMAP_HEIGHT};

            const float cached_dist = Distance(list.draw_cam.world_position(), sun_shadow_cache_[casc].view_pos),
                        cached_dir_dist = Distance(view_dir, sun_shadow_cache_[casc].view_dir);

            // discard cached cascade if view change was significant
            sun_shadow_cache_[casc].valid &= (cached_dist < 1.0f && cached_dir_dist < 0.1f);

            const uint8_t pattern_bit = (1u << uint8_t(frame_index_ % 8));
            bool should_update = rt_shadows_enabled || (pattern_bit & SunShadowUpdatePattern[casc]) != 0;

            if (EnableSunCulling && casc != 0 && casc != 3 && should_update) {
                // Check if cascade is visible to main camera

                const float p_min[2] = {-1.0f, -1.0f};
                const float p_max[2] = {1.0f, 1.0f};
                const float dist = near_planes[casc];

                should_update &= swCullCtxTestRect(&cull_ctx_, p_min, p_max, dist) != 0;
            }

            if (sun_shadow_cache_[casc].valid && !should_update) {
                // keep this cascade unchanged
                reg.clip_from_world = sun_shadow_cache_[casc].clip_from_world;
                continue;
            } else {
                reg.clip_from_world = sh_clip_from_world;

                sun_shadow_cache_[casc].valid = true;
                sun_shadow_cache_[casc].view_pos = list.draw_cam.world_position();
                sun_shadow_cache_[casc].view_dir = view_dir;
                sun_shadow_cache_[casc].clip_from_world = sh_clip_from_world;
            }

            const int ZSliceCount = (casc + 1) * 6;
            Ren::Frustum z_frustums[24];

            const float z_beg = -sh_clip_frustum.planes[int(eCamPlane::Near)].d;
            const float z_end = sh_clip_frustum.planes[int(eCamPlane::Far)].d;

            for (int i = 0; i < ZSliceCount; ++i) {
                z_frustums[i] = sh_clip_frustum;

                const float k_near = float(i) / ZSliceCount;
                const float k_far = float(i + 1) / ZSliceCount;

                z_frustums[i].planes[int(eCamPlane::Near)].d = -(z_beg + k_near * (z_end - z_beg));
                z_frustums[i].planes[int(eCamPlane::Far)].d = z_beg + k_far * (z_end - z_beg);
            }

            std::future<void> futures[24];

            uint64_t CompMask = CompDrawableBit;
            if (rt_shadows_enabled) {
                CompMask |= CompAccStructureBit;
            }
            for (auto it = temp_visible_objects_.begin(); it != temp_visible_objects_.end(); ++it) {
                it->val.objects.clear();
                it->val.count = 0;
            }
            for (auto it = scene.object_counts.cbegin(); it != scene.object_counts.cend(); ++it) {
                VisObjStorage &s = temp_visible_objects_[it->key & CompMask];
                s.objects.resize(s.objects.size() + it->val);
            }
            for (int i = 0; i < ZSliceCount; ++i) {
                futures[i] =
                    threads_.Enqueue(GatherObjectsForZSlice_Job, std::ref(z_frustums[i]), std::ref(scene),
                                     shadow_cam.world_position(), Ren::Mat4f{}, CompMask, nullptr, (0b00000100 << casc),
                                     proc_objects_.get(), std::ref(temp_visible_objects_));
            }

            for (int i = 0; i < ZSliceCount; ++i) {
                futures[i].wait();
            }

            /////

            for (auto it = temp_visible_objects_.begin(); it != temp_visible_objects_.end(); ++it) {
                for (const VisObj i : Ren::Span<VisObj>{it->val.objects.data(), it->val.count.load()}) {
                    const SceneObject &obj = scene.objects[i.index];

                    const Transform &tr = transforms[obj.components[CompTransform]];
                    const Drawable &dr = drawables[obj.components[CompDrawable]];
                    if ((dr.vis_mask & Drawable::eVisibility::Shadow) == 0) {
                        continue;
                    }

                    if ((tr.bbox_max_ws[0] - tr.bbox_min_ws[0]) < object_dim_thres &&
                        (tr.bbox_max_ws[1] - tr.bbox_min_ws[1]) < object_dim_thres &&
                        (tr.bbox_max_ws[2] - tr.bbox_min_ws[2]) < object_dim_thres) {
                        continue;
                    }

                    const Mesh *mesh = dr.mesh.get();

                    if (proc_objects_[i.index].base_vertex == 0xffffffff) {
                        proc_objects_[i.index].base_vertex = mesh->attribs_buf1().sub.offset / 16;

                        if (obj.comp_mask & CompAnimStateBit) {
                            const AnimState &as = anims[obj.components[CompAnimState]];
                            proc_objects_[i.index].base_vertex =
                                __push_skeletal_mesh(skinned_buf_vtx_offset, as, mesh, list);
                        }
                    }

                    if (casc != 3 && proc_objects_[i.index].rt_sh_index == -1 &&
                        (obj.comp_mask & CompAccStructureBit)) {
                        proc_objects_[i.index].rt_sh_index = list.rt_obj_instances[1].count;

                        const AccStructure &acc = acc_structs[obj.components[CompAccStructure]];
                        if (acc.mesh->blas && list.rt_obj_instances[1].count < MAX_RT_OBJ_INSTANCES) {
                            const Mat4f world_from_object_trans = Transpose(tr.world_from_object);

                            RTObjInstance &new_instance =
                                list.rt_obj_instances[1].data[list.rt_obj_instances[1].count++];
                            memcpy(new_instance.xform, ValuePtr(world_from_object_trans), 12 * sizeof(float));
                            memcpy(new_instance.bbox_min_ws, ValuePtr(tr.bbox_min_ws), 3 * sizeof(float));
                            new_instance.geo_index = acc.mesh->blas->geo_index;
                            new_instance.geo_count = acc.mesh->blas->geo_count;
                            new_instance.mask = uint8_t(acc.vis_mask);
                            memcpy(new_instance.bbox_max_ws, ValuePtr(tr.bbox_max_ws), 3 * sizeof(float));
                            new_instance.blas_ref = acc.mesh->blas.get();
                        }
                    }

                    const Ren::Span<const Ren::TriGroup> groups = mesh->groups();
                    for (int j = 0; j < int(groups.size()); ++j) {
                        const Ren::TriGroup &grp = groups[j];

                        const MaterialRef &front_mat =
                            (j >= dr.material_override.size()) ? grp.front_mat : dr.material_override[j].first;
                        const Bitmask<eMatFlags> front_mat_flags = front_mat->flags();

                        if ((front_mat_flags & eMatFlags::AlphaBlend) == 0) {
                            if ((front_mat_flags & eMatFlags::AlphaTest) && front_mat->textures.size() > 4 &&
                                front_mat->textures[4]) {
                                // assume only the fourth texture gives transparency
                                __record_texture(list.visible_textures, front_mat->textures[4], 0, 0xffffu);
                            }

                            BasicDrawBatch &batch = list.shadow_batches.emplace_back();

                            batch.type_bits = BasicDrawBatch::TypeSimple;
                            // we do not care if it is skinned
                            if (obj.comp_mask & CompVegStateBit) {
                                batch.type_bits = BasicDrawBatch::TypeVege;
                            }

                            const MaterialRef &back_mat =
                                (j >= dr.material_override.size()) ? grp.back_mat : dr.material_override[j].second;

                            const bool simple_twosided = (front_mat_flags & eMatFlags::TwoSided) ||
                                                         (!(front_mat_flags & eMatFlags::AlphaTest) &&
                                                          !(back_mat->flags() & eMatFlags::AlphaTest));

                            batch.alpha_test_bit = (front_mat_flags & eMatFlags::AlphaTest) ? 1 : 0;
                            batch.moving_bit = 0;
                            batch.two_sided_bit = simple_twosided ? 1 : 0;
                            batch.indices_offset = (mesh->indices_buf().sub.offset + grp.offset) / sizeof(uint32_t);
                            batch.base_vertex = proc_objects_[i.index].base_vertex;
                            batch.indices_count = grp.num_indices;
                            batch.instance_index = i.index;
                            batch.material_index = ((front_mat_flags & eMatFlags::AlphaTest) ||
                                                    (batch.type_bits == BasicDrawBatch::TypeVege))
                                                       ? int32_t(front_mat.index())
                                                       : 0;
                            batch.instance_count = 1;

                            if (!simple_twosided && front_mat != back_mat) {
                                const Bitmask<eMatFlags> back_mat_flags = back_mat->flags();
                                if ((back_mat_flags & eMatFlags::AlphaTest) && back_mat->textures.size() > 4 &&
                                    back_mat->textures[4]) {
                                    // assume only the fourth texture gives transparency
                                    __record_texture(list.visible_textures, back_mat->textures[4], 0, 0xffffu);
                                }

                                BasicDrawBatch &back_batch = list.shadow_batches.emplace_back(batch);
                                back_batch.back_sided_bit = 1;
                                back_batch.alpha_test_bit = (back_mat_flags & eMatFlags::AlphaTest) ? 1 : 0;
                                back_batch.material_index = int32_t(back_mat.index());
                            }
                        }
                    }
                }
            }

            sh_list.shadow_batch_count = uint32_t(list.shadow_batches.size()) - sh_list.shadow_batch_start;
        }
    }

    const Vec3f cam_pos = cam.world_position();

    for (int i = 0; i < int(list.lights.size()) && shadows_enabled; i++) {
        LightItem &l = list.lights[i];
        const uint32_t lsource_index = litem_to_lsource_[i];
        const LightSource *ls = &lights_src[lsource_index];

        if (!ls->cast_shadow) {
            continue;
        }

        const auto light_center = Vec3f{l.pos[0], l.pos[1], l.pos[2]};
        const float distance = Distance(light_center, cam_pos);

        const int ShadowResolutions[][2] = {{512, 512}, {256, 256}, {128, 128}, {64, 64}};

        SmallVector<ShadReg *, 6> regions;
        if (ls->type == eLightType::Sphere || ls->type == eLightType::Line) {
            regions.resize(6, nullptr);
        } else if (ls->type == eLightType::Rect || ls->type == eLightType::Disk) {
            regions.resize(5, nullptr);
        } else {
            regions.resize(1, nullptr);
        }
        bool all_initialized = true;

        // Find already allocated regions
        for (int r = 0; r < int(regions.size()); ++r) {
            // choose resolution based on distance
            int res_index = std::min(int(distance * 0.02f), 4);

            for (int j = 0; j < int(allocated_shadow_regions_.count); j++) {
                ShadReg &reg = allocated_shadow_regions_.data[j];
                if (reg.ls_index == lsource_index && reg.reg_index == r) {
                    if (reg.size[0] != ShadowResolutions[res_index][0] ||
                        reg.size[1] != ShadowResolutions[res_index][1]) {
                        // free and reallocate region
                        shadow_splitter_.Free(reg.pos);
                        reg = allocated_shadow_regions_.data[--allocated_shadow_regions_.count];
                    } else {
                        regions[r] = &reg;
                    }
                    break;
                }
            }

            // Try to allocate best resolution possible
            // TODO: make all regions use the same resolution
            for (; res_index < 4 && !regions[r]; res_index++) {
                int alloc_res[2] = {ShadowResolutions[res_index][0], ShadowResolutions[res_index][1]};
                if (r < 4 && (ls->type == eLightType::Rect || ls->type == eLightType::Disk)) {
                    // allocate half of a region
                    alloc_res[1] /= 2;
                }
                int pos[2];
                int node = shadow_splitter_.Allocate(alloc_res, pos);
                if (node == -1 && allocated_shadow_regions_.count) {
                    ShadReg *oldest = &allocated_shadow_regions_.data[0];
                    for (int j = 0; j < int(allocated_shadow_regions_.count); j++) {
                        if (allocated_shadow_regions_.data[j].last_visible < oldest->last_visible) {
                            oldest = &allocated_shadow_regions_.data[j];
                        }
                    }
                    if ((scene.update_counter - oldest->last_visible) > 10) {
                        // kick out one of old cached regions
                        shadow_splitter_.Free(oldest->pos);
                        *oldest = allocated_shadow_regions_.data[--allocated_shadow_regions_.count];
                        // try again to insert
                        node = shadow_splitter_.Allocate(alloc_res, pos);
                    }
                }
                if (node != -1) {
                    regions[r] = &allocated_shadow_regions_.data[allocated_shadow_regions_.count++];
                    regions[r]->ls_index = lsource_index;
                    regions[r]->reg_index = r;
                    regions[r]->pos[0] = pos[0];
                    regions[r]->pos[1] = pos[1];
                    regions[r]->size[0] = ShadowResolutions[res_index][0];
                    regions[r]->size[1] = ShadowResolutions[res_index][1];
                    regions[r]->last_update = regions[r]->last_visible = 0xffffffff;
                }
            }

            all_initialized &= (regions[r] != nullptr);
        }

        if (all_initialized) {
            const auto light_dir = Vec3f{-l.dir[0], -l.dir[1], -l.dir[2]};
            const auto light_side = Vec3f{l.u[0], l.u[1], l.u[2]};
            const auto light_up = Vec3f{l.v[0], l.v[1], l.v[2]};

            float light_angle = 91.0f;
            // if (ls->type == eLightType::Point) {
            //     light_angle = 2.0f * std::acos(l.spot) * 180.0f / Pi<float>();
            // }

            l.shadowreg_index = int(list.shadow_regions.count);

            for (int i = 0; i < int(regions.size()); ++i) {
                ShadReg *region = regions[i];

                Vec3f _light_dir, _light_up;
                if (i == 0 || i == 1) {
                    _light_dir = (i == 0) ? light_up : -light_up;
                    _light_up = -light_dir;
                } else if (i == 2 || i == 3) {
                    _light_dir = (i == 2) ? light_side : -light_side;
                    _light_up = -light_dir;
                } else if (i == 4 || i == 5) {
                    _light_dir = (i == 4) ? light_dir : -light_dir;
                    _light_up = light_up;
                }

                Camera shadow_cam;
                shadow_cam.SetupView(light_center, light_center + _light_dir, _light_up);
                shadow_cam.Perspective(light_angle, 1.0f, ls->cull_offset, ls->cull_radius);
                shadow_cam.UpdatePlanes();

                if (i < 4 && (ls->type == eLightType::Rect || ls->type == eLightType::Disk)) {
                    // cut top half of a frustum
                    Ren::Plane plane;
                    plane.n = _light_up;
                    plane.d = Dot(plane.n, light_center);
                    shadow_cam.set_frustum_plane(Ren::eCamPlane::Top, plane);
                }

                // TODO: Check visibility of shadow frustum itself

                ShadowList &sh_list = list.shadow_lists.data[list.shadow_lists.count++];

                sh_list.shadow_map_pos[0] = sh_list.scissor_test_pos[0] = region->pos[0];
                sh_list.shadow_map_pos[1] = sh_list.scissor_test_pos[1] = region->pos[1];
                sh_list.shadow_map_size[0] = sh_list.scissor_test_size[0] = region->size[0];
                sh_list.shadow_map_size[1] = sh_list.scissor_test_size[1] = region->size[1];
                if (i < 4 && (ls->type == eLightType::Rect || ls->type == eLightType::Disk)) {
                    // set to half of a region
                    sh_list.scissor_test_size[1] /= 2;
                }
                sh_list.shadow_batch_start = uint32_t(list.shadow_batches.size());
                sh_list.shadow_batch_count = 0;
                sh_list.view_frustum_outline_count = 0;
                sh_list.bias[0] = ls->shadow_bias[0];
                sh_list.bias[1] = ls->shadow_bias[1];

                bool light_sees_dynamic_objects = false;

                float near_clip = ls->cull_radius, far_clip = 0.0f;

                const uint32_t skip_check_bit = (1u << 31u);
                const uint32_t index_bits = ~skip_check_bit;

                stack_size = 0;
                stack[stack_size++] = scene.root_node;

                while (stack_size) {
                    const uint32_t cur = stack[--stack_size] & index_bits;
                    uint32_t skip_check = stack[stack_size] & skip_check_bit;
                    const bvh_node_t *n = &scene.nodes[cur];

                    const eVisResult res = shadow_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                    if (res == eVisResult::Invisible) {
                        continue;
                    } else if (res == eVisResult::FullyVisible) {
                        skip_check = skip_check_bit;
                    }

                    if (!n->leaf_node) {
                        stack[stack_size++] = skip_check | n->left_child;
                        stack[stack_size++] = skip_check | n->right_child;
                    } else {
                        const auto &obj = scene.objects[n->prim_index];

                        const uint32_t drawable_flags = CompDrawableBit | CompTransformBit;
                        if ((obj.comp_mask & drawable_flags) == drawable_flags) {
                            const Transform &tr = transforms[obj.components[CompTransform]];

                            if (!skip_check && shadow_cam.CheckFrustumVisibility(tr.bbox_min_ws, tr.bbox_max_ws) ==
                                                   eVisResult::Invisible) {
                                continue;
                            }

                            const Drawable &dr = drawables[obj.components[CompDrawable]];
                            if ((dr.vis_mask & Drawable::eVisibility::Shadow) == 0) {
                                continue;
                            }

                            const float max_extent = 0.5f * Distance(tr.bbox_min, tr.bbox_max);
                            const float dist = Distance(light_center, 0.5f * (tr.bbox_min_ws + tr.bbox_max_ws));
                            near_clip = std::min(near_clip, dist - max_extent);
                            far_clip = std::max(far_clip, dist + max_extent);

                            const Mesh *mesh = dr.mesh.get();

                            Mat4f world_from_object_trans = Transpose(tr.world_from_object);

                            if (proc_objects_[n->prim_index].base_vertex == 0xffffffff) {
                                proc_objects_[n->prim_index].base_vertex = mesh->attribs_buf1().sub.offset / 16;

                                if (obj.comp_mask & CompAnimStateBit) {
                                    const AnimState &as = anims[obj.components[CompAnimState]];
                                    proc_objects_[n->prim_index].base_vertex =
                                        __push_skeletal_mesh(skinned_buf_vtx_offset, as, mesh, list);
                                }
                            }

                            const Ren::Span<const Ren::TriGroup> groups = mesh->groups();
                            for (int j = 0; j < int(groups.size()); ++j) {
                                const Ren::TriGroup &grp = groups[j];

                                const MaterialRef &front_mat =
                                    (j >= dr.material_override.size()) ? grp.front_mat : dr.material_override[j].first;
                                const Bitmask<eMatFlags> front_mat_flags = front_mat->flags();

                                if ((front_mat_flags & eMatFlags::AlphaBlend) == 0) {
                                    if ((front_mat_flags & eMatFlags::AlphaTest) && front_mat->textures.size() > 4 &&
                                        front_mat->textures[4]) {
                                        // assume only the fourth texture gives transparency
                                        __record_texture(list.visible_textures, front_mat->textures[4], 0, 0xffffu);
                                    }

                                    BasicDrawBatch &batch = list.shadow_batches.emplace_back();

                                    batch.type_bits = BasicDrawBatch::TypeSimple;
                                    // we do not care if it is skinned
                                    if (obj.comp_mask & CompVegStateBit) {
                                        batch.type_bits = BasicDrawBatch::TypeVege;
                                    }

                                    const MaterialRef &back_mat = (j >= dr.material_override.size())
                                                                      ? grp.back_mat
                                                                      : dr.material_override[j].second;

                                    const bool simple_twosided = (front_mat_flags & eMatFlags::TwoSided) ||
                                                                 (!(front_mat_flags & eMatFlags::AlphaTest) &&
                                                                  !(back_mat->flags() & eMatFlags::AlphaTest));

                                    batch.alpha_test_bit = (front_mat_flags & eMatFlags::AlphaTest) ? 1 : 0;
                                    batch.moving_bit = 0;
                                    batch.two_sided_bit = simple_twosided ? 1 : 0;
                                    batch.indices_offset =
                                        (mesh->indices_buf().sub.offset + grp.offset) / sizeof(uint32_t);
                                    batch.base_vertex = proc_objects_[n->prim_index].base_vertex;
                                    batch.indices_count = grp.num_indices;
                                    batch.instance_index = n->prim_index;
                                    batch.material_index = ((front_mat_flags & eMatFlags::AlphaTest) ||
                                                            (batch.type_bits == BasicDrawBatch::TypeVege))
                                                               ? uint32_t(front_mat.index())
                                                               : 0;
                                    batch.instance_count = 1;

                                    if (!simple_twosided && front_mat != back_mat) {
                                        const Bitmask<eMatFlags> back_mat_flags = back_mat->flags();
                                        if ((back_mat_flags & eMatFlags::AlphaTest) && back_mat->textures.size() > 4 &&
                                            back_mat->textures[4]) {
                                            // assume only the fourth texture gives transparency
                                            __record_texture(list.visible_textures, back_mat->textures[4], 0, 0xffffu);
                                        }

                                        BasicDrawBatch &back_batch = list.shadow_batches.emplace_back(batch);
                                        back_batch.back_sided_bit = 1;
                                        back_batch.alpha_test_bit = (back_mat_flags & eMatFlags::AlphaTest) ? 1 : 0;
                                        back_batch.material_index = int32_t(back_mat.index());
                                    }
                                }
                            }
                        }

                        if ((obj.last_change_mask & CompTransformBit) || (obj.comp_mask & CompVegStateBit)) {
                            light_sees_dynamic_objects = true;
                        }
                    }
                }

                shadow_cam.Perspective(light_angle, 1.0f, std::max(ls->cull_offset, near_clip),
                                       std::min(ls->cull_radius, far_clip));

                sh_list.cam_near = region->cam_near = shadow_cam.near();
                sh_list.cam_far = region->cam_far = shadow_cam.far();

                ShadowMapRegion &reg = list.shadow_regions.data[list.shadow_regions.count++];
                reg.transform = Vec4f{float(sh_list.shadow_map_pos[0]) / SHADOWMAP_WIDTH,
                                      float(sh_list.shadow_map_pos[1]) / SHADOWMAP_HEIGHT,
                                      float(sh_list.shadow_map_size[0]) / SHADOWMAP_WIDTH,
                                      float(sh_list.shadow_map_size[1]) / SHADOWMAP_HEIGHT};
                reg.clip_from_world = shadow_cam.proj_matrix() * shadow_cam.view_matrix();

                if (!light_sees_dynamic_objects && region->last_update != 0xffffffff &&
                    (scene.update_counter - region->last_update > 2)) {
                    // nothing has changed within the last two frames, discard added batches
                    list.shadow_batches.resize(sh_list.shadow_batch_start);
                    sh_list.shadow_batch_count = 0;
                } else {
                    if (light_sees_dynamic_objects || region->last_update == 0xffffffff) {
                        region->last_update = scene.update_counter;
                    }
                    sh_list.shadow_batch_count = uint32_t(list.shadow_batches.size()) - sh_list.shadow_batch_start;
                }

                region->last_visible = scene.update_counter;
            }
        }
    }

    if (shadows_enabled && list.render_settings.debug_shadows) {
        list.cached_shadow_regions.count = 0;
        for (int i = 0; i < int(allocated_shadow_regions_.count); i++) {
            const ShadReg &r = allocated_shadow_regions_.data[i];
            if (r.last_visible != scene.update_counter) {
                list.cached_shadow_regions.data[list.cached_shadow_regions.count++] = r;
            }
        }
    }

    OPTICK_POP();

    /***********************************************************************************/
    /*                                OPTIMIZING DRAW LISTS                            */
    /***********************************************************************************/

    OPTICK_PUSH("OPTIMIZING");
    const uint64_t drawables_sort_start = Sys::GetTimeUs();

    { // Sort drawables to optimize state switches
        temp_sort_spans_32_[0].resize(list.basic_batches.size());
        temp_sort_spans_32_[1].resize(list.basic_batches.size());
        list.basic_batch_indices.resize(list.basic_batches.size());
        uint32_t spans_count = 0;

        // compress batches into spans with indentical key values (makes sorting faster)
        for (uint32_t start = 0, end = 1; end <= uint32_t(list.basic_batches.size()); end++) {
            if (end == list.basic_batches.size() ||
                (list.basic_batches[start].sort_key != list.basic_batches[end].sort_key)) {
                temp_sort_spans_32_[0][spans_count].key = list.basic_batches[start].sort_key;
                temp_sort_spans_32_[0][spans_count].base = start;
                temp_sort_spans_32_[0][spans_count++].count = end - start;
                start = end;
            }
        }

        RadixSort_LSB<SortSpan32>(temp_sort_spans_32_[0].data(), temp_sort_spans_32_[0].data() + spans_count,
                                  temp_sort_spans_32_[1].data());

        // decompress sorted spans
        size_t counter = 0;
        for (uint32_t i = 0; i < spans_count; i++) {
            for (uint32_t j = 0; j < temp_sort_spans_32_[0][i].count; j++) {
                list.basic_batch_indices[counter++] = temp_sort_spans_32_[0][i].base + j;
            }
        }

        // Merge similar batches
        for (uint32_t start = 0, end = 1; end <= uint32_t(list.basic_batch_indices.size()); end++) {
            if (end == list.basic_batch_indices.size() ||
                list.basic_batches[list.basic_batch_indices[start]].sort_key !=
                    list.basic_batches[list.basic_batch_indices[end]].sort_key) {

                BasicDrawBatch &b1 = list.basic_batches[list.basic_batch_indices[start]];
                b1.instance_start = uint32_t(list.instance_indices.size());
                for (uint32_t j = 0; j < b1.instance_count; ++j) {
                    list.instance_indices.emplace_back(b1.instance_index, b1.material_index);
                }

                for (uint32_t i = start + 1; i < end; i++) {
                    BasicDrawBatch &b2 = list.basic_batches[list.basic_batch_indices[i]];
                    b2.instance_start = uint32_t(list.instance_indices.size());
                    for (uint32_t j = 0; j < b2.instance_count; ++j) {
                        list.instance_indices.emplace_back(b2.instance_index, b2.material_index);
                    }

                    if (b1.base_vertex == b2.base_vertex) {
                        b1.instance_count += b2.instance_count;
                        b2.instance_count = 0;
                    }
                }

                start = end;
            }
        }
    }

    temp_sort_spans_64_[0].resize(uint32_t(list.custom_batches.size()));
    temp_sort_spans_64_[1].resize(uint32_t(list.custom_batches.size()));
    list.custom_batch_indices.resize(uint32_t(list.custom_batches.size()));
    uint32_t spans_count = 0;

    // compress batches into spans with indentical key values (makes sorting faster)
    for (uint32_t start = 0, end = 1; end <= uint32_t(list.custom_batches.size()); end++) {
        if (end == list.custom_batches.size() ||
            (list.custom_batches[start].sort_key != list.custom_batches[end].sort_key)) {
            temp_sort_spans_64_[0][spans_count].key = list.custom_batches[start].sort_key;
            temp_sort_spans_64_[0][spans_count].base = start;
            temp_sort_spans_64_[0][spans_count++].count = end - start;
            start = end;
        }
    }

    RadixSort_LSB<SortSpan64>(temp_sort_spans_64_[0].data(), temp_sort_spans_64_[0].data() + spans_count,
                              temp_sort_spans_64_[1].data());

    // decompress sorted spans
    size_t counter = 0;
    for (uint32_t i = 0; i < spans_count; i++) {
        for (uint32_t j = 0; j < temp_sort_spans_64_[0][i].count; j++) {
            list.custom_batch_indices[counter++] = temp_sort_spans_64_[0][i].base + j;
        }
    }

    list.alpha_blend_start_index = -1;

    // Merge similar batches
    for (uint32_t start = 0, end = 1; end <= uint32_t(list.custom_batch_indices.size()); end++) {
        if (end == list.custom_batch_indices.size() ||
            list.custom_batches[list.custom_batch_indices[start]].sort_key !=
                list.custom_batches[list.custom_batch_indices[end]].sort_key) {

            CustomDrawBatch &b1 = list.custom_batches[list.custom_batch_indices[start]];
            b1.instance_start = uint32_t(list.instance_indices.size());
            for (uint32_t j = 0; j < b1.instance_count; ++j) {
                list.instance_indices.emplace_back(b1.instance_index, b1.material_index);
            }
            for (uint32_t i = start + 1; i < end; i++) {
                CustomDrawBatch &b2 = list.custom_batches[list.custom_batch_indices[i]];
                b2.instance_start = uint32_t(list.instance_indices.size());
                for (uint32_t j = 0; j < b2.instance_count; ++j) {
                    list.instance_indices.emplace_back(b2.instance_index, b2.material_index);
                }

                if (b1.base_vertex == b2.base_vertex) {
                    b1.instance_count += b2.instance_count;
                    b2.instance_count = 0;
                }
            }

            if (list.alpha_blend_start_index == -1 && b1.alpha_blend_bit) {
                list.alpha_blend_start_index = int(start);
            }

            start = end;
        }
    }

    list.shadow_batch_indices.resize(uint32_t(list.shadow_batches.size()));

    uint32_t sh_batch_indices_counter = 0;

    for (int i = 0; i < int(list.shadow_lists.count); i++) {
        ShadowList &sh_list = list.shadow_lists.data[i];

        const uint32_t shadow_batch_end = sh_list.shadow_batch_start + sh_list.shadow_batch_count;

        temp_sort_spans_32_[0].resize(sh_list.shadow_batch_count);
        temp_sort_spans_32_[1].resize(sh_list.shadow_batch_count);
        uint32_t spans_count = 0;

        // compress batches into spans with indentical key values (makes sorting faster)
        for (uint32_t start = sh_list.shadow_batch_start, end = sh_list.shadow_batch_start + 1; end <= shadow_batch_end;
             end++) {
            if (end == shadow_batch_end || (list.shadow_batches[start].sort_key != list.shadow_batches[end].sort_key)) {
                temp_sort_spans_32_[0][spans_count].key = list.shadow_batches[start].sort_key;
                temp_sort_spans_32_[0][spans_count].base = start;
                temp_sort_spans_32_[0][spans_count++].count = end - start;
                start = end;
            }
        }

        RadixSort_LSB<SortSpan32>(temp_sort_spans_32_[0].data(), temp_sort_spans_32_[0].data() + spans_count,
                                  temp_sort_spans_32_[1].data());

        // decompress sorted spans
        for (uint32_t i = 0; i < spans_count; i++) {
            for (uint32_t j = 0; j < temp_sort_spans_32_[0][i].count; j++) {
                list.shadow_batch_indices[sh_batch_indices_counter++] = temp_sort_spans_32_[0][i].base + j;
            }
        }
        assert(sh_batch_indices_counter == shadow_batch_end);

        // Merge similar batches
        for (uint32_t start = sh_list.shadow_batch_start, end = sh_list.shadow_batch_start + 1; end <= shadow_batch_end;
             end++) {
            if (end == shadow_batch_end || list.shadow_batches[list.shadow_batch_indices[start]].sort_key !=
                                               list.shadow_batches[list.shadow_batch_indices[end]].sort_key) {

                BasicDrawBatch &b1 = list.shadow_batches[list.shadow_batch_indices[start]];
                b1.instance_start = uint32_t(list.instance_indices.size());
                for (uint32_t j = 0; j < b1.instance_count; ++j) {
                    list.instance_indices.emplace_back(b1.instance_index, b1.material_index);
                }

                for (uint32_t i = start + 1; i < end; i++) {
                    BasicDrawBatch &b2 = list.shadow_batches[list.shadow_batch_indices[i]];
                    b2.instance_start = uint32_t(list.instance_indices.size());
                    for (uint32_t j = 0; j < b2.instance_count; ++j) {
                        list.instance_indices.emplace_back(b2.instance_index, b2.material_index);
                    }

                    if (b1.base_vertex == b2.base_vertex) {
                        b1.instance_count += b2.instance_count;
                        b2.instance_count = 0;
                    }
                }

                start = end;
            }
        }
    }

    /*{ // sort feedback array
        temp_sort_spans_32_[0].count = tex_feedback_.count;
        temp_sort_spans_32_[1].count = tex_feedback_.count;
        uint32_t spans_count = 0;

        const uint32_t tex_feedback_end = tex_feedback_.count;
        for (uint32_t start = 0, end = 1; end <= tex_feedback_end; end++) {
            if (end == tex_feedback_end || (tex_feedback_.data[start].sort_key !=
                                            tex_feedback_.data[end].sort_key)) {
                temp_sort_spans_32_[0].data[spans_count].key =
                    tex_feedback_.data[start].sort_key;
                temp_sort_spans_32_[0].data[spans_count].base = start;
                temp_sort_spans_32_[0].data[spans_count++].count = end - start;
                start = end;
            }
        }

        RadixSort_LSB<SortSpan32>(temp_sort_spans_32_[0].data,
                                  temp_sort_spans_32_[0].data + spans_count,
                                  temp_sort_spans_32_[1].data);

        // decompress sorted spans
        tex_feedback_sorted_.count = 0;
        for (uint32_t i = 0; i < spans_count; i++) {
            for (uint32_t j = 0; j < temp_sort_spans_32_[0].data[i].count; j++) {
                tex_feedback_sorted_.data[tex_feedback_sorted_.count++] =
                    tex_feedback_.data[temp_sort_spans_32_[0].data[i].base + j];
            }
        }

        volatile int ii = 0;
    }*/

    std::sort(list.desired_textures.data(), list.desired_textures.data() + list.desired_textures.size(),
              [](const TexEntry &t1, const TexEntry &t2) { return t1.sort_key < t2.sort_key; });

    OPTICK_POP();

    /**********************************************************************************/
    /*                                ASSIGNING TO CLUSTERS                           */
    /**********************************************************************************/

    OPTICK_PUSH("ASSIGNING");
    const uint64_t items_assignment_start = Sys::GetTimeUs();

    if (!list.lights.empty() || !list.decals.empty() || !list.probes.empty()) {
        { // main frustum
            list.draw_cam.ExtractSubFrustums(ITEM_GRID_RES_X, ITEM_GRID_RES_Y, ITEM_GRID_RES_Z,
                                             temp_sub_frustums_.data);

            std::future<void> futures[ITEM_GRID_RES_Z];
            std::atomic_int items_count = {};

            for (int i = 0; i < ITEM_GRID_RES_Z; i++) {
                futures[i] = threads_.Enqueue(ClusterItemsForZSlice_Job, i, temp_sub_frustums_.data, decals_boxes_.data,
                                              lights_src, litem_to_lsource_, std::ref(list), list.cells.data,
                                              list.items.data, std::ref(items_count));
            }

            for (std::future<void> &fut : futures) {
                fut.wait();
            }

            list.items.count = std::min(items_count.load(), MAX_ITEMS_TOTAL);
        }
        { // rt frustum
            list.ext_cam.ExtractSubFrustums(ITEM_GRID_RES_X, ITEM_GRID_RES_Y, ITEM_GRID_RES_Z, temp_sub_frustums_.data);

            std::future<void> futures[ITEM_GRID_RES_Z];
            std::atomic_int items_count = {};

            for (int i = 0; i < ITEM_GRID_RES_Z; i++) {
                futures[i] = threads_.Enqueue(ClusterItemsForZSlice_Job, i, temp_sub_frustums_.data, decals_boxes_.data,
                                              lights_src, litem_to_lsource_, std::ref(list), list.rt_cells.data,
                                              list.rt_items.data, std::ref(items_count));
            }

            for (std::future<void> &fut : futures) {
                fut.wait();
            }

            list.rt_items.count = std::min(items_count.load(), MAX_ITEMS_TOTAL);
        }
    } else {
        CellData _dummy = {};
        std::fill(list.cells.data, list.cells.data + ITEM_CELLS_COUNT, _dummy);
        list.items.count = 0;
        std::fill(list.rt_cells.data, list.rt_cells.data + ITEM_CELLS_COUNT, _dummy);
        list.rt_items.count = 0;
    }

    if (list.render_settings.enable_culling && list.render_settings.debug_culling) {
        list.depth_w = cull_ctx_.w;
        list.depth_h = cull_ctx_.h;

        temp_depth.resize(size_t(list.depth_w) * list.depth_h);
        swCullCtxDebugDepth(&cull_ctx_, temp_depth.data());

        list.depth_pixels.resize(4ull * list.depth_w * list.depth_h);
        const float *_depth_pixels_f32 = temp_depth.data();
        uint8_t *_depth_pixels_u8 = list.depth_pixels.data();

        for (int x = 0; x < list.depth_w; x++) {
            for (int y = 0; y < list.depth_h; y++) {
                const float z = _depth_pixels_f32[(list.depth_h - y - 1) * list.depth_w + x];
                _depth_pixels_u8[4ul * (y * list.depth_w + x) + 0] = uint8_t(z * 255);
                _depth_pixels_u8[4ul * (y * list.depth_w + x) + 1] = uint8_t(z * 255);
                _depth_pixels_u8[4ul * (y * list.depth_w + x) + 2] = uint8_t(z * 255);
                _depth_pixels_u8[4ul * (y * list.depth_w + x) + 3] = 255;
            }
        }
    }

    uint64_t iteration_end = Sys::GetTimeUs();

    if (list.render_settings.enable_timers) {
        list.frontend_info.start_timepoint_us = iteration_start;
        list.frontend_info.end_timepoint_us = iteration_end;
        list.frontend_info.occluders_time_us = uint32_t(main_gather_start - occluders_start);
        list.frontend_info.main_gather_time_us = uint32_t(shadow_gather_start - main_gather_start);
        list.frontend_info.shadow_gather_time_us = uint32_t(drawables_sort_start - shadow_gather_start);
        list.frontend_info.drawables_sort_time_us = uint32_t(items_assignment_start - drawables_sort_start);
        list.frontend_info.items_assignment_time_us = uint32_t(iteration_end - items_assignment_start);
    }

    ++frame_index_;

    OPTICK_POP();
    __itt_task_end(__g_itt_domain);
}

void Eng::Renderer::GatherObjectsForZSlice_Job(const Ren::Frustum &frustum, const SceneData &scene,
                                               const Ren::Vec3f &cam_pos, const Ren::Mat4f &clip_from_identity,
                                               const uint64_t comp_mask, SWcull_ctx *cull_ctx, const uint8_t visit_mask,
                                               ProcessedObjData proc_objects[],
                                               Ren::HashMap32<uint32_t, VisObjStorage> &out_visible_objects2) {
    using namespace RendererInternal;
    using namespace Ren;

    OPTICK_EVENT();

    // retrieve pointers to components for fast access
    const auto *transforms = (Transform *)scene.comp_store[CompTransform]->SequentialData();

    uint32_t stack[MAX_STACK_SIZE];
    uint32_t stack_size = 0;

    // Gather meshes and lights, skip occluded and frustum culled
    stack_size = 0;
    stack[stack_size++] = scene.root_node;

    while (stack_size) {
        const uint32_t cur = stack[--stack_size] & IndexBits;
        uint32_t skip_frustum_check = stack[stack_size] & SkipFrustumCheckBit;
        const bvh_node_t *n = &scene.nodes[cur];

        const float bbox_points[8][3] = {BBOX_POINTS(n->bbox_min, n->bbox_max)};
        eVisResult cam_visibility = eVisResult::PartiallyVisible;
        if (!skip_frustum_check) {
            cam_visibility = frustum.CheckVisibility(bbox_points);
            if (cam_visibility == eVisResult::FullyVisible) {
                skip_frustum_check = SkipFrustumCheckBit;
            }
        }

        if (cull_ctx && cam_visibility != eVisResult::Invisible) {
            // do not question visibility of the node in which we are inside
            if (cam_pos[0] < n->bbox_min[0] - 0.5f || cam_pos[1] < n->bbox_min[1] - 0.5f ||
                cam_pos[2] < n->bbox_min[2] - 0.5f || cam_pos[0] > n->bbox_max[0] + 0.5f ||
                cam_pos[1] > n->bbox_max[1] + 0.5f || cam_pos[2] > n->bbox_max[2] + 0.5f) {
                SWcull_surf surf;

                surf.type = SW_OCCLUDEE;
                surf.prim_type = SW_TRIANGLES;
                surf.index_type = SW_UNSIGNED_INT;
                surf.attribs = &bbox_points[0][0];
                surf.indices = &bbox_indices[0];
                surf.stride = 3 * sizeof(float);
                surf.count = 36;
                surf.xform = ValuePtr(clip_from_identity);

                swCullCtxSubmitCullSurfs(cull_ctx, &surf, 1);

                if (surf.visible == 0) {
                    cam_visibility = eVisResult::Invisible;
                }
            }
        }

        if (cam_visibility == eVisResult::Invisible) {
            continue;
        }

        if (!n->leaf_node) {
            stack[stack_size++] = skip_frustum_check | n->left_child;
            stack[stack_size++] = skip_frustum_check | n->right_child;
        } else {
            const SceneObject &obj = scene.objects[n->prim_index];

            if (obj.comp_mask & comp_mask) {
                const Transform &tr = transforms[obj.components[CompTransform]];

                const float bbox_points[8][3] = {BBOX_POINTS(tr.bbox_min_ws, tr.bbox_max_ws)};

                if (!skip_frustum_check) {
                    // Node has slightly enlarged bounds, so we need to check object's bounding box here
                    cam_visibility = frustum.CheckVisibility(bbox_points);
                    if (cam_visibility == eVisResult::Invisible) {
                        continue;
                    }
                }

                if (cull_ctx && cam_visibility != eVisResult::Invisible) {
                    // do not question visibility of the object in which we are inside
                    if (cam_pos[0] < tr.bbox_min_ws[0] - 0.5f || cam_pos[1] < tr.bbox_min_ws[1] - 0.5f ||
                        cam_pos[2] < tr.bbox_min_ws[2] - 0.5f || cam_pos[0] > tr.bbox_max_ws[0] + 0.5f ||
                        cam_pos[1] > tr.bbox_max_ws[1] + 0.5f || cam_pos[2] > tr.bbox_max_ws[2] + 0.5f) {
                        SWcull_surf surf;

                        surf.type = SW_OCCLUDEE;
                        surf.prim_type = SW_TRIANGLES;
                        surf.index_type = SW_UNSIGNED_INT;
                        surf.attribs = &bbox_points[0][0];
                        surf.indices = &bbox_indices[0];
                        surf.stride = 3 * sizeof(float);
                        surf.count = 36;
                        surf.xform = ValuePtr(clip_from_identity);

                        swCullCtxSubmitCullSurfs(cull_ctx, &surf, 1);

                        if (surf.visible == 0) {
                            cam_visibility = eVisResult::Invisible;
                        }
                    }
                }

                if (cam_visibility != eVisResult::Invisible) {
                    const uint8_t mask = proc_objects[n->prim_index].visited_mask.fetch_and(~visit_mask);
                    if (mask & visit_mask) {
                        VisObjStorage &s = out_visible_objects2[obj.comp_mask & comp_mask];
                        const uint32_t index2 = s.count.fetch_add(1);
                        s.objects[index2] = {n->prim_index,
                                             Distance2(cam_pos, 0.5f * (tr.bbox_max_ws + tr.bbox_min_ws))};
                    }
                }
            }
        }
    }
}

void Eng::Renderer::ClusterItemsForZSlice_Job(const int slice, const Ren::Frustum *sub_frustums,
                                              const BBox *decals_boxes, const LightSource *const light_sources,
                                              Ren::Span<const uint32_t> litem_to_lsource, const DrawList &list,
                                              CellData out_cells[], ItemData out_items[],
                                              std::atomic_int &items_count) {
    using namespace RendererInternal;
    using namespace Ren;

    OPTICK_EVENT();

    const float epsilon = 0.001f;

    const int frustums_per_slice = ITEM_GRID_RES_X * ITEM_GRID_RES_Y;
    const int base_index = slice * frustums_per_slice;
    const Frustum *first_sf = &sub_frustums[base_index];

    // Reset cells information for slice
    for (int s = 0; s < frustums_per_slice; s++) {
        out_cells[base_index + s] = {};
    }

    // Gather to local list first
    ItemData local_items[ITEM_GRID_RES_X * ITEM_GRID_RES_Y][MAX_ITEMS_PER_CELL];

    for (int j = 0; j < int(list.lights.size()); j++) {
        const LightItem &l = list.lights[j];
        const LightSource *ls = &light_sources[litem_to_lsource[j]];
        const float radius = ls->radius;
        const float cull_radius = ls->cull_radius;
        const float cap_radius = ls->cap_radius;

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if light is inside of a whole z-slice
        for (int k = int(eCamPlane::Near); k <= int(eCamPlane::Far); k++) {
            const float *p_n = ValuePtr(first_sf->planes[k].n);
            const float p_d = first_sf->planes[k].d;

            const float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
            if (dist < -cull_radius) {
                visible_to_slice = eVisResult::Invisible;
            } else if (l.spot > epsilon) {
                /*const float dn[3] = _CROSS(l.dir, p_n);
                const float m[3] = _CROSS(l.dir, dn);

                const float Q[3] = {l.pos[0] - cull_radius * l.dir[0] - cap_radius * m[0],
                                    l.pos[1] - cull_radius * l.dir[1] - cap_radius * m[1],
                                    l.pos[2] - cull_radius * l.dir[2] - cap_radius * m[2]};

                if (dist < -radius && p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d < -epsilon) {
                    visible_to_slice = eVisResult::Invisible;
                }*/
            }
        }

        // Skip light for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += ITEM_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if light is inside of grid line
            for (int k = int(eCamPlane::Top); k <= int(eCamPlane::Bottom); k++) {
                const float *p_n = ValuePtr(first_line_sf->planes[k].n);
                const float p_d = first_line_sf->planes[k].d;

                const float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
                if (dist < -cull_radius) {
                    visible_to_line = eVisResult::Invisible;
                } else if (l.spot > epsilon) {
                    /*const float dn[3] = _CROSS(l.dir, p_n);
                    const float m[3] = _CROSS(l.dir, dn);

                    const float Q[3] = {l.pos[0] - cull_radius * l.dir[0] - cap_radius * m[0],
                                        l.pos[1] - cull_radius * l.dir[1] - cap_radius * m[1],
                                        l.pos[2] - cull_radius * l.dir[2] - cap_radius * m[2]};

                    const float val = p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d;

                    if (dist < -radius && val < -epsilon) {
                        visible_to_line = eVisResult::Invisible;
                    }*/
                }
            }

            // Skip light for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < ITEM_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::Left); k <= int(eCamPlane::Right); k++) {
                    const float *p_n = ValuePtr(sf->planes[k].n);
                    const float p_d = sf->planes[k].d;

                    const float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
                    if (dist < -cull_radius) {
                        res = eVisResult::Invisible;
                    } else if (l.spot > epsilon) {
                        /*const float dn[3] = _CROSS(l.dir, p_n);
                        const float m[3] = _CROSS(l.dir, dn);

                        const float Q[3] = {l.pos[0] - cull_radius * l.dir[0] - cap_radius * m[0],
                                            l.pos[1] - cull_radius * l.dir[1] - cap_radius * m[1],
                                            l.pos[2] - cull_radius * l.dir[2] - cap_radius * m[2]};

                        if (dist < -radius && p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d < -epsilon) {
                            res = eVisResult::Invisible;
                        }*/
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = out_cells[index];
                    if (cell.light_count < MAX_LIGHTS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.light_count].light_index = uint16_t(j);
                        cell.light_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < int(list.decals.size()); j++) {
        const DecalItem &de = list.decals[j];

        const float bbox_points[8][3] = {BBOX_POINTS(decals_boxes[j].bmin, decals_boxes[j].bmax)};

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if decal is inside of a whole slice
        for (int k = int(eCamPlane::Near); k <= int(eCamPlane::Far); k++) {
            int in_count = 8;

            for (int i = 0; i < 8; i++) { // NOLINT
                const float dist = first_sf->planes[k].n[0] * bbox_points[i][0] +
                                   first_sf->planes[k].n[1] * bbox_points[i][1] +
                                   first_sf->planes[k].n[2] * bbox_points[i][2] + first_sf->planes[k].d;
                if (dist < 0.0f) {
                    in_count--;
                }
            }

            if (in_count == 0) {
                visible_to_slice = eVisResult::Invisible;
                break;
            }
        }

        // Skip decal for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += ITEM_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if decal is inside of grid line
            for (int k = int(eCamPlane::Top); k <= int(eCamPlane::Bottom); k++) {
                int in_count = 8;

                for (int i = 0; i < 8; i++) { // NOLINT
                    const float dist = first_line_sf->planes[k].n[0] * bbox_points[i][0] +
                                       first_line_sf->planes[k].n[1] * bbox_points[i][1] +
                                       first_line_sf->planes[k].n[2] * bbox_points[i][2] + first_line_sf->planes[k].d;
                    if (dist < 0.0f) {
                        in_count--;
                    }
                }

                if (in_count == 0) {
                    visible_to_line = eVisResult::Invisible;
                    break;
                }
            }

            // Skip decal for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < ITEM_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::Left); k <= int(eCamPlane::Right); k++) {
                    int in_count = 8;

                    for (int i = 0; i < 8; i++) { // NOLINT
                        const float dist = sf->planes[k].n[0] * bbox_points[i][0] +
                                           sf->planes[k].n[1] * bbox_points[i][1] +
                                           sf->planes[k].n[2] * bbox_points[i][2] + sf->planes[k].d;
                        if (dist < 0.0f) {
                            in_count--;
                        }
                    }

                    if (in_count == 0) {
                        res = eVisResult::Invisible;
                        break;
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = out_cells[index];
                    if (cell.decal_count < MAX_DECALS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.decal_count].decal_index = uint16_t(j);
                        cell.decal_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < int(list.probes.size()); j++) {
        const ProbeItem &p = list.probes[j];
        const float *p_pos = &p.position[0];

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if probe is inside of a whole slice
        for (int k = int(eCamPlane::Near); k <= int(eCamPlane::Far); k++) {
            float dist = first_sf->planes[k].n[0] * p_pos[0] + first_sf->planes[k].n[1] * p_pos[1] +
                         first_sf->planes[k].n[2] * p_pos[2] + first_sf->planes[k].d;
            if (dist < -p.radius) {
                visible_to_slice = eVisResult::Invisible;
            }
        }

        // Skip probe for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += ITEM_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if probe is inside of grid line
            for (int k = int(eCamPlane::Top); k <= int(eCamPlane::Bottom); k++) {
                float dist = first_line_sf->planes[k].n[0] * p_pos[0] + first_line_sf->planes[k].n[1] * p_pos[1] +
                             first_line_sf->planes[k].n[2] * p_pos[2] + first_line_sf->planes[k].d;
                if (dist < -p.radius) {
                    visible_to_line = eVisResult::Invisible;
                }
            }

            // Skip probe for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < ITEM_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::Left); k <= int(eCamPlane::Right); k++) {
                    const float dist = sf->planes[k].n[0] * p_pos[0] + sf->planes[k].n[1] * p_pos[1] +
                                       sf->planes[k].n[2] * p_pos[2] + sf->planes[k].d;

                    if (dist < -p.radius) {
                        res = eVisResult::Invisible;
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = out_cells[index];
                    if (cell.probe_count < MAX_PROBES_PER_CELL) {
                        local_items[row_offset + col_offset][cell.probe_count].probe_index = uint16_t(j);
                        cell.probe_count++;
                    }
                }
            }
        }
    }

    const float EllipsoidInfluence = 3.0f;

    for (int j = 0; j < int(list.ellipsoids.size()); j++) {
        const EllipsItem &e = list.ellipsoids[j];
        const float *p_pos = &e.position[0];

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if ellipsoid is inside of a whole slice
        for (int k = int(eCamPlane::Near); k <= int(eCamPlane::Far); k++) {
            const float dist = first_sf->planes[k].n[0] * p_pos[0] + first_sf->planes[k].n[1] * p_pos[1] +
                               first_sf->planes[k].n[2] * p_pos[2] + first_sf->planes[k].d;
            if (dist < -EllipsoidInfluence) {
                visible_to_slice = eVisResult::Invisible;
            }
        }

        // Skip ellipsoid for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += ITEM_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if ellipsoid is inside of grid line
            for (int k = int(eCamPlane::Top); k <= int(eCamPlane::Bottom); k++) {
                float dist = first_line_sf->planes[k].n[0] * p_pos[0] + first_line_sf->planes[k].n[1] * p_pos[1] +
                             first_line_sf->planes[k].n[2] * p_pos[2] + first_line_sf->planes[k].d;
                if (dist < -EllipsoidInfluence) {
                    visible_to_line = eVisResult::Invisible;
                }
            }

            // Skip ellipsoid for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < ITEM_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::Left); k <= int(eCamPlane::Right); k++) {
                    const float dist = sf->planes[k].n[0] * p_pos[0] + sf->planes[k].n[1] * p_pos[1] +
                                       sf->planes[k].n[2] * p_pos[2] + sf->planes[k].d;

                    if (dist < -EllipsoidInfluence) {
                        res = eVisResult::Invisible;
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = out_cells[index];
                    if (cell.ellips_count < MAX_ELLIPSES_PER_CELL) {
                        local_items[row_offset + col_offset][cell.ellips_count].ellips_index = uint16_t(j);
                        cell.ellips_count++;
                    }
                }
            }
        }
    }

    // Pack gathered local item data to total list
    for (int s = 0; s < frustums_per_slice; s++) {
        CellData &cell = out_cells[base_index + s];

        const int local_items_count =
            int(std::max(cell.light_count, std::max(cell.decal_count, std::max(cell.probe_count, cell.ellips_count))));

        if (local_items_count) {
            cell.item_offset = items_count.fetch_add(local_items_count);
            if (cell.item_offset > MAX_ITEMS_TOTAL) {
                cell.item_offset = 0;
                cell.light_count = cell.decal_count = cell.probe_count = cell.ellips_count = 0;
            } else {
                const int free_items_left = MAX_ITEMS_TOTAL - cell.item_offset;

                cell.light_count = std::min(int(cell.light_count), free_items_left);
                cell.decal_count = std::min(int(cell.decal_count), free_items_left);
                cell.probe_count = std::min(int(cell.probe_count), free_items_left);
                cell.ellips_count = std::min(int(cell.ellips_count), free_items_left);

                memcpy(&out_items[cell.item_offset], &local_items[s][0], local_items_count * sizeof(ItemData));
            }
        }
    }
}

void RendererInternal::__push_ellipsoids(const Eng::Drawable &dr, const Ren::Mat4f &world_from_object,
                                         Eng::DrawList &list) {
    /*if (list.ellipsoids.size() + dr.ellipsoids_count > Eng::MAX_ELLIPSES_TOTAL) {
        return;
    }

    const Ren::Skeleton *skel = dr.mesh->skel();

    for (int i = 0; i < dr.ellipsoids_count; i++) {
        const Eng::Drawable::Ellipsoid &e = dr.ellipsoids[i];
        Eng::EllipsItem &ei = list.ellipsoids.emplace_back();

        auto pos = Ren::Vec4f{e.offset[0], e.offset[1], e.offset[2], 1.0f},
             axis = Ren::Vec4f{-e.axis[0], -e.axis[1], -e.axis[2], 0.0f};

        if (e.bone_index != -1 && skel->bones_count) {
            const Ren::Mat4f _world_from_object = world_from_object * skel->bones[e.bone_index].cur_comb_matrix;

            pos = _world_from_object * pos;
            axis = _world_from_object * axis;
        } else {
            pos = world_from_object * pos;
            axis = world_from_object * axis;
        }

        int perp = 0;
        if (std::abs(axis[1]) <= std::abs(axis[0]) && std::abs(axis[1]) <= std::abs(axis[2])) {
            perp = 1;
        } else if (std::abs(axis[2]) <= std::abs(axis[0]) && std::abs(axis[2]) <= std::abs(axis[1])) {
            perp = 2;
        }

        memcpy(&ei.position[0], &pos[0], 3 * sizeof(float));
        ei.radius = e.radius;
        memcpy(&ei.axis[0], &axis[0], 3 * sizeof(float));
        ei.perp = perp;
    }*/
}

uint32_t RendererInternal::__push_skeletal_mesh(const uint32_t skinned_buf_vtx_offset, const Eng::AnimState &as,
                                                const Ren::Mesh *mesh, Eng::DrawList &list) {
    const Ren::Skeleton *skel = mesh->skel();

    const auto palette_start = uint16_t(list.skin_transforms.size() / 2);
    list.skin_transforms.resize(list.skin_transforms.size() + 2 * skel->bones_count);
    Eng::SkinTransform *out_matr_palette = &list.skin_transforms[2 * palette_start];

    for (int i = 0; i < skel->bones_count; i++) {
        const Ren::Mat4f matr_curr_trans = Transpose(as.matr_palette_curr[i]);
        memcpy(out_matr_palette[2 * i + 0].matr, ValuePtr(matr_curr_trans), 12 * sizeof(float));

        const Ren::Mat4f matr_prev_trans = Transpose(as.matr_palette_prev[i]);
        memcpy(out_matr_palette[2 * i + 1].matr, ValuePtr(matr_prev_trans), 12 * sizeof(float));
    }

    const Ren::BufferRange &sk_buf = mesh->sk_attribs_buf();
    const Ren::BufferRange &deltas_buf = mesh->sk_deltas_buf();

    const uint32_t vertex_beg = sk_buf.sub.offset / 48, vertex_cnt = sk_buf.size / 48;
    const uint32_t deltas_offset = deltas_buf.sub.offset / 24;

    const uint32_t curr_out_offset = skinned_buf_vtx_offset + list.skin_vertices_count;

    Eng::SkinRegion &sr = list.skin_regions.emplace_back();
    sr.in_vtx_offset = vertex_beg;
    sr.out_vtx_offset = curr_out_offset;
    sr.delta_offset = deltas_offset;
    sr.xform_offset = palette_start;
    // shape key data for current frame
    sr.shape_key_offset_curr = list.shape_keys_data.count;
    memcpy(&list.shape_keys_data.data[list.shape_keys_data.count], &as.shape_palette_curr[0],
           as.shape_palette_count_curr * sizeof(Eng::ShapeKeyData));
    list.shape_keys_data.count += as.shape_palette_count_curr;
    sr.shape_key_count_curr = as.shape_palette_count_curr;
    // shape key data from previous frame
    sr.shape_key_offset_prev = list.shape_keys_data.count;
    memcpy(&list.shape_keys_data.data[list.shape_keys_data.count], &as.shape_palette_prev[0],
           as.shape_palette_count_prev * sizeof(Eng::ShapeKeyData));
    list.shape_keys_data.count += as.shape_palette_count_prev;
    sr.shape_key_count_prev = as.shape_palette_count_prev;
    sr.vertex_count = vertex_cnt;
    if (skel->shapes_count) {
        sr.shape_keyed_vertex_count = skel->shapes[0].delta_count;
    } else {
        sr.shape_keyed_vertex_count = 0;
    }

    list.skin_vertices_count += vertex_cnt;

    assert(list.skin_vertices_count <= Eng::MAX_SKIN_VERTICES_TOTAL);
    return curr_out_offset;
}

uint32_t RendererInternal::__record_texture(std::vector<Eng::TexEntry> &storage, const Ren::Tex2DRef &tex,
                                            const int prio, const uint16_t distance) {
    const uint32_t index = tex.index();

    auto entry = std::lower_bound(storage.begin(), storage.end(), index,
                                  [](const Eng::TexEntry &t1, const uint32_t t2) { return t1.index < t2; });
    if (entry == storage.end() || entry->index != index) {
        entry = storage.insert(entry, {index});
    }

    entry->prio = prio;
    entry->cam_dist = std::min(entry->cam_dist, uint32_t(distance));

    return uint32_t(std::distance(storage.begin(), entry));
}

void RendererInternal::__record_textures(std::vector<Eng::TexEntry> &storage, const Ren::Material *mat,
                                         const bool is_animated, const uint16_t distance) {
    static const int TexPriorities[] = {0, 1, 2, 0, 4, 5, 6, 7};
    for (int i = 0; i < int(mat->textures.size()); ++i) {
        int prio = TexPriorities[i];
        if (!is_animated) {
            prio += 8;
        }
        prio = std::min(prio, 15);
        __record_texture(storage, mat->textures[i], prio, distance);
    }
}

#undef BBOX_POINTS
#undef _CROSS

#undef REN_UNINITIALIZE_X2
#undef REN_UNINITIALIZE_X4
#undef REN_UNINITIALIZE_X8
