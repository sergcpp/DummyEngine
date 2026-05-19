#version 430 core
#extension GL_EXT_control_flow_attributes : require
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#define SWRT_SUBGROUP 1
#endif

#define MAX_STACK_FACTORS_SIZE 10

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "swrt_common.glsl"
#include "texturing_common.glsl"
#include "gi_cache_common.glsl"
#include "pmj_common.glsl"
#include "light_bvh_common.glsl"

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

layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    rt_geo_instance_t g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    material_data_t g_materials[];
};

layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;

layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;

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

    hit_data_t inter;
    inter.mask = 0;
    inter.obj_index = inter.prim_index = 0;
    inter.geo_index_count = 0;
    inter.tmin = 0.0;
    inter.tmax = 100.0;
    inter.u = inter.v = 0.0;

    const vec3 inv_d = safe_invert(probe_ray_dir);

    vec3 throughput = vec3(1.0);

    int transp_depth = 0;
    while (true) {
        Traverse_TLAS_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                probe_pos, probe_ray_dir, inv_d, (1u << RAY_TYPE_DIFFUSE), 0 /* root_node */, inter);
        if (inter.mask != 0 && transp_depth++ < 4) {
            // perform alpha test, account for alpha blending
            const bool backfacing = (inter.prim_index < 0);
            const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

            int geo_index = int(inter.geo_index_count & 0x00ffffffu);
            for (; geo_index < int(inter.geo_index_count & 0x00ffffffu) + int(inter.geo_index_count >> 24) - 1; ++geo_index) {
                const int tri_start = int(g_geometries[geo_index + 1].indices_start) / 3;
                if (tri_start > tri_index) {
                    break;
                }
            }

            const rt_geo_instance_t geo = g_geometries[geo_index];
            const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
            if ((mat_index & MATERIAL_SOLID_BIT) != 0) {
                break;
            }
            const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

            const uint i0 = texelFetch(g_vtx_indices, 3 * tri_index + 0).x;
            const uint i1 = texelFetch(g_vtx_indices, 3 * tri_index + 1).x;
            const uint i2 = texelFetch(g_vtx_indices, 3 * tri_index + 2).x;

            const vec2 uv0 = unpackHalf2x16(floatBitsToUint(texelFetch(g_vtx_data0, int(geo.vertices_start + i0)).w));
            const vec2 uv1 = unpackHalf2x16(floatBitsToUint(texelFetch(g_vtx_data0, int(geo.vertices_start + i1)).w));
            const vec2 uv2 = unpackHalf2x16(floatBitsToUint(texelFetch(g_vtx_data0, int(geo.vertices_start + i2)).w));

            const vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;
    #if defined(BINDLESS_TEXTURES)
            const float alpha = textureLodBindless(GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA]), uv, 0.0).x;
            if (alpha < 0.5) {
                inter.tmin = (inter.tmax + 0.0005);
                inter.tmax = 100.0 - inter.tmin;
                inter.mask = 0;
                continue;
            }
            if (mat.params[2].y > 0) {
                const vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLodBindless(GET_HANDLE(mat.texture_indices[MAT_TEX_BASECOLOR]), uv, 0.0)));
                throughput = min(throughput, mix(vec3(1.0), 0.8 * mat.params[2].y * base_color, alpha));
                if (dot(throughput, vec3(0.333)) > 0.1) {
                    inter.tmin = (inter.tmax + 0.0005);
                    inter.tmax = 100.0 - inter.tmin;
                    inter.mask = 0;
                    continue;
                }
            }
    #endif
        }
        break;
    }

    const uint out_index = PROBE_TOTAL_RAYS_COUNT * probe_index + ray_index;

    if (inter.mask == 0) {
        const uint packed_throughput = PackRGB565(throughput);

        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 0] = packed_throughput;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 1] = floatBitsToUint(1e27);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 2] = 0xffffffffu;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 3] = 0;
    } else {
        const bool backfacing = (inter.prim_index < 0);
        const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

        int i = int(inter.geo_index_count & 0x00ffffffu);
        for (; i < int(inter.geo_index_count & 0x00ffffffu) + int((inter.geo_index_count >> 24) & 0xffu) - 1; ++i) {
            const int tri_start = int(g_geometries[i + 1].indices_start / 3);
            if (tri_start > tri_index) {
                break;
            }
        }

        const int geo_index = i - int(inter.geo_index_count & 0x00ffffffu);
        const int prim_id = tri_index - int(g_geometries[i].indices_start / 3);
        const uint packed_throughput = PackRGB565(throughput);

        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 0] = (uint(inter.obj_index) << 16u) | packed_throughput;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 1] = floatBitsToUint(backfacing ? -inter.tmax : inter.tmax);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 2] = (uint(prim_id) << 8u) | (geo_index & 0xffu);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 3] = packUnorm2x16(vec2(inter.u, inter.v));
    }
}