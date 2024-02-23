#version 460
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_EXT_ray_query : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "_texturing.glsl"
#include "rt_shadows_interface.h"
#include "rt_shadow_common.glsl.inl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = BIND_UB_SHARED_DATA_BUF, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 g_vtx_data0[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_indices[];
};

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

        const uint ray_flags = gl_RayFlagsCullFrontFacingTrianglesEXT;

        float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

        rayQueryEXT rq;
        rayQueryInitializeEXT(rq,               // rayQuery
                              g_tlas,           // topLevel
                              ray_flags,        // rayFlags
                              0xff,             // cullMask
                              ray_origin_ws.xyz,// origin
                              min_t,            // tMin
                              shadow_ray_ws,    // direction
                              max_t             // tMax
                              );
        while(rayQueryProceedEXT(rq)) {
            if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
                // perform alpha test
                int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, false);
                int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, false);
                int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, false);
                vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, false);

                RTGeoInstance geo = g_geometries[custom_index + geo_index];
                MaterialData mat = g_materials[geo.material_index];

                uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
                uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
                uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

                vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
                vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
                vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

                vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
                float alpha = textureLod(SAMPLER2D(mat.texture_indices[3]), uv, 0.0).r;
                if (alpha >= 0.5) {
                    rayQueryConfirmIntersectionEXT(rq);
                }
            }
        }

        if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
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
