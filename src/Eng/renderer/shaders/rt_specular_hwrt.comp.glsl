#version 460
#extension GL_EXT_ray_query : require
#extension GL_EXT_control_flow_attributes : require
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "rt_specular_common.glsl"
#include "rad_cache_common.glsl"

#include "rt_specular_interface.h"

#pragma multi_compile FIRST SECOND LAYERED
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

layout(std430, binding = VTX_BUF2_SLOT) readonly buffer VtxData1 {
    uvec4 g_vtx_data1[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_vtx_indices[];
};

#ifdef STOCH_LIGHTS
    layout(binding = STOCH_LIGHTS_BUF_SLOT) uniform usamplerBuffer g_stoch_lights_buf;
    layout(binding = LIGHT_NODES_BUF_SLOT) uniform samplerBuffer g_light_nodes_buf;
#endif

layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2D g_shadow_color_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

#ifdef GI_CACHE
    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(std430, binding = RAY_COUNTER_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

#ifdef LAYERED
    layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;
#else
    layout(binding = TCBN_2D_TEX_SLOT) uniform sampler2DArray g_tcbn_2d_tex;

    layout(std430, binding = CACHE_ENTRIES_BUF_SLOT) readonly buffer CacheEntries {
        uint g_cache_entries[];
    };

    layout(std430, binding = CACHE_VOXELS_BUF_SLOT) readonly buffer CacheVoxels {
        uint g_cache_voxels[];
    };

    layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;
#endif

layout(std430, binding = OUT_RAY_HITS_BUF_SLOT) writeonly buffer RayHitsList {
    uint g_out_ray_hits[];
};

#ifndef LAYERED
bool hash_map_find(const uint hash_key, inout uint cache_entry, out uint bucket_offset) {
    const uint base_slot = hash_map_base_slot(hash_key);
    for (bucket_offset = 0; bucket_offset < HASH_GRID_HASH_MAP_BUCKET_SIZE; ++bucket_offset) {
        const uint stored_hash_key = g_cache_entries[base_slot + bucket_offset];
        if (stored_hash_key == hash_key) {
            cache_entry = base_slot + bucket_offset;
            return true;
        }
    }
    return false;
}

uint find_entry(const vec3 p, const bool backfacing, const vec3 cam_pos) {
    const uint hash_key = compute_hash(p, backfacing, cam_pos);
    uint cache_entry = HASH_GRID_INVALID_CACHE_ENTRY, collisions_count;
    hash_map_find(hash_key, cache_entry, collisions_count);
    return cache_entry;
}
#endif

layout (local_size_x = GRP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_index = gl_WorkGroupID.x * GRP_SIZE_X + gl_LocalInvocationIndex;
    if (ray_index >= g_ray_counter[7]) return;

#ifndef LAYERED
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
    const vec4 normal_roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x);
    vec3 normal_ws = normal_roughness.xyz;
    const vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    const float first_roughness = normal_roughness.w * normal_roughness.w;

    const vec2 px_center = vec2(icoord) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);

    const vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;

    const vec3 view_ray_vs = normalize(ray_origin_vs);
    const vec4 u = vec4(texelFetch(g_tcbn_2d_tex, (ivec3(icoord, g_params.frame_index) + 39 * RAND_DIM_2D_SPECULAR_0) % 64, 0).xy,
                        texelFetch(g_tcbn_2d_tex, (ivec3(icoord, g_params.frame_index) + 39 * RAND_DIM_2D_SPECULAR_1) % 64, 0).xy);
    const vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, first_roughness, u.xy);
    vec3 refl_ray_ws = (g_shrd_data.world_from_view * vec4(refl_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.world_from_view * vec4(ray_origin_vs, 1.0);
    ray_origin_ws /= ray_origin_ws.w;

    ray_origin_ws.xyz += (NormalBiasConstant + abs(ray_origin_ws.xyz) * NormalBiasPosAddition + view_z * NormalBiasViewAddition) * normal_ws;
    const float t_min = 0.0;
#else // LAYERED
    const uint packed_coords = g_ray_list[2 * ray_index + 0];
    const uint packed_dir = g_ray_list[2 * ray_index + 1];

    uvec2 ray_coords;
    uint layer_index;
    UnpackRayCoords(packed_coords, ray_coords, layer_index);

    const vec2 oct_dir = vec2(packed_dir & 0xffffu, (packed_dir >> 16) & 0xffffu) / 65535.0;
    vec3 refl_ray_ws = UnpackUnitVector(oct_dir);

    uint frag_index = layer_index * g_shrd_data.uren_res.x * g_shrd_data.uren_res.y;
    frag_index += ray_coords.y * g_shrd_data.uren_res.x + ray_coords.x;
    const float depth = uintBitsToFloat(texelFetch(g_oit_depth_buf, int(frag_index)).x);
    const float first_roughness = 0.0;

    const ivec2 icoord = ivec2(ray_coords);
    const vec2 norm_uvs = (vec2(ray_coords) + 0.5) * g_shrd_data.fren_res.zw;
    const vec3 ray_origin_ss = vec3(norm_uvs, depth);
    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;

    vec4 ray_origin_ws = g_shrd_data.world_from_view * vec4(ray_origin_vs, 1.0);
    ray_origin_ws /= ray_origin_ws.w;

    const float t_min = 0.001;
#endif // LAYERED

#if defined(FIRST) || defined(LAYERED)
    const float roughness = first_roughness;
    vec3 throughput = vec3(1.0);
#elif defined(SECOND)
    const float first_hit_t = uintBitsToFloat(g_ray_list[ray_index * RAY_LIST_STRIDE + 1]);
    ray_origin_ws.xyz += refl_ray_ws * first_hit_t;

    normal_ws = UnpackUnitVector(unpackUnorm2x16(g_ray_list[ray_index * RAY_LIST_STRIDE + 2]));
    ray_origin_ws.xyz += 0.001 * normal_ws;

    const uint packed_roughness_throughput = g_ray_list[ray_index * RAY_LIST_STRIDE + 3];
    const float roughness = float(packed_roughness_throughput >> 16u) / 255.0;

    refl_ray_ws = SampleReflectionVector(refl_ray_ws, normal_ws, roughness, u.zw);
    vec3 throughput = UnpackRGB565(packed_roughness_throughput & 0xffffu);
#endif

    const float t_max = 100.0;

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,                       // rayQuery
                          g_tlas,                   // topLevel
                          0,                        // rayFlags
                          (1u << RAY_TYPE_SPECULAR),// cullMask
                          ray_origin_ws.xyz,        // origin
                          t_min,                    // tMin
                          refl_ray_ws,              // direction
                          t_max                     // tMax
                          );

    int transp_depth = 0;
    while(rayQueryProceedEXT(rq)) {
        if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            if (transp_depth++ < 8) {
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
                    const vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLodBindless(GET_HANDLE(mat.texture_indices[MAT_TEX_BASECOLOR]), uv, 0.0)));
                    throughput = min(throughput, mix(vec3(1.0), 0.8 * mat.params[2].y * base_color, alpha));
                    if (dot(throughput, vec3(0.333)) > 0.1) {
                        continue;
                    }
                }
            }
            rayQueryConfirmIntersectionEXT(rq);
        }
    }

    const bool is_hit = (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT);
