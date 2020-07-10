#include "Renderer.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

namespace RendererInternal {
bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]) {
    return p[0] > bbox_min[0] && p[0] < bbox_max[0] && p[1] > bbox_min[1] &&
           p[1] < bbox_max[1] && p[2] > bbox_min[2] && p[2] < bbox_max[2];
}

const uint8_t bbox_indices[] = {0, 1, 2, 2, 1, 3, 0, 4, 5, 0, 5, 1, 0, 2, 4, 4, 2, 6,
                                2, 3, 6, 6, 3, 7, 3, 1, 5, 3, 5, 7, 4, 6, 5, 5, 6, 7};

template <typename SpanType>
void RadixSort_LSB(SpanType *begin, SpanType *end, SpanType *begin1) {
    SpanType *end1 = begin1 + (end - begin);

    for (unsigned shift = 0; shift < sizeof(SpanType::key) * 8; shift += 8) {
        size_t count[0x100] = {};
        for (SpanType *p = begin; p != end; p++) {
            count[(p->key >> shift) & 0xFFu]++;
        }
        SpanType *bucket[0x100], *q = begin1;
        for (int i = 0; i < 0x100; q += count[i++]) {
            bucket[i] = q;
        }
        for (SpanType *p = begin; p != end; p++) {
            *bucket[(p->key >> shift) & 0xFFu]++ = *p;
        }
        std::swap(begin, begin1);
        std::swap(end, end1);
    }
}

static const Ren::Vec4f ClipFrustumPoints[] = {
    Ren::Vec4f{-1.0f, -1.0f, -1.0f, 1.0f}, Ren::Vec4f{1.0f, -1.0f, -1.0f, 1.0f},
    Ren::Vec4f{1.0f, 1.0f, -1.0f, 1.0f},   Ren::Vec4f{-1.0f, 1.0f, -1.0f, 1.0f},

    Ren::Vec4f{-1.0f, -1.0f, 1.0f, 1.0f},  Ren::Vec4f{1.0f, -1.0f, 1.0f, 1.0f},
    Ren::Vec4f{1.0f, 1.0f, 1.0f, 1.0f},    Ren::Vec4f{-1.0f, 1.0f, 1.0f, 1.0f}};

static const uint8_t SunShadowUpdatePattern[4] = {
    0b11111111, // update cascade 0 every frame
    0b11111111, // update cascade 1 every frame
    0b01010101, // update cascade 2 once in two frames
    0b00100010  // update cascade 3 once in four frames
};

int16_t f32_to_s16(float value) { return int16_t(value * 32767); }

uint16_t f32_to_u16(float value) { return uint16_t(value * 65535); }

uint8_t f32_to_u8(float value) { return uint8_t(value * 255); }

void __push_ellipsoids(const Drawable &dr, const Ren::Mat4f &world_from_object,
                       DrawList &list);
uint32_t __push_skeletal_mesh(uint32_t skinned_buf_vtx_offset, const AnimState &as,
                              const Ren::Mesh *mesh, DrawList &list);
void __init_wind_params(const VegState &vs, const Environment &env,
                        const Ren::Mat4f &object_from_world, InstanceData &instance);
} // namespace RendererInternal

#define REN_UNINITIALIZE_X2(t)                                                           \
    t{Ren::Uninitialize}, t { Ren::Uninitialize }
#define REN_UNINITIALIZE_X4(t) REN_UNINITIALIZE_X2(t), REN_UNINITIALIZE_X2(t)
#define REN_UNINITIALIZE_X8(t) REN_UNINITIALIZE_X4(t), REN_UNINITIALIZE_X4(t)

#define BBOX_POINTS(min, max)                                                            \
    (min)[0], (min)[1], (min)[2], (max)[0], (min)[1], (min)[2], (min)[0], (min)[1],      \
        (max)[2], (max)[0], (min)[1], (max)[2], (min)[0], (max)[1], (min)[2], (max)[0],  \
        (max)[1], (min)[2], (min)[0], (max)[1], (max)[2], (max)[0], (max)[1], (max)[2]

// faster than std::min/max/abs in debug
#define _MIN(x, y) ((x) < (y) ? (x) : (y))
#define _MAX(x, y) ((x) < (y) ? (y) : (x))
#define _ABS(x) ((x) < 0 ? -(x) : (x))

#define _CROSS(x, y)                                                                     \
    {                                                                                    \
        (x)[1] * (y)[2] - (x)[2] * (y)[1], (x)[2] * (y)[0] - (x)[0] * (y)[2],            \
            (x)[0] * (y)[1] - (x)[1] * (y)[0]                                            \
    }

