#version 460
#extension GL_EXT_ray_query : require
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "gi_common.glsl"

#include "rt_gi_interface.h"

#pragma multi_compile FIRST SECOND
#pragma multi_compile _ NO_SUBGROUP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    rt_geo_instance_t g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    material_data_t g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 g_vtx_data0[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_vtx_indices[];
};

layout(std430, binding = RAY_COUNTER_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;

layout(std430, binding = OUT_RAY_HITS_BUF_SLOT) writeonly buffer RayHitsList {
    uint g_ray_hits[];
};

layout (local_size_x = GRP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_index = gl_WorkGroupID.x * GRP_SIZE_X + gl_LocalInvocationIndex;
    if (ray_index >= g_ray_counter[7]) return;

#if defined(FIRST)
    const uint packed_coords = g_ray_list[ray_index];
#elif defined(SECOND)
    const uint packed_coords = g_ray_list[ray_index * RAY_LIST_STRIDE + 0];
#endif

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    const ivec2 icoord = ivec2(ray_coords);
    const float depth = texelFetch(g_depth_tex, icoord, 0).x;
    vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x).xyz;

    const vec2 px_center = vec2(icoord) + 0.5;
    const vec2 in_uv = px_center / vec2(g_params.img_size);

    const vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    vec3 ray_origin_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, ray_origin_cs);
    const float view_z = LinearizeDepth(ray_origin_cs.z, g_shrd_data.clip_info);
    vec3 gi_ray_ws = SampleDiffuseVector(g_noise_tex, normal_ws, icoord, 0);

    // Bias to avoid self-intersection
    // TODO: use flat normal here
    ray_origin_ws += (NormalBiasConstant + abs(ray_origin_ws) * NormalBiasPosAddition + NormalBiasViewAddition * view_z) * normal_ws;//offset_ray(ray_origin_ws.xyz, normal_ws);

#if defined(FIRST)
    const float _cone_width = g_params.pixel_spread_angle * view_z;
    vec3 throughput = vec3(1.0);
#elif defined(SECOND)
    const float first_hit_t = uintBitsToFloat(g_ray_list[ray_index * RAY_LIST_STRIDE + 1]);
    ray_origin_ws += gi_ray_ws * first_hit_t;

    normal_ws = UnpackUnitVector(unpackUnorm2x16(g_ray_list[ray_index * RAY_LIST_STRIDE + 2]));
    ray_origin_ws += 0.001 * normal_ws;

    gi_ray_ws = SampleDiffuseVector(g_noise_tex, normal_ws, icoord, 1);
    const float _cone_width = g_params.pixel_spread_angle * (view_z + first_hit_t);
    vec3 throughput = UnpackRGB565(g_ray_list[ray_index * RAY_LIST_STRIDE + 3]);
#endif

    const float t_min = 0.0;
    const float t_max = 100.0;

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,                       // rayQuery
                          g_tlas,                   // topLevel
                          0,                        // rayFlags
                          (1u << RAY_TYPE_DIFFUSE), // cullMask
                          ray_origin_ws,            // origin
                          t_min,                    // tMin
                          gi_ray_ws,                // direction
                          t_max                     // tMax
                          );

    int transp_depth = 0;
    while (rayQueryProceedEXT(rq)) {
        if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            if (transp_depth++ < 4) {
                // perform alpha test, account for alpha blending
                const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, false);
                const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, false);
                const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, false);
                const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, false);
                const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, false);

                const rt_geo_instance_t geo = g_geometries[custom_index + geo_index];
                const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
                const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                const uint i0 = g_vtx_indices[geo.indices_start + 3 * prim_id + 0];
                const uint i1 = g_vtx_indices[geo.indices_start + 3 * prim_id + 1];
                const uint i2 = g_vtx_indices[geo.indices_start + 3 * prim_id + 2];

                const vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
                const vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
                const vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

                const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
                const float alpha = (1.0 - mat.params[3].x) * textureLodBindless(mat.texture_indices[MAT_TEX_ALPHA], uv, 0.0).x;
                if (alpha < 0.5) {
                    continue;
                }
                if (mat.params[2].y > 0) {
                    const vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLodBindless(mat.texture_indices[MAT_TEX_BASECOLOR], uv, 0.0)));
                    throughput = min(throughput, mix(vec3(1.0), 0.8 * mat.params[2].y * base_color, alpha));
                    if (dot(throughput, vec3(0.333)) > 0.1) {
                        continue;
                    }
                }
            }
            rayQueryConfirmIntersectionEXT(rq);
        }
    }

    const bool is_hit = rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT;

#if !defined(NO_SUBGROUP)
    const uvec4 miss_ballot = subgroupBallot(!is_hit);
    const uint local_miss_index = subgroupBallotExclusiveBitCount(miss_ballot);
    const uint miss_count = subgroupBallotBitCount(miss_ballot);

    const uvec4 hit_ballot = subgroupBallot(is_hit);
    const uint local_hit_index = subgroupBallotExclusiveBitCount(hit_ballot);
    const uint hit_count = subgroupBallotBitCount(hit_ballot);

    uint miss_index = 0, hit_index = 0;
    if (subgroupElect()) {
        if (miss_count != 0) {
            miss_index = atomicAdd(g_ray_counter[8], miss_count);
        }
        if (hit_count != 0) {
            hit_index = atomicAdd(g_ray_counter[10], hit_count);
        }
    }
    miss_index = subgroupBroadcastFirst(miss_index) + local_miss_index;
    hit_index = subgroupBroadcastFirst(hit_index) + local_hit_index;

    const uint out_index = is_hit ? hit_index : miss_index;
#else
    uint out_index = 0;
    if (!is_hit) {
        out_index = atomicAdd(g_ray_counter[8], 1);
    } else {
        out_index = atomicAdd(g_ray_counter[10], 1);
    }
#endif

    if (!is_hit) {
        const uint out_offset = g_params.img_size.x * g_params.img_size.y * RAY_HITS_STRIDE - (out_index + 1) * RAY_MISS_STRIDE;

        // Append at the end of buffer
    #if defined(FIRST)
        g_ray_hits[out_offset + 0] = packed_coords;
    #elif defined(SECOND)
        g_ray_hits[out_offset + 0] = ray_index;
    #endif
        g_ray_hits[out_offset + 1] = PackRGB565(throughput);
    } else {
        const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, true);
        const int instance_index = rayQueryGetIntersectionInstanceIdEXT(rq, true);
        const float hit_t = rayQueryGetIntersectionTEXT(rq, true);
        const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
        const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, true);
        const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, true);
        const uint packed_throughput = PackRGB565(throughput);

    #if defined(FIRST)
        g_ray_hits[out_index * RAY_HITS_STRIDE + 0] = packed_coords;
    #elif defined(SECOND)
        g_ray_hits[out_index * RAY_HITS_STRIDE + 0] = ray_index;
    #endif
        g_ray_hits[out_index * RAY_HITS_STRIDE + 1] = (uint(instance_index) << 16u) | packed_throughput;
        g_ray_hits[out_index * RAY_HITS_STRIDE + 2] = floatBitsToUint(backfacing ? -hit_t : hit_t);
        g_ray_hits[out_index * RAY_HITS_STRIDE + 3] = (uint(prim_id) << 8u) | (geo_index & 0xffu);
        g_ray_hits[out_index * RAY_HITS_STRIDE + 4] = packUnorm2x16(bary_coord);
    }
}