#ifndef LAYERED
    if (is_hit) {
        const float hit_t = rayQueryGetIntersectionTEXT(rq, true);
        const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, true);
        const vec3 P = ray_origin_ws.xyz + hit_t * refl_ray_ws;

        const uint grid_level = calc_grid_level(P, g_shrd_data.cam_pos_rad.xyz);
        const float voxel_size = calc_voxel_size(grid_level);

        float cone_angle = g_params.pixel_spread_angle;
        float cone_width = cone_angle * view_z;
#if defined(SECOND)
        cone_angle = sqrt(cone_angle * cone_angle + first_roughness * first_roughness);
        cone_width += cone_angle * first_hit_t;
#endif
        cone_angle = sqrt(cone_angle * cone_angle + roughness * roughness);
        cone_width += cone_angle * hit_t;
        const bool use_cache =  cone_width > 1.5 * voxel_size;
        if (use_cache) {
            const uint cache_entry = find_entry(P, backfacing, g_shrd_data.cam_pos_rad.xyz);
            if (cache_entry != HASH_GRID_INVALID_CACHE_ENTRY) {
                vec4 out_color = vec4(0.0);

                out_color.xyz = vec3(
                    unpackHalf2x16(g_cache_voxels[2 * cache_entry + 0]),
                    unpackHalf2x16(g_cache_voxels[2 * cache_entry + 1]).x);

                const bool is_emissive = any(lessThan(out_color.xyz, vec3(0.0)));
                if (!is_emissive) {
                    out_color.xyz *= throughput * RAD_CACHE_RADIANCE_COMPRESSION;
                    out_color.xyz = compress_hdr(out_color.xyz, g_shrd_data.cam_pos_and_exp.w);

                    const vec4 old_color = imageLoad(g_out_color_img, icoord);
                    out_color.xyz += old_color.xyz;
#if defined(SECOND)
                    out_color.w = old_color.w;
#else
                    out_color.w = NormalizeHitDist(hit_t, view_z, first_roughness);
#endif
                    imageStore(g_out_color_img, icoord, out_color);

                    const ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
                    if (copy_horizontal) {
                        imageStore(g_out_color_img, ivec2(copy_target.x, icoord.y), out_color);
                    }
                    if (copy_vertical) {
                        imageStore(g_out_color_img, ivec2(icoord.x, copy_target.y), out_color);
                    }
                    if (copy_diagonal) {
                        imageStore(g_out_color_img, copy_target, out_color);
                    }
                    return;
                }
            }
        }
    }
#endif // LAYERED

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
    #if !defined(LAYERED)
        const uint out_offset = g_params.img_size.x * g_params.img_size.y * RAY_HITS_STRIDE - (out_index + 1) * RAY_MISS_STRIDE;
    #else
        const uint out_offset = OIT_REFLECTION_LAYERS * ((g_params.img_size.x + 1) / 2) * ((g_params.img_size.y + 1) / 2) * RAY_HITS_STRIDE - (out_index + 1) * RAY_MISS_STRIDE;
    #endif

        // Append at the end of buffer
    #if defined(FIRST)
        g_out_ray_hits[out_offset + 0] = packed_coords;
    #elif defined(SECOND) || defined(LAYERED)
        g_out_ray_hits[out_offset + 0] = ray_index;
    #endif
        g_out_ray_hits[out_offset + 1] = PackRGB565(throughput);
    } else {
        const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, true);
        const int instance_index = rayQueryGetIntersectionInstanceIdEXT(rq, true);
        const float hit_t = rayQueryGetIntersectionTEXT(rq, true);
        const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
        const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, true);
        const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, true);
        const uint packed_throughput = PackRGB565(throughput);

    #if defined(FIRST)
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 0] = packed_coords;
    #elif defined(SECOND) || defined(LAYERED)
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 0] = ray_index;
    #endif
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 1] = (uint(instance_index) << 16u) | packed_throughput;
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 2] = floatBitsToUint(backfacing ? -hit_t : hit_t);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 3] = (uint(prim_id) << 8u) | (geo_index & 0xffu);
        g_out_ray_hits[out_index * RAY_HITS_STRIDE + 4] = packUnorm2x16(bary_coord);
    }
}