void Renderer::GatherDrawables(const SceneData &scene, const Ren::Camera &cam,
                               DrawList &list) {
    using namespace RendererInternal;
    using namespace Ren;

    const uint64_t iteration_start = Sys::GetTimeUs();

    list.draw_cam = cam;
    list.env = scene.env;
    list.decals_atlas = &scene.decals_atlas;
    list.probe_storage = &scene.probe_storage;

    // mask render flags with what renderer itself is capable of
    list.render_flags &= render_flags_;

    if (list.render_flags & DebugBVH) {
        // copy nodes list for debugging
        list.temp_nodes = scene.nodes;
        list.root_index = scene.root_node;
    } else {
        // free allocated memory
        list.temp_nodes = {};
    }

    list.light_sources.count = 0;
    list.decals.count = 0;
    list.probes.count = 0;
    list.ellipsoids.count = 0;

    list.instances.count = 0;
    list.shadow_batches.count = 0;
    list.zfill_batches.count = 0;
    list.main_batches.count = 0;

    list.shadow_lists.count = 0;
    list.shadow_regions.count = 0;

    list.skin_transforms.count = 0;
    list.skin_regions.count = 0;
    list.shape_keys_data.count = 0;
    list.skin_vertices_count = 0;

    const bool culling_enabled = (list.render_flags & EnableCulling) != 0;
    const bool lighting_enabled = (list.render_flags & EnableLights) != 0;
    const bool decals_enabled = (list.render_flags & EnableDecals) != 0;
    const bool shadows_enabled = (list.render_flags & EnableShadows) != 0;
    const bool zfill_enabled = (list.render_flags & (EnableZFill | EnableSSAO)) != 0;

    const uint32_t render_mask = list.draw_cam.render_mask();

    int program_index = 0;
    if ((list.render_flags & EnableLightmap) == 0) {
        program_index = 1;
    }

    litem_to_lsource_.count = 0;
    ditem_to_decal_.count = 0;
    decals_boxes_.count = 0;

    memset(proc_objects_.data, 0xff, sizeof(ProcessedObjData) * scene.objects.size());

    // retrieve pointers to components for fast access
    const auto *transforms = (Transform *)scene.comp_store[CompTransform]->Get(0);
    const auto *drawables = (Drawable *)scene.comp_store[CompDrawable]->Get(0);
    const auto *occluders = (Occluder *)scene.comp_store[CompOccluder]->Get(0);
    const auto *lightmaps = (Lightmap *)scene.comp_store[CompLightmap]->Get(0);
    const auto *lights_src = (LightSource *)scene.comp_store[CompLightSource]->Get(0);
    const auto *decals = (Decal *)scene.comp_store[CompDecal]->Get(0);
    const auto *probes = (LightProbe *)scene.comp_store[CompProbe]->Get(0);
    const auto *anims = (AnimState *)scene.comp_store[CompAnimState]->Get(0);
    const auto *vegs = (VegState *)scene.comp_store[CompVegState]->Get(0);

    // make sure we can access components by index
    assert(scene.comp_store[CompTransform]->IsSequential());
    assert(scene.comp_store[CompDrawable]->IsSequential());
    assert(scene.comp_store[CompOccluder]->IsSequential());
    assert(scene.comp_store[CompLightmap]->IsSequential());
    assert(scene.comp_store[CompLightSource]->IsSequential());
    assert(scene.comp_store[CompDecal]->IsSequential());
    assert(scene.comp_store[CompProbe]->IsSequential());
    assert(scene.comp_store[CompAnimState]->IsSequential());
    assert(scene.comp_store[CompVegState]->IsSequential());

    const uint32_t skinned_buf_vtx_offset = skinned_buf1_vtx_offset_ / 16;

    const Mat4f &view_from_world = list.draw_cam.view_matrix(),
                &clip_from_view = list.draw_cam.proj_matrix();

    swCullCtxClear(&cull_ctx_);

    const Mat4f view_from_identity = view_from_world * Mat4f{1.0f},
                clip_from_identity = clip_from_view * view_from_identity;

    const uint32_t SkipCheckBit = (1u << 31u);
    const uint32_t IndexBits = ~SkipCheckBit;

    uint32_t stack[MAX_STACK_SIZE];
    uint32_t stack_size = 0;

    /**********************************************************************************/
    /*                                OCCLUDERS PROCESSING                            */
    /**********************************************************************************/

    const uint64_t occluders_start = Sys::GetTimeUs();

    if (scene.root_node != 0xffffffff) {
        // Rasterize occluder meshes into a small framebuffer
        stack[stack_size++] = scene.root_node;

        while (stack_size && culling_enabled) {
            uint32_t cur = stack[--stack_size] & IndexBits;
            uint32_t skip_check = (stack[stack_size] & SkipCheckBit);
            const bvh_node_t *n = &scene.nodes[cur];

            if (!skip_check) {
                const eVisResult res =
                    list.draw_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                if (res == eVisResult::Invisible) {
                    continue;
                } else if (res == eVisResult::FullyVisible) {
                    skip_check = SkipCheckBit;
                }
            }

            if (!n->prim_count) {
                stack[stack_size++] = skip_check | n->left_child;
                stack[stack_size++] = skip_check | n->right_child;
            } else {
                const SceneObject &obj = scene.objects[n->prim_index];

                const uint32_t occluder_flags = CompTransformBit | CompOccluderBit;
                if ((obj.comp_mask & occluder_flags) == occluder_flags) {
                    const Transform &tr = transforms[obj.components[CompTransform]];

                    // Node has slightly enlarged bounds, so we need to check object's
                    // bounding box here
                    if (!skip_check &&
                        list.draw_cam.CheckFrustumVisibility(
                            tr.bbox_min_ws, tr.bbox_max_ws) == eVisResult::Invisible) {
                        continue;
                    }

                    const Mat4f &world_from_object = tr.mat;

                    const Mat4f view_from_object = view_from_world * world_from_object,
                                clip_from_object = clip_from_view * view_from_object;

                    const Occluder &occ = occluders[obj.components[CompOccluder]];
                    const Mesh *mesh = occ.mesh.get();

                    SWcull_surf surf[Ren::MaxMeshTriGroupsCount];
                    int surf_count = 0;

                    const TriGroup *s = &mesh->group(0);
                    while (s->offset != -1) {
                        SWcull_surf *_surf = &surf[surf_count++];

                        _surf->type = SW_OCCLUDER;
                        _surf->prim_type = SW_TRIANGLES;
                        _surf->index_type = SW_UNSIGNED_INT;
                        _surf->attribs = mesh->attribs();
                        _surf->indices = ((const uint8_t *)mesh->indices() + s->offset);
                        _surf->stride = 13 * sizeof(float);
                        _surf->count = (SWuint)s->num_indices;
                        _surf->base_vertex = 0;
                        _surf->xform = ValuePtr(clip_from_object);
                        _surf->dont_skip = nullptr;

                        ++s;
                    }

                    swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                }
            }
        }
    }

    /**********************************************************************************/
    /*                        MESHES/LIGHTS/DECALS/PROBES GATHERING                   */
    /**********************************************************************************/

    const uint64_t main_gather_start = Sys::GetTimeUs();

    if (scene.root_node != 0xffffffff) {
        // Gather meshes and lights, skip occluded and frustum culled
        stack_size = 0;
        stack[stack_size++] = scene.root_node;

        while (stack_size) {
            const uint32_t cur = stack[--stack_size] & IndexBits;
            uint32_t skip_check = stack[stack_size] & SkipCheckBit;
            const bvh_node_t *n = &scene.nodes[cur];

            if (!skip_check) {
                const float bbox_points[8][3] = {BBOX_POINTS(n->bbox_min, n->bbox_max)};
                const eVisResult res = list.draw_cam.CheckFrustumVisibility(bbox_points);
                if (res == eVisResult::Invisible) {
                    continue;
                } else if (res == eVisResult::FullyVisible) {
                    skip_check = SkipCheckBit;
                }

                if (culling_enabled) {
                    const Vec3f &cam_pos = list.draw_cam.world_position();

                    // do not question visibility of the node in which we are inside
                    if (cam_pos[0] < n->bbox_min[0] - 0.5f ||
                        cam_pos[1] < n->bbox_min[1] - 0.5f ||
                        cam_pos[2] < n->bbox_min[2] - 0.5f ||
                        cam_pos[0] > n->bbox_max[0] + 0.5f ||
                        cam_pos[1] > n->bbox_max[1] + 0.5f ||
                        cam_pos[2] > n->bbox_max[2] + 0.5f) {
                        SWcull_surf surf;

                        surf.type = SW_OCCLUDEE;
                        surf.prim_type = SW_TRIANGLES;
                        surf.index_type = SW_UNSIGNED_BYTE;
                        surf.attribs = &bbox_points[0][0];
                        surf.indices = &bbox_indices[0];
                        surf.stride = 3 * sizeof(float);
                        surf.count = 36;
                        surf.base_vertex = 0;
                        surf.xform = ValuePtr(clip_from_identity);
                        surf.dont_skip = nullptr;

                        swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                        if (surf.visible == 0) {
                            continue;
                        }
                    }
                }
            }

            if (!n->prim_count) {
                stack[stack_size++] = skip_check | n->left_child;
                stack[stack_size++] = skip_check | n->right_child;
            } else {
                const SceneObject &obj = scene.objects[n->prim_index];

                if ((obj.comp_mask & CompTransformBit) &&
                    (obj.comp_mask & (CompDrawableBit | CompDecalBit |
                                      CompLightSourceBit | CompProbeBit))) { // NOLINT
                    const Transform &tr = transforms[obj.components[CompTransform]];

                    if (!skip_check) {
                        const float bbox_points[8][3] = {
                            BBOX_POINTS(tr.bbox_min_ws, tr.bbox_max_ws)};

                        // Node has slightly enlarged bounds, so we need to check object's
                        // bounding box here
                        if (list.draw_cam.CheckFrustumVisibility(bbox_points) ==
                            eVisResult::Invisible) {
                            continue;
                        }

                        if (culling_enabled) {
                            const Vec3f &cam_pos = list.draw_cam.world_position();

                            // do not question visibility of the object in which we are
                            // inside
                            if (cam_pos[0] < tr.bbox_min_ws[0] - 0.5f ||
                                cam_pos[1] < tr.bbox_min_ws[1] - 0.5f ||
                                cam_pos[2] < tr.bbox_min_ws[2] - 0.5f ||
                                cam_pos[0] > tr.bbox_max_ws[0] + 0.5f ||
                                cam_pos[1] > tr.bbox_max_ws[1] + 0.5f ||
                                cam_pos[2] > tr.bbox_max_ws[2] + 0.5f) {
                                SWcull_surf surf;

                                surf.type = SW_OCCLUDEE;
                                surf.prim_type = SW_TRIANGLES;
                                surf.index_type = SW_UNSIGNED_BYTE;
                                surf.attribs = &bbox_points[0][0];
                                surf.indices = &bbox_indices[0];
                                surf.stride = 3 * sizeof(float);
                                surf.count = 36;
                                surf.base_vertex = 0;
                                surf.xform = ValuePtr(clip_from_identity);
                                surf.dont_skip = nullptr;

                                swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                                if (surf.visible == 0) {
                                    continue;
                                }
                            }
                        }
                    }

                    const Mat4f &world_from_object = tr.mat,
                                &object_from_world = tr.inv_mat;
                    const Mat4f world_from_object_trans = Transpose(world_from_object);

                    proc_objects_.data[n->prim_index].instance_index =
                        list.instances.count;

                    InstanceData &curr_instance =
                        list.instances.data[list.instances.count++];
                    memcpy(&curr_instance.model_matrix[0][0],
                           ValuePtr(world_from_object_trans), 12 * sizeof(float));

                    if (obj.comp_mask & CompLightmapBit) {
                        const Lightmap &lm = lightmaps[obj.components[CompLightmap]];
                        memcpy(&curr_instance.lmap_transform[0], ValuePtr(lm.xform),
                               4 * sizeof(float));
                    } else if (obj.comp_mask & CompVegStateBit) {
                        const VegState &vs = vegs[obj.components[CompVegState]];
                        __init_wind_params(vs, list.env, object_from_world,
                                           curr_instance);
                    }

                    const Mat4f view_from_object = view_from_world * world_from_object,
                                clip_from_object = clip_from_view * view_from_object;

                    if (obj.comp_mask & CompDrawableBit) {
                        const Drawable &dr = drawables[obj.components[CompDrawable]];
                        if (!(dr.vis_mask & render_mask)) {
                            continue;
                        }
                        const Mesh *mesh = dr.mesh.get();

                        const float max_sort_dist = 100.0f;
                        const auto dist = (uint8_t)_MIN(
                            255 * Distance(tr.bbox_min_ws, cam.world_position()) /
                                max_sort_dist,
                            255);

                        uint32_t base_vertex = mesh->attribs_buf1().offset / 16;

                        if (obj.comp_mask & CompAnimStateBit) {
                            const AnimState &as = anims[obj.components[CompAnimState]];
                            base_vertex = __push_skeletal_mesh(skinned_buf_vtx_offset, as,
                                                               mesh, list);
                        }
                        proc_objects_.data[n->prim_index].base_vertex = base_vertex;

                        __push_ellipsoids(dr, world_from_object, list);

                        const uint32_t indices_start = mesh->indices_buf().offset;
                        const TriGroup *grp = &mesh->group(0);
                        while (grp->offset != -1) {
                            const Material *mat = grp->mat.get();
                            const uint32_t mat_flags = mat->flags();

                            MainDrawBatch &main_batch =
                                list.main_batches.data[list.main_batches.count++];

                            main_batch.alpha_blend_bit =
                                (mat_flags & uint32_t(eMaterialFlags::AlphaBlend)) ? 1
                                                                                   : 0;
                            main_batch.prog_id =
                                (uint32_t)mat->programs[program_index].index();
                            main_batch.alpha_test_bit =
                                (mat_flags & uint32_t(eMaterialFlags::AlphaTest)) ? 1 : 0;
                            main_batch.two_sided_bit =
                                (mat_flags & uint32_t(eMaterialFlags::TwoSided)) ? 1 : 0;
                            main_batch.mat_id = (uint32_t)grp->mat.index();
                            main_batch.cam_dist =
                                (mat_flags & uint32_t(eMaterialFlags::AlphaBlend))
                                    ? uint32_t(dist)
                                    : 0;
                            main_batch.indices_offset =
                                (indices_start + grp->offset) / sizeof(uint32_t);
                            main_batch.base_vertex = base_vertex;
                            main_batch.indices_count = grp->num_indices;
                            main_batch.instance_indices[0] =
                                (uint32_t)(list.instances.count - 1);
                            main_batch.instance_count = 1;

                            if (zfill_enabled &&
                                (!(mat->flags() & uint32_t(eMaterialFlags::AlphaBlend)) ||
                                 ((mat->flags() & uint32_t(eMaterialFlags::AlphaBlend)) &&
                                  (mat->flags() &
                                   uint32_t(eMaterialFlags::AlphaTest))))) {
                                DepthDrawBatch &zfill_batch =
                                    list.zfill_batches.data[list.zfill_batches.count++];

                                zfill_batch.type_bits = DepthDrawBatch::TypeSimple;

                                if (obj.comp_mask & CompAnimStateBit) {
                                    zfill_batch.type_bits = DepthDrawBatch::TypeSkinned;
                                } else if (obj.comp_mask & CompVegStateBit) {
                                    zfill_batch.type_bits = DepthDrawBatch::TypeVege;
                                }

                                zfill_batch.alpha_test_bit =
                                    (mat_flags & uint32_t(eMaterialFlags::AlphaTest)) ? 1
                                                                                      : 0;
                                zfill_batch.moving_bit =
                                    (obj.last_change_mask & CompTransformBit) ? 1 : 0;
                                zfill_batch.two_sided_bit =
                                    (mat_flags & uint32_t(eMaterialFlags::TwoSided)) ? 1
                                                                                     : 0;
                                zfill_batch.mat_id =
                                    (mat_flags & uint32_t(eMaterialFlags::AlphaTest))
                                        ? uint32_t(main_batch.mat_id)
                                        : 0;
                                zfill_batch.indices_offset = main_batch.indices_offset;
                                zfill_batch.base_vertex = base_vertex;
                                zfill_batch.indices_count = grp->num_indices;
                                zfill_batch.instance_indices[0] =
                                    (uint32_t)(list.instances.count - 1);
                                zfill_batch.instance_count = 1;
                            }

                            ++grp;
                        }

                        if (obj.last_change_mask & CompTransformBit) {
                            const Mat4f prev_world_from_object_trans =
                                Transpose(tr.prev_mat);

                            // moving objects need 2 transform matrices (for velocity
                            // calculation)
                            InstanceData &prev_instance =
                                list.instances.data[list.instances.count++];
                            memcpy(&prev_instance.model_matrix[0][0],
                                   ValuePtr(prev_world_from_object_trans),
                                   12 * sizeof(float));
                        }
                    }

                    if (lighting_enabled && (obj.comp_mask & CompLightSourceBit)) {
                        const LightSource &light =
                            lights_src[obj.components[CompLightSource]];

                        auto pos = Vec4f{light.offset[0], light.offset[1],
                                         light.offset[2], 1.0f};
                        pos = world_from_object * pos;
                        pos /= pos[3];

                        auto dir =
                            Vec4f{-light.dir[0], -light.dir[1], -light.dir[2], 0.0f};
                        dir = world_from_object * dir;

                        eVisResult res = eVisResult::FullyVisible;

                        for (int k = 0; k < 6 && !skip_check; k++) {
                            const Plane &plane = list.draw_cam.frustum_plane(k);

                            const float dist = plane.n[0] * pos[0] + plane.n[1] * pos[1] +
                                               plane.n[2] * pos[2] + plane.d;

                            if (dist < -light.influence) {
                                res = eVisResult::Invisible;
                                break;
                            } else if (std::abs(dist) < light.influence) {
                                res = eVisResult::PartiallyVisible;
                            }
                        }

                        if (res != eVisResult::Invisible) {
                            litem_to_lsource_.data[litem_to_lsource_.count++] = &light;
                            LightSourceItem &ls =
                                list.light_sources.data[list.light_sources.count++];

                            memcpy(&ls.pos[0], &pos[0], 3 * sizeof(float));
                            ls.radius = light.radius;
                            memcpy(&ls.col[0], &light.col[0], 3 * sizeof(float));
                            ls.shadowreg_index = -1;
                            memcpy(&ls.dir[0], &dir[0], 3 * sizeof(float));
                            ls.spot = light.spot;
                        }
                    }

                    if (decals_enabled && (obj.comp_mask & CompDecalBit)) {
                        const Decal &decal = decals[obj.components[CompDecal]];

                        const Mat4f &view_from_object = decal.view,
                                    &clip_from_view = decal.proj;

                        const Mat4f view_from_world =
                                        view_from_object * object_from_world,
                                    clip_from_world = clip_from_view * view_from_world;

                        const Mat4f world_from_clip = Inverse(clip_from_world);

                        Vec4f bbox_points[] = {REN_UNINITIALIZE_X8(Vec4f)};

                        Vec3f bbox_min = Vec3f{std::numeric_limits<float>::max()},
                              bbox_max = Vec3f{std::numeric_limits<float>::lowest()};

                        for (int k = 0; k < 8; k++) {
                            bbox_points[k] = world_from_clip * ClipFrustumPoints[k];
                            bbox_points[k] /= bbox_points[k][3];

                            bbox_min = Min(bbox_min, Vec3f{bbox_points[k]});
                            bbox_max = Max(bbox_max, Vec3f{bbox_points[k]});
                        }

                        eVisResult res = eVisResult::FullyVisible;

                        for (int p = int(eCamPlane::LeftPlane);
                             p <= int(eCamPlane::FarPlane) && !skip_check; p++) {
                            const Plane &plane = list.draw_cam.frustum_plane(p);

                            int in_count = 8;

                            for (int k = 0; k < 8; k++) {
                                const float dist = plane.n[0] * bbox_points[k][0] +
                                                   plane.n[1] * bbox_points[k][1] +
                                                   plane.n[2] * bbox_points[k][2] +
                                                   plane.d;
                                if (dist < 0.0f) {
                                    in_count--;
                                }
                            }

                            if (in_count == 0) {
                                res = eVisResult::Invisible;
                                break;
                            } else if (in_count != 8) {
                                res = eVisResult::PartiallyVisible;
                            }
                        }

                        if (res != eVisResult::Invisible) {
                            ditem_to_decal_.data[ditem_to_decal_.count++] = &decal;
                            decals_boxes_.data[decals_boxes_.count++] = {bbox_min,
                                                                         bbox_max};

                            const Mat4f clip_from_world_transposed =
                                Transpose(clip_from_world);

                            DecalItem &de = list.decals.data[list.decals.count++];
                            memcpy(&de.mat[0][0], &clip_from_world_transposed[0][0],
                                   12 * sizeof(float));
                            memcpy(&de.diff[0], &decal.diff[0], 4 * sizeof(float));
                            memcpy(&de.norm[0], &decal.norm[0], 4 * sizeof(float));
                            memcpy(&de.spec[0], &decal.spec[0], 4 * sizeof(float));
                        }
                    }

                    if (obj.comp_mask & CompProbeBit) {
                        const LightProbe &probe = probes[obj.components[CompProbe]];

                        auto pos = Vec4f{probe.offset[0], probe.offset[1],
                                         probe.offset[2], 1.0f};
                        pos = world_from_object * pos;
                        pos /= pos[3];

                        ProbeItem &pr = list.probes.data[list.probes.count++];
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
        }
    }

    /**********************************************************************************/
    /*                                SHADOWMAP GATHERING                             */
    /**********************************************************************************/

    const uint64_t shadow_gather_start = Sys::GetTimeUs();

    if (lighting_enabled && scene.root_node != 0xffffffff && shadows_enabled &&
        Length2(list.env.sun_dir) > 0.9f &&
        Length2(list.env.sun_col) > std::numeric_limits<float>::epsilon()) {
        // Reserve space for sun shadow
        int sun_shadow_pos[2] = {0, 0};
        int sun_shadow_res[2];
        if (shadow_splitter_.FindNode(sun_shadow_pos, sun_shadow_res) == -1 ||
            sun_shadow_res[0] != SUN_SHADOW_RES || sun_shadow_res[1] != SUN_SHADOW_RES) {
            shadow_splitter_.Clear();

            sun_shadow_res[0] = SUN_SHADOW_RES;
            sun_shadow_res[1] = SUN_SHADOW_RES;

            const int id = shadow_splitter_.Allocate(sun_shadow_res, sun_shadow_pos);
            assert(id != -1 && sun_shadow_pos[0] == 0 && sun_shadow_pos[1] == 0);
        }

        // Planes, that define shadow map splits
        const float far_planes[] = {
            float(REN_SHAD_CASCADE0_DIST), float(REN_SHAD_CASCADE1_DIST),
            float(REN_SHAD_CASCADE2_DIST), float(REN_SHAD_CASCADE3_DIST)};
        const float near_planes[] = {list.draw_cam.near(), 0.9f * far_planes[0],
                                     0.9f * far_planes[1], 0.9f * far_planes[2]};

        // Reserved positions for sun shadowmap
        const int OneCascadeRes = SUN_SHADOW_RES / 2;
        const int map_positions[][2] = {{0, 0},
                                        {OneCascadeRes, 0},
                                        {0, OneCascadeRes},
                                        {OneCascadeRes, OneCascadeRes}};

        // Choose up vector for shadow camera
        const Vec3f &light_dir = list.env.sun_dir;
        auto cam_up = Vec3f{0.0f, 0.0, 1.0f};
        if (_ABS(light_dir[0]) <= _ABS(light_dir[1]) &&
            _ABS(light_dir[0]) <= _ABS(light_dir[2])) {
            cam_up = Vec3f{1.0f, 0.0, 0.0f};
        } else if (_ABS(light_dir[1]) <= _ABS(light_dir[0]) &&
                   _ABS(light_dir[1]) <= _ABS(light_dir[2])) {
            cam_up = Vec3f{0.0f, 1.0, 0.0f};
        }
        // Calculate side vector of shadow camera
        const Vec3f cam_side = Normalize(Cross(light_dir, cam_up));
        cam_up = Cross(cam_side, light_dir);

        const Vec3f scene_dims =
            scene.nodes[scene.root_node].bbox_max - scene.nodes[scene.root_node].bbox_min;
        const float max_dist = Length(scene_dims);

        const Vec3f view_dir = list.draw_cam.view_dir();

        // Gather drawables for each cascade
        for (int casc = 0; casc < 4; casc++) {
            Camera temp_cam = list.draw_cam;
            temp_cam.Perspective(list.draw_cam.angle(), list.draw_cam.aspect(),
                                 near_planes[casc], far_planes[casc]);
            temp_cam.UpdatePlanes();

            const Mat4f &tmp_cam_view_from_world = temp_cam.view_matrix(),
                        &tmp_cam_clip_from_view = temp_cam.proj_matrix();

            const Mat4f tmp_cam_clip_from_world =
                tmp_cam_clip_from_view * tmp_cam_view_from_world;
            const Mat4f tmp_cam_world_from_clip = Inverse(tmp_cam_clip_from_world);

            Vec3f bounding_center;
            const float bounding_radius = temp_cam.GetBoundingSphere(bounding_center);
            float object_dim_thres = 0.0f;

            Vec3f cam_target = bounding_center;

            { // Snap camera movement to shadow map pixels
                const float move_step = (2 * bounding_radius) / (0.5f * SUN_SHADOW_RES);
                //                      |_shadow map extent_|   |_res of one cascade_|

                // Project target on shadow cam view matrix
                float _dot_f = Dot(cam_target, light_dir),
                      _dot_s = Dot(cam_target, cam_side),
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

            const Vec3f cam_center = cam_target + max_dist * light_dir;

            Camera shadow_cam;
            shadow_cam.SetupView(cam_center, cam_target, cam_up);
            shadow_cam.Orthographic(-bounding_radius, bounding_radius, bounding_radius,
                                    -bounding_radius, 0.0f, max_dist + bounding_radius);
            shadow_cam.UpdatePlanes();

            const Mat4f sh_clip_from_world =
                shadow_cam.proj_matrix() * shadow_cam.view_matrix();

            ShadowList &sh_list = list.shadow_lists.data[list.shadow_lists.count++];

            sh_list.shadow_map_pos[0] = map_positions[casc][0];
            sh_list.shadow_map_pos[1] = map_positions[casc][1];
            sh_list.shadow_map_size[0] = OneCascadeRes;
            sh_list.shadow_map_size[1] = OneCascadeRes;
            sh_list.shadow_batch_start = list.shadow_batches.count;
            sh_list.shadow_batch_count = 0;
            sh_list.cam_near = shadow_cam.near();
            sh_list.cam_far = shadow_cam.far();
            sh_list.bias[0] = scene.env.sun_shadow_bias[0];
            sh_list.bias[1] = scene.env.sun_shadow_bias[1];

            Frustum sh_clip_frustum;

            { // Construct shadow clipping frustum
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

                Vec2i frustum_edges[] = {Vec2i{0, 1}, Vec2i{1, 2}, Vec2i{2, 3},
                                         Vec2i{3, 0}, Vec2i{4, 5}, Vec2i{5, 6},
                                         Vec2i{6, 7}, Vec2i{7, 4}, Vec2i{0, 4},
                                         Vec2i{1, 5}, Vec2i{2, 6}, Vec2i{3, 7}};

                int silhouette_edges[12], silhouette_edges_count = 0;

                for (int i = 0; i < 12; i++) {
                    const int k1 = frustum_edges[i][0], k2 = frustum_edges[i][1];

                    int last_sign = 0;
                    bool is_silhouette = true;

                    for (int k = 0; k < 8; k++) {
                        if (k == k1 || k == k2) {
                            continue;
                        }

                        const float d =
                            (fr_points_proj[k][0] - fr_points_proj[k1][0]) *
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

                        const float x_diff0 =
                            (fr_points_proj[k1][0] - fr_points_proj[k2][0]);
                        const bool is_vertical0 =
                            _ABS(x_diff0) < std::numeric_limits<float>::epsilon();
                        const float slope0 = is_vertical0 ? 0.0f
                                                          : (fr_points_proj[k1][1] -
                                                             fr_points_proj[k2][1]) /
                                                                x_diff0,
                                    b0 = is_vertical0 ? fr_points_proj[k1][0]
                                                      : (fr_points_proj[k1][1] -
                                                         slope0 * fr_points_proj[k1][0]);

                        // Check if it is a duplicate
                        for (int k = 0; k < silhouette_edges_count - 1; k++) {
                            const int j = silhouette_edges[k];

                            const float x_diff1 =
                                (fr_points_proj[frustum_edges[j][0]][0] -
                                 fr_points_proj[frustum_edges[j][1]][0]);
                            const bool is_vertical1 =
                                _ABS(x_diff1) < std::numeric_limits<float>::epsilon();
                            const float
                                slope1 = is_vertical1
                                             ? 0.0f
                                             : (fr_points_proj[frustum_edges[j][0]][1] -
                                                fr_points_proj[frustum_edges[j][1]][1]) /
                                                   x_diff1,
                                b1 = is_vertical1
                                         ? fr_points_proj[frustum_edges[j][0]][0]
                                         : fr_points_proj[frustum_edges[j][0]][1] -
                                               slope1 *
                                                   fr_points_proj[frustum_edges[j][0]][0];

                            if (is_vertical1 == is_vertical0 &&
                                _ABS(slope1 - slope0) < 0.001f &&
                                _ABS(b1 - b0) < 0.001f) {
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

                    const auto p1 = Vec3f{frustum_points[edge[0]]},
                               p2 = Vec3f{frustum_points[edge[1]]};

                    // Extrude edge in direction of light
                    const Vec3f p3 = p2 + light_dir;

                    // Construct clipping plane
                    sh_clip_frustum.planes[i] = Plane{p1, p2, p3};

                    // Store projected points for debugging
                    sh_list.view_frustum_outline[2 * i + 0] = fr_points_proj[edge[0]];
                    sh_list.view_frustum_outline[2 * i + 1] = fr_points_proj[edge[1]];

                    // Find region for scissor test
                    const auto p1i =
                        Vec2i{sh_list.shadow_map_pos[0] +
                                  int((0.5f * sh_list.view_frustum_outline[2 * i + 0][0] +
                                       0.5f) *
                                      (float)sh_list.shadow_map_size[0]),
                              sh_list.shadow_map_pos[1] +
                                  int((0.5f * sh_list.view_frustum_outline[2 * i + 0][1] +
                                       0.5f) *
                                      (float)sh_list.shadow_map_size[1])};

                    const auto p2i =
                        Vec2i{sh_list.shadow_map_pos[0] +
                                  int((0.5f * sh_list.view_frustum_outline[2 * i + 1][0] +
                                       0.5f) *
                                      (float)sh_list.shadow_map_size[0]),
                              sh_list.shadow_map_pos[1] +
                                  int((0.5f * sh_list.view_frustum_outline[2 * i + 1][1] +
                                       0.5f) *
                                      (float)sh_list.shadow_map_size[1])};

                    const auto scissor_margin = Vec2i{2}; // shadow uses 5x5 filter

                    scissor_min =
                        Min(scissor_min, Min(p1i - scissor_margin, p2i - scissor_margin));
                    scissor_max =
                        Max(scissor_max, Max(p1i + scissor_margin, p2i + scissor_margin));
                }

                scissor_min = Max(scissor_min, Vec2i{0});
                scissor_max =
                    Min(scissor_max, Vec2i{map_positions[casc][0] + OneCascadeRes,
                                           map_positions[casc][1] + OneCascadeRes});

                sh_list.scissor_test_pos[0] = scissor_min[0];
                sh_list.scissor_test_pos[1] = scissor_min[1];
                sh_list.scissor_test_size[0] = scissor_max[0] - scissor_min[0];
                sh_list.scissor_test_size[1] = scissor_max[1] - scissor_min[1];

                // add near and far planes
                sh_clip_frustum.planes[sh_clip_frustum.planes_count++] =
                    shadow_cam.frustum_plane(eCamPlane::NearPlane);
                sh_clip_frustum.planes[sh_clip_frustum.planes_count++] =
                    shadow_cam.frustum_plane(eCamPlane::FarPlane);
            }

            ShadowMapRegion &reg = list.shadow_regions.data[list.shadow_regions.count++];

            reg.transform = Vec4f{float(sh_list.shadow_map_pos[0]) / SHADOWMAP_WIDTH,
                                  float(sh_list.shadow_map_pos[1]) / SHADOWMAP_HEIGHT,
                                  float(sh_list.shadow_map_size[0]) / SHADOWMAP_WIDTH,
                                  float(sh_list.shadow_map_size[1]) / SHADOWMAP_HEIGHT};

            const float cached_dist = Distance(list.draw_cam.world_position(),
                                               sun_shadow_cache_[casc].view_pos),
                        cached_dir_dist =
                            Distance(view_dir, sun_shadow_cache_[casc].view_dir);

            // discard cached cascade if view change was significant
            sun_shadow_cache_[casc].valid &=
                (cached_dist < 1.0f && cached_dir_dist < 0.1f);

            const uint8_t pattern_bit = (1u << uint8_t(frame_counter_ % 8));
            const bool should_update = (pattern_bit & SunShadowUpdatePattern[casc]) != 0;

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

#if 0
            if (shadow_cam.CheckFrustumVisibility(cam.world_position()) != eVisResult::FullyVisible) {
                // Check if shadowmap frustum is visible to main camera
                
                Mat4f world_from_clip = Inverse(sh_clip_from_world);

                Vec4f frustum_points[] = {
                    { -1.0f, -1.0f, 0.0f, 1.0f },
                    {  1.0f, -1.0f, 0.0f, 1.0f },
                    { -1.0f, -1.0f, 1.0f, 1.0f },
                    {  1.0f, -1.0f, 1.0f, 1.0f },

                    { -1.0f,  1.0f, 0.0f, 1.0f },
                    {  1.0f,  1.0f, 0.0f, 1.0f },
                    { -1.0f,  1.0f, 1.0f, 1.0f },
                    {  1.0f,  1.0f, 1.0f, 1.0f }
                };

                for (int k = 0; k < 8; k++) {
                    frustum_points[k] = world_from_clip * frustum_points[k];
                    frustum_points[k] /= frustum_points[k][3];
                }

                SWcull_surf surf;

                surf.type = SW_OCCLUDEE;
                surf.prim_type = SW_TRIANGLES;
                surf.index_type = SW_UNSIGNED_BYTE;
                surf.attribs = &frustum_points[0][0];
                surf.indices = &bbox_indices[0];
                surf.stride = 4 * sizeof(float);
                surf.count = 36;
                surf.base_vertex = 0;
                surf.xform = ValuePtr(clip_from_identity);
                surf.dont_skip = nullptr;

                swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                if (surf.visible == 0) {
                    continue;
                }
            }
#endif

            stack_size = 0;
            stack[stack_size++] = scene.root_node;

            while (stack_size) {
                const uint32_t cur = stack[--stack_size] & IndexBits;
                uint32_t skip_check = stack[stack_size] & SkipCheckBit;
                const bvh_node_t *n = &scene.nodes[cur];

                if (!skip_check) {
                    eVisResult res =
                        sh_clip_frustum.CheckVisibility(n->bbox_min, n->bbox_max);
                    if (res == eVisResult::Invisible) {
                        continue;
                    } else if (res == eVisResult::FullyVisible) {
                        skip_check = SkipCheckBit;
                    }
                }

                if (!n->prim_count) {
                    stack[stack_size++] = skip_check | n->left_child;
                    stack[stack_size++] = skip_check | n->right_child;
                } else {
                    const SceneObject &obj = scene.objects[n->prim_index];

                    const uint32_t drawable_flags = CompDrawableBit | CompTransformBit;
                    if ((obj.comp_mask & drawable_flags) == drawable_flags) {
                        const Transform &tr = transforms[obj.components[CompTransform]];
                        const Drawable &dr = drawables[obj.components[CompDrawable]];
                        if ((dr.vis_mask &
                             uint32_t(Drawable::eDrVisibility::VisShadow)) == 0) {
                            continue;
                        }

                        if (!skip_check && sh_clip_frustum.CheckVisibility(
                                               tr.bbox_min_ws, tr.bbox_max_ws) ==
                                               eVisResult::Invisible) {
                            continue;
                        }

                        if ((tr.bbox_max_ws[0] - tr.bbox_min_ws[0]) < object_dim_thres &&
                            (tr.bbox_max_ws[1] - tr.bbox_min_ws[1]) < object_dim_thres &&
                            (tr.bbox_max_ws[2] - tr.bbox_min_ws[2]) < object_dim_thres) {
                            continue;
                        }

                        const Mat4f &world_from_object = tr.mat,
                                    &object_from_world = tr.inv_mat;
                        const Mesh *mesh = dr.mesh.get();

                        if (proc_objects_.data[n->prim_index].instance_index ==
                            0xffffffff) {
                            proc_objects_.data[n->prim_index].instance_index =
                                list.instances.count;

                            const Mat4f world_from_object_trans =
                                Transpose(world_from_object);

                            InstanceData &instance =
                                list.instances.data[list.instances.count++];
                            memcpy(&instance.model_matrix[0][0],
                                   ValuePtr(world_from_object_trans), 12 * sizeof(float));

                            if (obj.comp_mask & CompVegStateBit) {
                                const VegState &vs = vegs[obj.components[CompVegState]];
                                const Mat4f object_from_world =
                                    Inverse(world_from_object);
                                __init_wind_params(vs, list.env, object_from_world,
                                                   instance);
                            }
                        }

                        if (proc_objects_.data[n->prim_index].base_vertex == 0xffffffff) {
                            proc_objects_.data[n->prim_index].base_vertex =
                                mesh->attribs_buf1().offset / 16;

                            if (obj.comp_mask & CompAnimStateBit) {
                                const AnimState &as =
                                    anims[obj.components[CompAnimState]];
                                proc_objects_.data[n->prim_index].base_vertex =
                                    __push_skeletal_mesh(skinned_buf_vtx_offset, as, mesh,
                                                         list);
                            }
                        }

                        const TriGroup *grp = &mesh->group(0);
                        while (grp->offset != -1) {
                            const Material *mat = grp->mat.get();
                            const uint32_t mat_flags = mat->flags();

                            if ((mat_flags & uint32_t(eMaterialFlags::AlphaBlend)) == 0) {
                                DepthDrawBatch &batch =
                                    list.shadow_batches.data[list.shadow_batches.count++];

                                batch.mat_id =
                                    (mat_flags & uint32_t(eMaterialFlags::AlphaTest))
                                        ? (uint32_t)grp->mat.index()
                                        : 0;

                                batch.type_bits = DepthDrawBatch::TypeSimple;

                                // we do not care if it is skinned
                                if (obj.comp_mask & CompVegStateBit) {
                                    batch.type_bits = DepthDrawBatch::TypeVege;
                                }

                                batch.alpha_test_bit =
                                    (mat_flags & uint32_t(eMaterialFlags::AlphaTest)) ? 1
                                                                                      : 0;
                                batch.moving_bit = 0;
                                batch.two_sided_bit =
                                    (mat_flags & uint32_t(eMaterialFlags::TwoSided)) ? 1
                                                                                     : 0;
                                batch.indices_offset =
                                    (mesh->indices_buf().offset + grp->offset) /
                                    sizeof(uint32_t);
                                batch.base_vertex =
                                    proc_objects_.data[n->prim_index].base_vertex;
                                batch.indices_count = grp->num_indices;
                                batch.instance_indices[0] =
                                    proc_objects_.data[n->prim_index].instance_index;
                                batch.instance_count = 1;
                            }
                            ++grp;
                        }
                    }
                }
            }

            sh_list.shadow_batch_count =
                list.shadow_batches.count - sh_list.shadow_batch_start;
        }
    }

    const Vec3f cam_pos = cam.world_position();

    for (int i = 0; i < int(list.light_sources.count) && shadows_enabled; i++) {
        LightSourceItem &l = list.light_sources.data[i];
        const LightSource *ls = litem_to_lsource_.data[i];

        if (!ls->cast_shadow) {
            continue;
        }

        const auto light_center = Vec3f{l.pos[0], l.pos[1], l.pos[2]};
        const float distance = Distance(light_center, cam_pos);

        const int ShadowResolutions[][2] = {{512, 512}, {256, 256}, {128, 128}, {64, 64}};

        // choose resolution based on distance
        int res_index = _MIN(int(distance * 0.02f), 4);

        ShadReg *region = nullptr;

        for (int j = 0; j < (int)allocated_shadow_regions_.count; j++) {
            ShadReg &reg = allocated_shadow_regions_.data[j];

            if (reg.ls == ls) {
                if (reg.size[0] != ShadowResolutions[res_index][0] ||
                    reg.size[1] != ShadowResolutions[res_index][1]) {
                    // free and reallocate region
                    shadow_splitter_.Free(reg.pos);
                    reg =
                        allocated_shadow_regions_.data[--allocated_shadow_regions_.count];
                } else {
                    region = &reg;
                }
                break;
            }
        }

        // try to allocate best resolution possible
        for (; res_index < 4 && !region; res_index++) {
            int pos[2];
            int node = shadow_splitter_.Allocate(ShadowResolutions[res_index], pos);
            if (node == -1 && allocated_shadow_regions_.count) {
                ShadReg *oldest = &allocated_shadow_regions_.data[0];
                for (int j = 0; j < (int)allocated_shadow_regions_.count; j++) {
                    if (allocated_shadow_regions_.data[j].last_visible <
                        oldest->last_visible) {
                        oldest = &allocated_shadow_regions_.data[j];
                    }
                }
                if ((scene.update_counter - oldest->last_visible) > 10) {
                    // kick out one of old cached region
                    shadow_splitter_.Free(oldest->pos);
                    *oldest =
                        allocated_shadow_regions_.data[--allocated_shadow_regions_.count];
                    // try again to insert
                    node = shadow_splitter_.Allocate(ShadowResolutions[res_index], pos);
                }
            }
            if (node != -1) {
                region =
                    &allocated_shadow_regions_.data[allocated_shadow_regions_.count++];
                region->ls = ls;
                region->pos[0] = pos[0];
                region->pos[1] = pos[1];
                region->size[0] = ShadowResolutions[res_index][0];
                region->size[1] = ShadowResolutions[res_index][1];
                region->last_update = region->last_visible = 0xffffffff;
            }
        }

        if (region) {
            const auto light_dir = Vec3f{-l.dir[0], -l.dir[1], -l.dir[2]};

            auto light_up = Vec3f{0.0f, 0.0, 1.0f};
            if (_ABS(light_dir[0]) <= _ABS(light_dir[1]) &&
                _ABS(light_dir[0]) <= _ABS(light_dir[2])) {
                light_up = Vec3f{1.0f, 0.0, 0.0f};
            } else if (_ABS(light_dir[1]) <= _ABS(light_dir[0]) &&
                       _ABS(light_dir[1]) <= _ABS(light_dir[2])) {
                light_up = Vec3f{0.0f, 1.0, 0.0f};
            }

            float light_angle = 2.0f * std::acos(l.spot) * 180.0f / Pi<float>();

            Camera shadow_cam;
            shadow_cam.SetupView(light_center, light_center + light_dir, light_up);
            shadow_cam.Perspective(light_angle, 1.0f, 0.1f, ls->influence);
            shadow_cam.UpdatePlanes();

            // TODO: Check visibility of shadow frustum

            const Mat4f clip_from_world = shadow_cam.proj_matrix() * shadow_cam.view_matrix();

            ShadowList &sh_list = list.shadow_lists.data[list.shadow_lists.count++];

            sh_list.shadow_map_pos[0] = sh_list.scissor_test_pos[0] = region->pos[0];
            sh_list.shadow_map_pos[1] = sh_list.scissor_test_pos[1] = region->pos[1];
            sh_list.shadow_map_size[0] = sh_list.scissor_test_size[0] = region->size[0];
            sh_list.shadow_map_size[1] = sh_list.scissor_test_size[1] = region->size[1];
            sh_list.shadow_batch_start = list.shadow_batches.count;
            sh_list.shadow_batch_count = 0;
            sh_list.cam_near = region->cam_near = shadow_cam.near();
            sh_list.cam_far = region->cam_far = shadow_cam.far();
            sh_list.view_frustum_outline_count = 0;
            sh_list.bias[0] = ls->shadow_bias[0];
            sh_list.bias[1] = ls->shadow_bias[1];

            l.shadowreg_index = (int)list.shadow_regions.count;
            ShadowMapRegion &reg = list.shadow_regions.data[list.shadow_regions.count++];

            reg.transform = Vec4f{float(sh_list.shadow_map_pos[0]) / SHADOWMAP_WIDTH,
                                  float(sh_list.shadow_map_pos[1]) / SHADOWMAP_HEIGHT,
                                  float(sh_list.shadow_map_size[0]) / SHADOWMAP_WIDTH,
                                  float(sh_list.shadow_map_size[1]) / SHADOWMAP_HEIGHT};
            reg.clip_from_world = clip_from_world;

            bool light_sees_dynamic_objects = false;

            const uint32_t skip_check_bit = (1u << 31u);
            const uint32_t index_bits = ~skip_check_bit;

            stack_size = 0;
            stack[stack_size++] = scene.root_node;

            while (stack_size) {
                const uint32_t cur = stack[--stack_size] & index_bits;
                uint32_t skip_check = stack[stack_size] & skip_check_bit;
                const bvh_node_t *n = &scene.nodes[cur];

                const eVisResult res =
                    shadow_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                if (res == eVisResult::Invisible) {
                    continue;
                } else if (res == eVisResult::FullyVisible) {
                    skip_check = skip_check_bit;
                }

                if (!n->prim_count) {
                    stack[stack_size++] = skip_check | n->left_child;
                    stack[stack_size++] = skip_check | n->right_child;
                } else {
                    const auto &obj = scene.objects[n->prim_index];

                    const uint32_t drawable_flags = CompDrawableBit | CompTransformBit;
                    if ((obj.comp_mask & drawable_flags) == drawable_flags) {
                        const Transform &tr = transforms[obj.components[CompTransform]];

                        if (!skip_check && shadow_cam.CheckFrustumVisibility(
                                               tr.bbox_min_ws, tr.bbox_max_ws) ==
                                               eVisResult::Invisible) {
                            continue;
                        }

                        const Mat4f &world_from_object = tr.mat,
                                    &object_from_world = tr.inv_mat;
                        const Drawable &dr = drawables[obj.components[CompDrawable]];
                        if ((dr.vis_mask &
                             uint32_t(Drawable::eDrVisibility::VisShadow)) == 0) {
                            continue;
                        }

                        const Mesh *mesh = dr.mesh.get();

                        Mat4f world_from_object_trans = Transpose(world_from_object);

                        if (proc_objects_.data[n->prim_index].instance_index ==
                            0xffffffff) {
                            proc_objects_.data[n->prim_index].instance_index =
                                list.instances.count;

                            InstanceData &instance =
                                list.instances.data[list.instances.count++];
                            memcpy(&instance.model_matrix[0][0],
                                   ValuePtr(world_from_object_trans), 12 * sizeof(float));

                            if (obj.comp_mask & CompVegStateBit) {
                                const VegState &vs = vegs[obj.components[CompVegState]];
                                __init_wind_params(vs, list.env, object_from_world,
                                                   instance);
                            }
                        }

                        if (proc_objects_.data[n->prim_index].base_vertex == 0xffffffff) {
                            proc_objects_.data[n->prim_index].base_vertex =
                                mesh->attribs_buf1().offset / 16;

                            if (obj.comp_mask & CompAnimStateBit) {
                                const AnimState &as =
                                    anims[obj.components[CompAnimState]];
                                proc_objects_.data[n->prim_index].base_vertex =
                                    __push_skeletal_mesh(skinned_buf_vtx_offset, as, mesh,
                                                         list);
                            }
                        }

                        const TriGroup *grp = &mesh->group(0);
                        while (grp->offset != -1) {
                            const Material *mat = grp->mat.get();
                            const uint32_t mat_flags = mat->flags();

                            if ((mat_flags & uint32_t(eMaterialFlags::AlphaBlend)) == 0) {
                                DepthDrawBatch &batch =
                                    list.shadow_batches.data[list.shadow_batches.count++];

                                batch.mat_id =
                                    (mat_flags & uint32_t(eMaterialFlags::AlphaTest))
                                        ? (uint32_t)grp->mat.index()
                                        : 0;

                                batch.type_bits = DepthDrawBatch::TypeSimple;

                                // we do not care if it is skinned
                                if (obj.comp_mask & CompVegStateBit) {
                                    batch.type_bits = DepthDrawBatch::TypeVege;
                                }

                                batch.alpha_test_bit =
                                    (mat_flags & uint32_t(eMaterialFlags::AlphaTest)) ? 1
                                                                                      : 0;
                                batch.moving_bit = 0;
                                batch.two_sided_bit =
                                    (mat_flags & uint32_t(eMaterialFlags::TwoSided)) ? 1
                                                                                     : 0;
                                batch.indices_offset =
                                    (mesh->indices_buf().offset + grp->offset) /
                                    sizeof(uint32_t);
                                batch.base_vertex =
                                    proc_objects_.data[n->prim_index].base_vertex;
                                batch.indices_count = grp->num_indices;
                                batch.instance_indices[0] =
                                    proc_objects_.data[n->prim_index].instance_index;
                                batch.instance_count = 1;
                            }
                            ++grp;
                        }
                    }

                    if ((obj.last_change_mask & CompTransformBit) ||
                        (obj.comp_mask & CompVegStateBit)) {
                        light_sees_dynamic_objects = true;
                    }
                }
            }

            if (!light_sees_dynamic_objects && region->last_update != 0xffffffff &&
                (scene.update_counter - region->last_update > 2)) {
                // nothing was changed within the last two frames, discard added batches
                list.shadow_batches.count = sh_list.shadow_batch_start;
                sh_list.shadow_batch_count = 0;
            } else {
                if (light_sees_dynamic_objects || region->last_update == 0xffffffff) {
                    region->last_update = scene.update_counter;
                }
                sh_list.shadow_batch_count =
                    list.shadow_batches.count - sh_list.shadow_batch_start;
            }

            region->last_visible = scene.update_counter;
        }
    }

    if (shadows_enabled && (list.render_flags & DebugShadow)) {
        list.cached_shadow_regions.count = 0;
        for (int i = 0; i < (int)allocated_shadow_regions_.count; i++) {
            const ShadReg &r = allocated_shadow_regions_.data[i];
            if (r.last_visible != scene.update_counter) {
                list.cached_shadow_regions.data[list.cached_shadow_regions.count++] = r;
            }
        }
    }

    /***********************************************************************************/
    /*                                OPTIMIZING DRAW LISTS                            */
    /***********************************************************************************/

    const uint64_t drawables_sort_start = Sys::GetTimeUs();

    // Sort drawables to optimize state switches

    if (zfill_enabled) {
        temp_sort_spans_32_[0].count = list.zfill_batches.count;
        temp_sort_spans_32_[1].count = list.zfill_batches.count;
        list.zfill_batch_indices.count = list.zfill_batches.count;
        uint32_t spans_count = 0;

        // compress batches into spans with indentical key values (makes sorting faster)
        for (uint32_t start = 0, end = 1; end <= list.zfill_batches.count; end++) {
            if (end == list.zfill_batches.count ||
                (list.zfill_batches.data[start].sort_key !=
                 list.zfill_batches.data[end].sort_key)) {
                temp_sort_spans_32_[0].data[spans_count].key =
                    list.zfill_batches.data[start].sort_key;
                temp_sort_spans_32_[0].data[spans_count].base = start;
                temp_sort_spans_32_[0].data[spans_count++].count = end - start;
                start = end;
            }
        }

        RadixSort_LSB<SortSpan32>(temp_sort_spans_32_[0].data,
                                  temp_sort_spans_32_[0].data + spans_count,
                                  temp_sort_spans_32_[1].data);

        // decompress sorted spans
        size_t counter = 0;
        for (uint32_t i = 0; i < spans_count; i++) {
            for (uint32_t j = 0; j < temp_sort_spans_32_[0].data[i].count; j++) {
                list.zfill_batch_indices.data[counter++] =
                    temp_sort_spans_32_[0].data[i].base + j;
            }
        }

        // Merge similar batches
        for (uint32_t start = 0, end = 1; end <= list.zfill_batch_indices.count; end++) {
            if ((end - start) >= REN_MAX_BATCH_SIZE ||
                end == list.zfill_batch_indices.count ||
                list.zfill_batches.data[list.zfill_batch_indices.data[start]].sort_key !=
                    list.zfill_batches.data[list.zfill_batch_indices.data[end]]
                        .sort_key) {

                DepthDrawBatch &b1 =
                    list.zfill_batches.data[list.zfill_batch_indices.data[start]];
                for (uint32_t i = start + 1; i < end; i++) {
                    DepthDrawBatch &b2 =
                        list.zfill_batches.data[list.zfill_batch_indices.data[i]];

                    if (b1.base_vertex == b2.base_vertex &&
                        b1.instance_count + b2.instance_count <= REN_MAX_BATCH_SIZE) {
                        memcpy(&b1.instance_indices[b1.instance_count],
                               &b2.instance_indices[0], b2.instance_count * sizeof(int));
                        b1.instance_count += b2.instance_count;
                        b2.instance_count = 0;
                    }
                }

                start = end;
            }
        }
    }

    temp_sort_spans_64_[0].count = list.main_batches.count;
    temp_sort_spans_64_[1].count = list.main_batches.count;
    list.main_batch_indices.count = list.main_batches.count;
    uint32_t spans_count = 0;

    // compress batches into spans with indentical key values (makes sorting faster)
    for (uint32_t start = 0, end = 1; end <= list.main_batches.count; end++) {
        if (end == list.main_batches.count || (list.main_batches.data[start].sort_key !=
                                               list.main_batches.data[end].sort_key)) {
            temp_sort_spans_64_[0].data[spans_count].key =
                list.main_batches.data[start].sort_key;
            temp_sort_spans_64_[0].data[spans_count].base = start;
            temp_sort_spans_64_[0].data[spans_count++].count = end - start;
            start = end;
        }
    }

    RadixSort_LSB<SortSpan64>(temp_sort_spans_64_[0].data,
                              temp_sort_spans_64_[0].data + spans_count,
                              temp_sort_spans_64_[1].data);

    // decompress sorted spans
    size_t counter = 0;
    for (uint32_t i = 0; i < spans_count; i++) {
        for (uint32_t j = 0; j < temp_sort_spans_64_[0].data[i].count; j++) {
            list.main_batch_indices.data[counter++] =
                temp_sort_spans_64_[0].data[i].base + j;
        }
    }

    // Merge similar batches
    for (uint32_t start = 0, end = 1; end <= list.main_batch_indices.count; end++) {
        if ((end - start) >= REN_MAX_BATCH_SIZE || end == list.main_batch_indices.count ||
            list.main_batches.data[list.main_batch_indices.data[start]].sort_key !=
                list.main_batches.data[list.main_batch_indices.data[end]].sort_key) {

            MainDrawBatch &b1 =
                list.main_batches.data[list.main_batch_indices.data[start]];
            for (uint32_t i = start + 1; i < end; i++) {
                MainDrawBatch &b2 =
                    list.main_batches.data[list.main_batch_indices.data[i]];

                if (b1.base_vertex == b2.base_vertex &&
                    b1.instance_count + b2.instance_count <= REN_MAX_BATCH_SIZE) {
                    memcpy(&b1.instance_indices[b1.instance_count],
                           &b2.instance_indices[0], b2.instance_count * sizeof(int));
                    b1.instance_count += b2.instance_count;
                    b2.instance_count = 0;
                }
            }

            start = end;
        }
    }

    list.shadow_batch_indices.count = list.shadow_batches.count;

    uint32_t sh_batch_indices_counter = 0;

    for (int i = 0; i < (int)list.shadow_lists.count; i++) {
        ShadowList &sh_list = list.shadow_lists.data[i];

        const uint32_t shadow_batch_end =
            sh_list.shadow_batch_start + sh_list.shadow_batch_count;

        temp_sort_spans_32_[0].count = sh_list.shadow_batch_count;
        temp_sort_spans_32_[1].count = sh_list.shadow_batch_count;
        uint32_t spans_count = 0;

        // compress batches into spans with indentical key values (makes sorting faster)
        for (uint32_t start = sh_list.shadow_batch_start,
                      end = sh_list.shadow_batch_start + 1;
             end <= shadow_batch_end; end++) {
            if (end == shadow_batch_end || (list.shadow_batches.data[start].sort_key !=
                                            list.shadow_batches.data[end].sort_key)) {
                temp_sort_spans_32_[0].data[spans_count].key =
                    list.shadow_batches.data[start].sort_key;
                temp_sort_spans_32_[0].data[spans_count].base = start;
                temp_sort_spans_32_[0].data[spans_count++].count = end - start;
                start = end;
            }
        }

        RadixSort_LSB<SortSpan32>(temp_sort_spans_32_[0].data,
                                  temp_sort_spans_32_[0].data + spans_count,
                                  temp_sort_spans_32_[1].data);

        // decompress sorted spans
        for (uint32_t i = 0; i < spans_count; i++) {
            for (uint32_t j = 0; j < temp_sort_spans_32_[0].data[i].count; j++) {
                list.shadow_batch_indices.data[sh_batch_indices_counter++] =
                    temp_sort_spans_32_[0].data[i].base + j;
            }
        }
        assert(sh_batch_indices_counter == shadow_batch_end);

        // Merge similar batches
        for (uint32_t start = sh_list.shadow_batch_start,
                      end = sh_list.shadow_batch_start + 1;
             end <= shadow_batch_end; end++) {
            if ((end - start) >= REN_MAX_BATCH_SIZE || end == shadow_batch_end ||
                list.shadow_batches.data[list.shadow_batch_indices.data[start]]
                        .sort_key !=
                    list.shadow_batches.data[list.shadow_batch_indices.data[end]]
                        .sort_key) {

                DepthDrawBatch &b1 =
                    list.shadow_batches.data[list.shadow_batch_indices.data[start]];
                for (uint32_t i = start + 1; i < end; i++) {
                    DepthDrawBatch &b2 =
                        list.shadow_batches.data[list.shadow_batch_indices.data[i]];

                    if (b1.base_vertex == b2.base_vertex &&
                        b1.instance_count + b2.instance_count <= REN_MAX_BATCH_SIZE) {
                        memcpy(&b1.instance_indices[b1.instance_count],
                               &b2.instance_indices[0], b2.instance_count * sizeof(int));
                        b1.instance_count += b2.instance_count;
                        b2.instance_count = 0;
                    }
                }

                start = end;
            }
        }
    }

    /**********************************************************************************/
    /*                                ASSIGNING TO CLUSTERS                           */
    /**********************************************************************************/

    const uint64_t items_assignment_start = Sys::GetTimeUs();

    if (list.light_sources.count || list.decals.count || list.probes.count) {
        list.draw_cam.ExtractSubFrustums(REN_GRID_RES_X, REN_GRID_RES_Y, REN_GRID_RES_Z,
                                         temp_sub_frustums_.data);

        std::future<void> futures[REN_GRID_RES_Z];
        std::atomic_int a_items_count = {};

        for (int i = 0; i < REN_GRID_RES_Z; i++) {
            futures[i] = threads_->enqueue(
                GatherItemsForZSlice_Job, i, temp_sub_frustums_.data, decals_boxes_.data,
                litem_to_lsource_.data, std::ref(list), std::ref(a_items_count));
        }

        for (std::future<void> &fut : futures) {
            fut.wait();
        }

        list.items.count = std::min(a_items_count.load(), REN_MAX_ITEMS_TOTAL);
    } else {
        CellData _dummy = {};
        std::fill(list.cells.data, list.cells.data + REN_CELLS_COUNT, _dummy);
        list.items.count = 0;
    }

    if ((list.render_flags & (EnableCulling | DebugCulling)) ==
        (EnableCulling | DebugCulling)) {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;

        int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
        depth_pixels_[1].resize(w * h * 4);
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                float z = cull_ctx_.zbuf.depth[(h - y - 1) * w + x];
                z = (2.0f * NEAR_CLIP) /
                    (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                depth_pixels_[1][4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                depth_pixels_[1][4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                depth_pixels_[1][4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                depth_pixels_[1][4 * (y * w + x) + 3] = 255;
            }
        }

        depth_tiles_[1].resize(w * h * 4);
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                const SWzrange *zr = swZbufGetTileRange(&cull_ctx_.zbuf, x, (h - y - 1));

                float z = zr->min;
                z = (2.0f * NEAR_CLIP) /
                    (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                depth_tiles_[1][4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                depth_tiles_[1][4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                depth_tiles_[1][4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                depth_tiles_[1][4 * (y * w + x) + 3] = 255;
            }
        }
    }

    uint64_t iteration_end = Sys::GetTimeUs();

    if (list.render_flags & EnableTimers) {
        list.frontend_info.start_timepoint_us = iteration_start;
        list.frontend_info.end_timepoint_us = iteration_end;
        list.frontend_info.occluders_time_us =
            uint32_t(main_gather_start - occluders_start);
        list.frontend_info.main_gather_time_us =
            uint32_t(shadow_gather_start - main_gather_start);
        list.frontend_info.shadow_gather_time_us =
            uint32_t(drawables_sort_start - shadow_gather_start);
        list.frontend_info.drawables_sort_time_us =
            uint32_t(items_assignment_start - drawables_sort_start);
        list.frontend_info.items_assignment_time_us =
            uint32_t(iteration_end - items_assignment_start);
    }
}

void Renderer::GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums,
                                        const BBox *decals_boxes,
                                        const LightSource *const *litem_to_lsource,
                                        DrawList &list, std::atomic_int &items_count) {
    using namespace RendererInternal;
    using namespace Ren;

    const float epsilon = 0.001f;

    const int frustums_per_slice = REN_GRID_RES_X * REN_GRID_RES_Y;
    const int base_index = slice * frustums_per_slice;
    const Frustum *first_sf = &sub_frustums[base_index];

    // Reset cells information for slice
    for (int s = 0; s < frustums_per_slice; s++) {
        list.cells.data[base_index + s] = {};
    }

    // Gather to local list first
    ItemData local_items[REN_GRID_RES_X * REN_GRID_RES_Y][REN_MAX_ITEMS_PER_CELL];

    for (int j = 0; j < int(list.light_sources.count); j++) {
        const LightSourceItem &l = list.light_sources.data[j];
        const float radius = litem_to_lsource[j]->radius;
        const float influence = litem_to_lsource[j]->influence;
        const float cap_radius = litem_to_lsource[j]->cap_radius;

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if light is inside of a whole z-slice
        for (int k = int(eCamPlane::NearPlane); k <= int(eCamPlane::FarPlane); k++) {
            const float *p_n = ValuePtr(first_sf->planes[k].n);
            const float p_d = first_sf->planes[k].d;

            float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
            if (dist < -influence) {
                visible_to_slice = eVisResult::Invisible;
            } else if (l.spot > epsilon) {
                const float dn[3] = _CROSS(l.dir, p_n);
                const float m[3] = _CROSS(l.dir, dn);

                const float Q[3] = {l.pos[0] - influence * l.dir[0] - cap_radius * m[0],
                                    l.pos[1] - influence * l.dir[1] - cap_radius * m[1],
                                    l.pos[2] - influence * l.dir[2] - cap_radius * m[2]};

                if (dist < -radius &&
                    p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d < -epsilon) {
                    visible_to_slice = eVisResult::Invisible;
                }
            }
        }

        // Skip light for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice;
             row_offset += REN_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if light is inside of grid line
            for (int k = int(eCamPlane::TopPlane); k <= int(eCamPlane::BottomPlane);
                 k++) {
                const float *p_n = ValuePtr(first_line_sf->planes[k].n);
                const float p_d = first_line_sf->planes[k].d;

                float dist =
                    p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
                if (dist < -influence) {
                    visible_to_line = eVisResult::Invisible;
                } else if (l.spot > epsilon) {
                    const float dn[3] = _CROSS(l.dir, p_n);
                    const float m[3] = _CROSS(l.dir, dn);

                    const float Q[3] = {
                        l.pos[0] - influence * l.dir[0] - cap_radius * m[0],
                        l.pos[1] - influence * l.dir[1] - cap_radius * m[1],
                        l.pos[2] - influence * l.dir[2] - cap_radius * m[2]};

                    const float val = p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d;

                    if (dist < -radius && val < -epsilon) {
                        visible_to_line = eVisResult::Invisible;
                    }
                }
            }

            // Skip light for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::LeftPlane); k <= int(eCamPlane::RightPlane);
                     k++) {
                    const float *p_n = ValuePtr(sf->planes[k].n);
                    const float p_d = sf->planes[k].d;

                    float dist =
                        p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
                    if (dist < -influence) {
                        res = eVisResult::Invisible;
                    } else if (l.spot > epsilon) {
                        const float dn[3] = _CROSS(l.dir, p_n);
                        const float m[3] = _CROSS(l.dir, dn);

                        const float Q[3] = {
                            l.pos[0] - influence * l.dir[0] - cap_radius * m[0],
                            l.pos[1] - influence * l.dir[1] - cap_radius * m[1],
                            l.pos[2] - influence * l.dir[2] - cap_radius * m[2]};

                        if (dist < -radius &&
                            p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d <
                                -epsilon) {
                            res = eVisResult::Invisible;
                        }
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = list.cells.data[index];
                    if (cell.light_count < REN_MAX_LIGHTS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.light_count]
                            .light_index = (uint16_t)j;
                        cell.light_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < int(list.decals.count); j++) {
        const DecalItem &de = list.decals.data[j];

        const float bbox_points[8][3] = {
            BBOX_POINTS(decals_boxes[j].bmin, decals_boxes[j].bmax)};

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if decal is inside of a whole slice
        for (int k = int(eCamPlane::NearPlane); k <= int(eCamPlane::FarPlane); k++) {
            int in_count = 8;

            for (int i = 0; i < 8; i++) { // NOLINT
                const float dist = first_sf->planes[k].n[0] * bbox_points[i][0] +
                                   first_sf->planes[k].n[1] * bbox_points[i][1] +
                                   first_sf->planes[k].n[2] * bbox_points[i][2] +
                                   first_sf->planes[k].d;
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

        for (int row_offset = 0; row_offset < frustums_per_slice;
             row_offset += REN_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if decal is inside of grid line
            for (int k = int(eCamPlane::TopPlane); k <= int(eCamPlane::BottomPlane);
                 k++) {
                int in_count = 8;

                for (int i = 0; i < 8; i++) { // NOLINT
                    const float dist = first_line_sf->planes[k].n[0] * bbox_points[i][0] +
                                       first_line_sf->planes[k].n[1] * bbox_points[i][1] +
                                       first_line_sf->planes[k].n[2] * bbox_points[i][2] +
                                       first_line_sf->planes[k].d;
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

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::LeftPlane); k <= int(eCamPlane::RightPlane);
                     k++) {
                    int in_count = 8;

                    for (int i = 0; i < 8; i++) { // NOLINT
                        const float dist = sf->planes[k].n[0] * bbox_points[i][0] +
                                           sf->planes[k].n[1] * bbox_points[i][1] +
                                           sf->planes[k].n[2] * bbox_points[i][2] +
                                           sf->planes[k].d;
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
                    CellData &cell = list.cells.data[index];
                    if (cell.decal_count < REN_MAX_DECALS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.decal_count]
                            .decal_index = (uint16_t)j;
                        cell.decal_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < int(list.probes.count); j++) {
        const ProbeItem &p = list.probes.data[j];
        const float *p_pos = &p.position[0];

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if probe is inside of a whole slice
        for (int k = int(eCamPlane::NearPlane); k <= int(eCamPlane::FarPlane); k++) {
            float dist = first_sf->planes[k].n[0] * p_pos[0] +
                         first_sf->planes[k].n[1] * p_pos[1] +
                         first_sf->planes[k].n[2] * p_pos[2] + first_sf->planes[k].d;
            if (dist < -p.radius) {
                visible_to_slice = eVisResult::Invisible;
            }
        }

        // Skip probe for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice;
             row_offset += REN_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if probe is inside of grid line
            for (int k = int(eCamPlane::TopPlane); k <= int(eCamPlane::BottomPlane);
                 k++) {
                float dist = first_line_sf->planes[k].n[0] * p_pos[0] +
                             first_line_sf->planes[k].n[1] * p_pos[1] +
                             first_line_sf->planes[k].n[2] * p_pos[2] +
                             first_line_sf->planes[k].d;
                if (dist < -p.radius) {
                    visible_to_line = eVisResult::Invisible;
                }
            }

            // Skip probe for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::LeftPlane); k <= int(eCamPlane::RightPlane);
                     k++) {
                    const float dist = sf->planes[k].n[0] * p_pos[0] +
                                       sf->planes[k].n[1] * p_pos[1] +
                                       sf->planes[k].n[2] * p_pos[2] + sf->planes[k].d;

                    if (dist < -p.radius) {
                        res = eVisResult::Invisible;
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = list.cells.data[index];
                    if (cell.probe_count < REN_MAX_PROBES_PER_CELL) {
                        local_items[row_offset + col_offset][cell.probe_count]
                            .probe_index = (uint16_t)j;
                        cell.probe_count++;
                    }
                }
            }
        }
    }

    const float EllipsoidInfluence = 3.0f;

    for (int j = 0; j < int(list.ellipsoids.count); j++) {
        const EllipsItem &e = list.ellipsoids.data[j];
        const float *p_pos = &e.position[0];

        eVisResult visible_to_slice = eVisResult::FullyVisible;

        // Check if ellipsoid is inside of a whole slice
        for (int k = int(eCamPlane::NearPlane); k <= int(eCamPlane::FarPlane); k++) {
            float dist = first_sf->planes[k].n[0] * p_pos[0] +
                         first_sf->planes[k].n[1] * p_pos[1] +
                         first_sf->planes[k].n[2] * p_pos[2] + first_sf->planes[k].d;
            if (dist < -EllipsoidInfluence) {
                visible_to_slice = eVisResult::Invisible;
            }
        }

        // Skip ellipsoid for whole slice
        if (visible_to_slice == eVisResult::Invisible) {
            continue;
        }

        for (int row_offset = 0; row_offset < frustums_per_slice;
             row_offset += REN_GRID_RES_X) {
            const Frustum *first_line_sf = first_sf + row_offset;

            eVisResult visible_to_line = eVisResult::FullyVisible;

            // Check if ellipsoid is inside of grid line
            for (int k = int(eCamPlane::TopPlane); k <= int(eCamPlane::BottomPlane);
                 k++) {
                float dist = first_line_sf->planes[k].n[0] * p_pos[0] +
                             first_line_sf->planes[k].n[1] * p_pos[1] +
                             first_line_sf->planes[k].n[2] * p_pos[2] +
                             first_line_sf->planes[k].d;
                if (dist < -EllipsoidInfluence) {
                    visible_to_line = eVisResult::Invisible;
                }
            }

            // Skip ellipsoid for whole line
            if (visible_to_line == eVisResult::Invisible) {
                continue;
            }

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const Frustum *sf = first_line_sf + col_offset;

                eVisResult res = eVisResult::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = int(eCamPlane::LeftPlane); k <= int(eCamPlane::RightPlane);
                     k++) {
                    const float dist = sf->planes[k].n[0] * p_pos[0] +
                                       sf->planes[k].n[1] * p_pos[1] +
                                       sf->planes[k].n[2] * p_pos[2] + sf->planes[k].d;

                    if (dist < -EllipsoidInfluence) {
                        res = eVisResult::Invisible;
                    }
                }

                if (res != eVisResult::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    CellData &cell = list.cells.data[index];
                    if (cell.ellips_count < REN_MAX_ELLIPSES_PER_CELL) {
                        local_items[row_offset + col_offset][cell.ellips_count]
                            .ellips_index = (uint16_t)j;
                        cell.ellips_count++;
                    }
                }
            }
        }
    }

    // Pack gathered local item data to total list
    for (int s = 0; s < frustums_per_slice; s++) {
        CellData &cell = list.cells.data[base_index + s];

        int local_items_count =
            (int)_MAX(cell.light_count,
                      _MAX(cell.decal_count, _MAX(cell.probe_count, cell.ellips_count)));

        if (local_items_count) {
            cell.item_offset = items_count.fetch_add(local_items_count);
            if (cell.item_offset > REN_MAX_ITEMS_TOTAL) {
                cell.item_offset = 0;
                cell.light_count = cell.decal_count = cell.probe_count =
                    cell.ellips_count = 0;
            } else {
                int free_items_left = REN_MAX_ITEMS_TOTAL - cell.item_offset;

                cell.light_count = _MIN((int)cell.light_count, free_items_left);
                cell.decal_count = _MIN((int)cell.decal_count, free_items_left);
                cell.probe_count = _MIN((int)cell.probe_count, free_items_left);
                cell.ellips_count = _MIN((int)cell.ellips_count, free_items_left);

                memcpy(&list.items.data[cell.item_offset], &local_items[s][0],
                       local_items_count * sizeof(ItemData));
            }
        }
    }
}

void RendererInternal::__push_ellipsoids(const Drawable &dr,
                                         const Ren::Mat4f &world_from_object,
                                         DrawList &list) {
    if (list.ellipsoids.count + dr.ellipsoids_count > REN_MAX_ELLIPSES_TOTAL) {
        return;
    }

    const Ren::Skeleton *skel = dr.mesh->skel();

    for (int i = 0; i < dr.ellipsoids_count; i++) {
        const Drawable::Ellipsoid &e = dr.ellipsoids[i];
        EllipsItem &ei = list.ellipsoids.data[list.ellipsoids.count++];

        auto pos = Ren::Vec4f{e.offset[0], e.offset[1], e.offset[2], 1.0f},
             axis = Ren::Vec4f{-e.axis[0], -e.axis[1], -e.axis[2], 0.0f};

        if (e.bone_index != -1 && skel->bones_count) {
            const Ren::Mat4f _world_from_object =
                world_from_object * skel->bones[e.bone_index].cur_comb_matrix;

            pos = _world_from_object * pos;
            axis = _world_from_object * axis;
        } else {
            pos = world_from_object * pos;
            axis = world_from_object * axis;
        }

        int perp = 0;
        if (_ABS(axis[1]) <= _ABS(axis[0]) && _ABS(axis[1]) <= _ABS(axis[2])) {
            perp = 1;
        } else if (_ABS(axis[2]) <= _ABS(axis[0]) && _ABS(axis[2]) <= _ABS(axis[1])) {
            perp = 2;
        }

        memcpy(&ei.position[0], &pos[0], 3 * sizeof(float));
        ei.radius = e.radius;
        memcpy(&ei.axis[0], &axis[0], 3 * sizeof(float));
        ei.perp = perp;
    }
}

uint32_t RendererInternal::__push_skeletal_mesh(const uint32_t skinned_buf_vtx_offset,
                                                const AnimState &as,
                                                const Ren::Mesh *mesh, DrawList &list) {
    const Ren::Skeleton *skel = mesh->skel();

    const auto palette_start = uint16_t(list.skin_transforms.count / 2);
    SkinTransform *out_matr_palette =
        &list.skin_transforms.data[list.skin_transforms.count];
    list.skin_transforms.count += uint32_t(2 * skel->bones_count);

    for (int i = 0; i < skel->bones_count; i++) {
        const Ren::Mat4f matr_curr_trans = Ren::Transpose(as.matr_palette_curr[i]);
        memcpy(&out_matr_palette[2 * i + 0].matr[0][0], Ren::ValuePtr(matr_curr_trans),
               12 * sizeof(float));

        const Ren::Mat4f matr_prev_trans = Ren::Transpose(as.matr_palette_prev[i]);
        memcpy(&out_matr_palette[2 * i + 1].matr[0][0], Ren::ValuePtr(matr_prev_trans),
               12 * sizeof(float));
    }

    const Ren::BufferRange &sk_buf = mesh->sk_attribs_buf();
    const Ren::BufferRange &deltas_buf = mesh->sk_deltas_buf();

    const uint32_t vertex_beg = sk_buf.offset / 48, vertex_cnt = sk_buf.size / 48;
    const uint32_t deltas_offset = deltas_buf.offset / 24;

    const uint32_t curr_out_offset = skinned_buf_vtx_offset + list.skin_vertices_count;

    SkinRegion &sr = list.skin_regions.data[list.skin_regions.count++];
    sr.in_vtx_offset = vertex_beg;
    sr.out_vtx_offset = curr_out_offset;
    sr.delta_offset = deltas_offset;
    sr.xform_offset = palette_start;
    // shape key data for current frame
    sr.shape_key_offset_curr = list.shape_keys_data.count;
    memcpy(&list.shape_keys_data.data[list.shape_keys_data.count],
           &as.shape_palette_curr[0], as.shape_palette_count_curr * sizeof(ShapeKeyData));
    list.shape_keys_data.count += as.shape_palette_count_curr;
    sr.shape_key_count_curr = as.shape_palette_count_curr;
    // shape key data from previous frame
    sr.shape_key_offset_prev = list.shape_keys_data.count;
    memcpy(&list.shape_keys_data.data[list.shape_keys_data.count],
           &as.shape_palette_prev[0], as.shape_palette_count_prev * sizeof(ShapeKeyData));
    list.shape_keys_data.count += as.shape_palette_count_prev;
    sr.shape_key_count_prev = as.shape_palette_count_prev;
    sr.vertex_count = vertex_cnt;
    if (skel->shapes_count) {
        sr.shape_keyed_vertex_count = skel->shapes[0].delta_count;
    } else {
        sr.shape_keyed_vertex_count = 0;
    }

    list.skin_vertices_count += vertex_cnt;

    assert(list.skin_vertices_count <= REN_MAX_SKIN_VERTICES_TOTAL);
    return curr_out_offset;
}

void RendererInternal::__init_wind_params(const VegState &vs, const Environment &env,
                                          const Ren::Mat4f &object_from_world,
                                          InstanceData &instance) {
    instance.movement_scale = f32_to_u8(vs.movement_scale);
    instance.tree_mode = f32_to_u8(vs.tree_mode);
    instance.bend_scale = f32_to_u8(vs.bend_scale);
    instance.stretch = f32_to_u8(vs.stretch);

    const auto wind_vec_ws =
        Ren::Vec4f{env.wind_vec[0], env.wind_vec[1], env.wind_vec[2], 0.0f};
    const Ren::Vec4f wind_vec_ls = object_from_world * wind_vec_ws;

    instance.wind_dir_ls[0] = Ren::f32_to_f16(wind_vec_ls[0]);
    instance.wind_dir_ls[1] = Ren::f32_to_f16(wind_vec_ls[1]);
    instance.wind_dir_ls[2] = Ren::f32_to_f16(wind_vec_ls[2]);
    instance.wind_turb = Ren::f32_to_f16(env.wind_turbulence);
}

#undef BBOX_POINTS
#undef _MAX
#undef _CROSS

#undef REN_UNINITIALIZE_X2
#undef REN_UNINITIALIZE_X4
#undef REN_UNINITIALIZE_X8
