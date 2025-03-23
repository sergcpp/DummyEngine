#version 460
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_EXT_ray_query : require

#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "rt_shadows_interface.h"
#include "rt_shadow_common.glsl.inl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;
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
    uint g_indices[];
};

layout(std430, binding = TILE_LIST_SLOT) readonly buffer TileList {
    uvec4 g_tile_list[];
};

layout(binding = OUT_SHADOW_IMG_SLOT, r32ui) uniform restrict uimage2D g_out_shadow_img;

layout (local_size_x = TILE_SIZE_X, local_size_y = TILE_SIZE_Y, local_size_z = 1) in;

shared uint g_shared_mask;

void main() {
    if (gl_LocalInvocationIndex == 0) {
        g_shared_mask = 0;
    }

    uvec2 group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec4 tile = g_tile_list[gl_WorkGroupID.x];

    uvec2 tile_coord;
    uint mask;
    float min_t, max_t;
    UnpackTile(tile, tile_coord, mask, min_t, max_t);

    ivec2 icoord = ivec2(tile_coord * uvec2(TILE_SIZE_X, TILE_SIZE_Y) + group_thread_id);

    const uint bit_index = LaneIdToBitShift(group_thread_id);
    bool is_in_shadow = ((1u << bit_index) & mask) == 0u;
    if (!is_in_shadow) {
        float depth = texelFetch(g_depth_tex, icoord, 0).x;
        vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x).xyz;
        vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

        vec2 px_center = vec2(icoord) + 0.5;
        vec2 in_uv = px_center / vec2(g_params.img_size);

        const vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
        const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);

        const vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
        vec3 shadow_ray_ws = g_shrd_data.sun_dir.xyz;

        vec2 u = texelFetch(g_noise_tex, icoord % 128, 0).xy;
        shadow_ray_ws = MapToCone(u, shadow_ray_ws, g_shrd_data.sun_dir.w);

        vec4 ray_origin_ws = g_shrd_data.world_from_view * vec4(ray_origin_vs, 1.0);
        ray_origin_ws /= ray_origin_ws.w;

        // Bias to avoid self-intersection
        // TODO: use flat normal here
        ray_origin_ws.xyz += (NormalBiasConstant + abs(ray_origin_ws.xyz) * NormalBiasPosAddition + (-ray_origin_vs.z) * NormalBiasViewAddition) * normal_ws;

        float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

        rayQueryEXT rq;
        rayQueryInitializeEXT(rq,                       // rayQuery
                              g_tlas,                   // topLevel
                              0,                        // rayFlags
                              (1u << RAY_TYPE_SHADOW),  // cullMask
                              ray_origin_ws.xyz,        // origin
                              min_t,                    // tMin
                              shadow_ray_ws,            // direction
                              max_t                     // tMax
                              );

        int transp_depth = 0;
        while(rayQueryProceedEXT(rq)) {
            if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
                if (transp_depth++ < 4) {
                    // perform alpha test
                    const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, false);
                    const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, false);
                    const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, false);
                    const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, false);
                    const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, false);

                    const rt_geo_instance_t geo = g_geometries[custom_index + geo_index];
                    const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
                    const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                    const uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
                    const uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
                    const uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

                    const vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
                    const vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
                    const vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

                    const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
                    const float alpha = (1.0 - mat.params[3].x) * textureLod(SAMPLER2D(mat.texture_indices[MAT_TEX_ALPHA]), uv, 0.0).x;
                    if (alpha < 0.5) {
                        continue;
                    }
                }
                rayQueryConfirmIntersectionEXT(rq);
            }
        }

        if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT ||
            transp_depth >= 4) {
            is_in_shadow = true;
        }
    }

    uint new_mask = uint(is_in_shadow) << bit_index;
    if (gl_NumSubgroups == 1) {
        new_mask = subgroupOr(new_mask);
    } else {
        groupMemoryBarrier(); barrier();
        atomicOr(g_shared_mask, new_mask);
        groupMemoryBarrier(); barrier();
        new_mask = g_shared_mask;
    }
    if (gl_LocalInvocationIndex == 0) {
        uint old_mask = imageLoad(g_out_shadow_img, ivec2(tile_coord)).x;
        imageStore(g_out_shadow_img, ivec2(tile_coord), uvec4(old_mask & new_mask));
    }
}
