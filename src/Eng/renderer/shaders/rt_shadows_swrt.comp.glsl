#version 320 es
#extension GL_KHR_shader_subgroup_arithmetic : require
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "_swrt_common.glsl"
#include "_texturing.glsl"
#include "rt_shadows_interface.h"
#include "rt_shadow_common.glsl.inl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;

layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;

layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
layout(binding = MESHES_BUF_SLOT) uniform usamplerBuffer g_meshes;
layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;

layout(std430, binding = TILE_LIST_SLOT) readonly buffer TileList {
    uvec4 g_tile_list[];
};

layout(binding = OUT_SHADOW_IMG_SLOT, r32ui) uniform restrict uimage2D g_out_shadow_img;

layout (local_size_x = TILE_SIZE_X, local_size_y = TILE_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec4 tile = g_tile_list[gl_WorkGroupID.x];

    uvec2 tile_coord;
    uint mask;
    float min_t, max_t;
    UnpackTile(tile, tile_coord, mask, min_t, max_t);

    ivec2 icoord = ivec2(tile_coord * uvec2(TILE_SIZE_X, TILE_SIZE_Y) + group_thread_id);

    bool is_in_shadow = false;

    uint bit_index = LaneIdToBitShift(group_thread_id);
    bool is_active = ((1u << bit_index) & mask) != 0u;
    if (is_active) {
        float depth = texelFetch(g_depth_tex, icoord, 0).r;
        vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x).xyz;
        vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

        vec2 px_center = vec2(icoord) + 0.5;
        vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
        vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
        ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
        vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

        vec4 ray_origin_vs = g_shrd_data.view_from_clip * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
        vec3 shadow_ray_ws = g_shrd_data.sun_dir.xyz;

        vec2 u = texelFetch(g_noise_tex, icoord % 128, 0).rg;
        shadow_ray_ws = MapToCone(u, shadow_ray_ws, g_shrd_data.sun_dir.w);

        vec4 ray_origin_ws = g_shrd_data.world_from_view * ray_origin_vs;
        ray_origin_ws /= ray_origin_ws.w;

        // Bias to avoid self-intersection
        // TODO: use flat normal here
        ray_origin_ws.xyz += 0.001 * normal_ws; //offset_ray(ray_origin_ws.xyz, 2 * normal_ws);

        float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

        vec3 inv_d = safe_invert(shadow_ray_ws.xyz);

        hit_data_t inter;
        inter.mask = 0;
        inter.obj_index = inter.prim_index = 0;
        inter.geo_index = inter.geo_count = 0;
        inter.t = 1000.0;
        inter.u = inter.v = 0.0;

        Traverse_MacroTree_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_meshes, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                     ray_origin_ws.xyz, shadow_ray_ws.xyz, inv_d, 0 /* root_node */, inter);
        if (inter.mask != 0) {
            is_in_shadow = true;
        }
    }

    uint new_mask = uint(is_in_shadow) << bit_index;
    new_mask = subgroupOr(new_mask);
    if (gl_LocalInvocationIndex == 0) {
        uint old_mask = imageLoad(g_out_shadow_img, ivec2(tile_coord)).r;
        imageStore(g_out_shadow_img, ivec2(tile_coord), uvec4(old_mask & new_mask));
    }
}
