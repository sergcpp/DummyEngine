#version 460
#extension GL_EXT_control_flow_attributes : require
#extension GL_EXT_ray_query : require
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#define MAX_STACK_SIZE 10

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "gi_cache_common.glsl"
#include "pmj_common.glsl"

#include "rt_gi_cache_interface.h"

#pragma multi_compile _ PARTIAL
#pragma multi_compile _ NO_SUBGROUP

#if defined(NO_SUBGROUP)
    #define subgroupMin(x) (x)
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

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

layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(std430, binding = OUT_RAY_HITS_BUF_SLOT) writeonly buffer OutRayHitsList {
    uint g_out_ray_hits[];
};

layout (local_size_x = GRP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_index = gl_GlobalInvocationID.x;
    const uint probe_plane_index = gl_GlobalInvocationID.y;
    const uint plane_index = gl_GlobalInvocationID.z;

    uint probe_index = (plane_index * PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z) + probe_plane_index;

    const uvec3 probe_coords = get_probe_coords(probe_index);
    probe_index = get_scrolling_probe_index(probe_coords, g_params.grid_scroll.xyz);

    const uvec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
    const bool is_inactive = ray_index >= PROBE_FIXED_RAYS_COUNT && texelFetch(g_offset_tex, ivec3(tex_coords), 0).w < 0.5;
#ifdef PARTIAL
    const uvec3 oct_index = get_probe_coords(probe_index) & 1u;
    const bool is_wrong_oct = (oct_index.x | (oct_index.y << 1u) | (oct_index.z << 2u)) != g_params.oct_index;
#else
    const bool is_wrong_oct = false;
#endif

    if (!IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz) && (is_inactive || is_wrong_oct)) {
        return;
    }

    const vec3 probe_pos = get_probe_pos_ws(g_params.volume_index, probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz, g_offset_tex);
    const vec3 probe_ray_dir = get_probe_ray_dir(ray_index, g_params.quat_rot);
    const uvec3 output_coords = get_ray_data_coords(ray_index, probe_index);

    vec3 ro = probe_pos;

    const uint ray_flags = 0;//gl_RayFlagsCullBackFacingTrianglesEXT;
    const float t_min = 0.0;
    const float t_max = 100.0;

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,                       // rayQuery
                          g_tlas,                   // topLevel
                          ray_flags,                // rayFlags
                          (1u << RAY_TYPE_DIFFUSE), // cullMask
                          ro,                       // origin
                          t_min,                    // tMin
                          probe_ray_dir,            // direction
                          t_max                     // tMax
                          );

    vec3 throughput = vec3(1.0);

    int transp_depth = 0;
    while(rayQueryProceedEXT(rq) && transp_depth++ < 4) {
        if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
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
            const float alpha = textureLodBindless(mat.texture_indices[MAT_TEX_ALPHA], uv, 0.0).x;
            if (alpha >= 0.5) {
                rayQueryConfirmIntersectionEXT(rq);
            }
            if (mat.params[2].y > 0) {
                const vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLodBindless(mat.texture_indices[MAT_TEX_BASECOLOR], uv, 0.0)));
                throughput = min(throughput, mix(vec3(1.0), 0.8 * mat.params[2].y * base_color, alpha));
                if (dot(throughput, vec3(0.333)) <= 0.1) {
                    rayQueryConfirmIntersectionEXT(rq);
                }
            }
        }
    }

    const uint out_index = PROBE_TOTAL_RAYS_COUNT * probe_index + ray_index;

    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        const uint packed_throughput = PackRGB565(throughput);

        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 0] = packed_throughput;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 1] = floatBitsToUint(1e27);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 2] = 0xffffffffu;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 3] = 0;
    } else {
        const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true);
        const int instance_index = rayQueryGetIntersectionInstanceIdEXT(rq, true);
        const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, true);
        const float hit_t = rayQueryGetIntersectionTEXT(rq, true);
        const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
        const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, true);
        const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, true);
        const uint packed_throughput = PackRGB565(throughput);
        const mat4x3 world_from_object = rayQueryGetIntersectionObjectToWorldEXT(rq, true);

        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 0] = (uint(instance_index) << 16u) | packed_throughput;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 1] = floatBitsToUint(backfacing ? -hit_t : hit_t);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 2] = (uint(prim_id) << 8u) | (geo_index & 0xffu);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 3] = packUnorm2x16(bary_coord);
    }
}