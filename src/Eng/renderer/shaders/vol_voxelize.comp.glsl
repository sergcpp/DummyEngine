#version 430 core
#if defined(HWRT)
#extension GL_EXT_ray_query : require
#endif
#extension GL_EXT_control_flow_attributes : require

#include "_cs_common.glsl"
#include "rt_common.glsl"
#include "swrt_common.glsl"
#include "pmj_common.glsl"
#include "vol_common.glsl"

#include "vol_interface.h"

#pragma multi_compile _ HWRT

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = STBN_TEX_SLOT) uniform sampler2DArray g_stbn_tex;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    rt_geo_instance_t g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    material_data_t g_materials[];
};

#if defined(HWRT)
    layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
#else
    layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
    layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

    layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
    layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;

    layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
    layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;
#endif

layout(binding = OUT_FR_EMISSION_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_fr_emission_img;
layout(binding = OUT_FR_SCATTER_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_fr_scatter_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    ivec3 icoord = ivec3(gl_GlobalInvocationID.xy, 0);
    if (any(greaterThanEqual(icoord.xy, g_params.froxel_res.xy))) {
        return;
    }

    const vec2 norm_uvs = (vec2(icoord.xy) + 0.5) / g_params.froxel_res.xy;
    const vec2 d = norm_uvs * 2.0 - 1.0;

    vec3 origin_ws = g_shrd_data.cam_pos_and_exp.xyz;
    const vec3 target_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(d.xy, 1, 1));
    const vec3 dir_ws = normalize(target_ws - origin_ws);
    const vec3 inv_d = safe_invert(dir_ws);
    const vec3 neg_inv_d_o = -inv_d * origin_ws;
    const float k = dot(dir_ws, g_shrd_data.frustum_planes[4].xyz);

    const uint px_hash = hash((gl_GlobalInvocationID.x << 16) | gl_GlobalInvocationID.y);
    const float offset_rand = textureLod(g_stbn_tex, vec3((vec2(icoord.xy) + 0.5) / 64.0, float(g_params.frame_index % 64)), 0.0).x;

    const int MAX_VOL_STACK_SIZE = 6;

    vec4 scatter[MAX_VOL_STACK_SIZE];
    vec4 emission[MAX_VOL_STACK_SIZE];
    int vol_stack_size = 0;

    // Add air
    scatter[vol_stack_size] = vec4(0.0);
    emission[vol_stack_size++] = vec4(0.0);

    // TODO: Properly determine if we are inside of mesh volume
    const vec2 bbox_intersection = bbox_test(inv_d, neg_inv_d_o, g_params.bbox_min.xyz, g_params.bbox_max.xyz);
    if (bbox_intersection.x < bbox_intersection.y && bbox_intersection.x < 0.0) {
        // We are inside of global volume
        scatter[vol_stack_size] = g_params.scatter_color;
        emission[vol_stack_size++] = g_params.emission_color;
    }

    float cur_t = 0.0;
    int depth = 0;
    while (depth++ < 4) {
        int enter = -1;
        float next_t = MAX_DIST;
        vec4 next_scatter, next_emission;

        { // Check global volume intersection
            vec2 bbox_intersection = bbox_test(inv_d, neg_inv_d_o, g_params.bbox_min.xyz, g_params.bbox_max.xyz);
            if (bbox_intersection.x < bbox_intersection.y && any(greaterThan(bbox_intersection, vec2(cur_t)))) {
                enter = int(bbox_intersection.x > cur_t);
                next_t = (enter == 1) ? bbox_intersection.x : bbox_intersection.y;

                next_scatter = g_params.scatter_color;
                next_emission = g_params.emission_color;
            }
        }
        { // Check meshes intersection
#ifdef HWRT
            rayQueryEXT rq;
            rayQueryInitializeEXT(rq,                       // rayQuery
                                  g_tlas,                   // topLevel
                                  0,                        // rayFlags
                                  (1u << RAY_TYPE_VOLUME),  // cullMask
                                  origin_ws,                // origin
                                  cur_t,                    // tMin
                                  dir_ws,                   // direction
                                  next_t                    // tMax
                                 );
            while (rayQueryProceedEXT(rq)) {
                // NOTE: All instances are opaque, no need to confirm intersection
            }
            if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
                enter = int(rayQueryGetIntersectionFrontFaceEXT(rq, true));
                next_t = rayQueryGetIntersectionTEXT(rq, true);

                const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true);
                const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, true);

                const rt_geo_instance_t geo = g_geometries[custom_index + geo_index];
                const uint mat_index = (geo.material_index & 0xffff);
                const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                next_scatter = mat.params[0];
                next_emission = mat.params[1];
            }
#else // HWRT
            hit_data_t inter;
            inter.mask = 0;
            inter.obj_index = inter.prim_index = 0;
            inter.geo_index_count = 0;
            inter.t = next_t - cur_t;
            inter.u = inter.v = 0.0;

            Traverse_TLAS_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                    origin_ws + cur_t * dir_ws, dir_ws, inv_d, (1u << RAY_TYPE_VOLUME), 0 /* root_node */, inter);
            if (inter.mask != 0) {
                const bool backfacing = (inter.prim_index < 0);

                enter = int(!backfacing);
                next_t = cur_t + inter.t;

                const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

                int i = int(inter.geo_index_count & 0x00ffffffu);
                for (; i < int(inter.geo_index_count & 0x00ffffffu) + int((inter.geo_index_count >> 24) & 0xffu); ++i) {
                    const int tri_start = int(g_geometries[i].indices_start) / 3;
                    if (tri_start > tri_index) {
                        break;
                    }
                }

                const int geo_index = i - 1;

                const rt_geo_instance_t geo = g_geometries[geo_index];
                const uint mat_index = (geo.material_index & 0xffff);
                const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                next_scatter = mat.params[0];
                next_emission = mat.params[1];
            }
#endif // HWRT
        }

        if (enter == -1) {
            break;
        }

        // Apply jittering (check if sampling point is within ray segment)
        const float z_val = log2(k * next_t / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
        int z_next = min(int(z_val * g_params.froxel_res.z), g_params.froxel_res.z - 1);
        const float dist_next = vol_slice_distance(z_next, offset_rand, g_params.froxel_res.z);
        z_next += int(k * next_t > dist_next);

        vec4 emission_density = emission[vol_stack_size - 1];
        emission_density.xyz = compress_hdr(emission_density.xyz, g_shrd_data.cam_pos_and_exp.w);
        for (; icoord.z < z_next; ++icoord.z) {
            imageStore(g_out_fr_emission_img, icoord, emission_density);
            imageStore(g_out_fr_scatter_img, icoord, scatter[vol_stack_size - 1]);
        }

        if (enter == 1) {
            scatter[vol_stack_size] = next_scatter;
            emission[vol_stack_size++] = next_emission;
        } else {
            vol_stack_size = max(vol_stack_size - 1, 1);
        }
        cur_t = next_t + 0.0005;
    }

    for (; icoord.z < g_params.froxel_res.z; ++icoord.z) {
        imageStore(g_out_fr_emission_img, icoord, vec4(0.0));
        imageStore(g_out_fr_scatter_img, icoord, vec4(0.0));
    }
}
