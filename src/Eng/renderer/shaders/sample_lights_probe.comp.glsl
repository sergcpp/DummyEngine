#version 430 core
#if defined(HWRT)
#extension GL_EXT_ray_query : require
#endif
#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_vote : require
#define SWRT_SUBGROUP 1
#endif

#include "rt_common.glsl"
#include "swrt_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "pmj_common.glsl"
#include "rt_specular_common.glsl"
#include "light_bvh_common.glsl"
#include "gi_cache_common.glsl"

#include "sample_lights_interface.h"

#pragma multi_compile _ MIS
#pragma multi_compile _ PARTIAL
#pragma multi_compile _ HWRT
#pragma multi_compile _ NO_SUBGROUP

const int TRANSPARENCY_LIMIT = 4;

LAYOUT_PARAMS uniform UniformParams {
    Params2 g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;
layout(binding = LIGHTS_BUF_SLOT) uniform usamplerBuffer g_lights_buf;
layout(binding = LIGHT_NODES_BUF_SLOT) uniform samplerBuffer g_light_nodes_buf;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    rt_geo_instance_t g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    material_data_t g_materials[];
};

layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;

#if defined(HWRT)
    layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
#else
    layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
    layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

    layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
    layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;
#endif

layout(std430, binding = OUT_SH1_DATA_BUF_SLOT) writeonly buffer SH1Data {
    uvec2 g_sh1_data[];
};

shared uvec2 g_sh1_r[GRP_SIZE2_X];
shared uvec2 g_sh1_g[GRP_SIZE2_X];
shared uvec2 g_sh1_b[GRP_SIZE2_X];

layout (local_size_x = GRP_SIZE2_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_index = PROBE_FIXED_RAYS_COUNT + gl_GlobalInvocationID.x;
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
    const uvec3 output_coords = get_ray_data_coords(ray_index, probe_index);

    vec3 out_color = vec3(0.0), out_dir = vec3(0.0);

    const uint px_hash = g_params.pass_hash;
    const float light_pick_rand = get_scrambled_2d_rand(g_random_seq, RAND_DIM_LIGHT_PICK, px_hash, ray_index).x;

    float pdf_factor;
    const int li = PickLightSource(probe_pos, g_light_nodes_buf, g_params.stoch_lights_count, light_pick_rand, pdf_factor);
    if (li != -1) {
        const _light_item_t litem = FetchLightItem(g_lights_buf, li);
        const bool is_doublesided = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DOUBLESIDED_BIT) != 0;

        const vec3 p1 = litem.pos_and_radius.xyz,
                   p2 = litem.u_and_reg.xyz,
                   p3 = litem.v_and_blend.xyz;
        const vec2 uv1 = vec2(litem.pos_and_radius.w, litem.dir_and_spot.x),
                   uv2 = litem.dir_and_spot.yz,
                   uv3 = vec2(litem.dir_and_spot.w, litem.v_and_blend.w);
        const vec3 e1 = p2 - p1, e2 = p3 - p1;
        float light_fwd_len;
        const vec3 light_forward = normalize_len(cross(e1, e2), light_fwd_len);

        // Simple area sampling
        const vec2 rand_light_uv = get_scrambled_2d_rand(g_random_seq, RAND_DIM_LIGHT_UV, px_hash, ray_index);
        const float r1 = sqrt(rand_light_uv.x), r2 = rand_light_uv.y;
        const vec2 luv = uv1 * (1.0 - r1) + r1 * (uv2 * (1.0 - r2) + uv3 * r2);
        const vec3 lp = p1 * (1.0 - r1) + r1 * (p2 * (1.0 - r2) + p3 * r2);

        float ls_dist;
        const vec3 L = normalize_len(lp - probe_pos, ls_dist);

        float cos_theta = -dot(L, light_forward);
        if (is_doublesided) {
            cos_theta = abs(cos_theta);
        }
        const float ls_pdf = pdf_factor * (ls_dist * ls_dist) / (0.5 * light_fwd_len * cos_theta);

        if (max_component(litem.col_and_type.xyz) * g_shrd_data.cam_pos_and_exp.w / ls_pdf >= GI_LIGHT_CUTOFF) {
#ifdef HWRT
            rayQueryEXT rq;
            rayQueryInitializeEXT(rq,                       // rayQuery
                                  g_tlas,                   // topLevel
                                  0,                        // rayFlags
                                  (1u << RAY_TYPE_SHADOW),  // cullMask
                                  probe_pos,                // origin
                                  0.0,                      // tMin
                                  L,                        // direction
                                  ls_dist - 0.001           // tMax
                                  );

            int transp_depth = 0;
            while(rayQueryProceedEXT(rq) && transp_depth++ < TRANSPARENCY_LIMIT) {
                if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
                    // perform alpha test
                    const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, false);
                    const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, false);
                    const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, false);
                    const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, false);
                    const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, false);

                    const rt_geo_instance_t geo = g_geometries[custom_index + geo_index];
                    const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
                    const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                    const uint i0 = texelFetch(g_vtx_indices, int(geo.indices_start + 3 * prim_id + 0)).x;
                    const uint i1 = texelFetch(g_vtx_indices, int(geo.indices_start + 3 * prim_id + 1)).x;
                    const uint i2 = texelFetch(g_vtx_indices, int(geo.indices_start + 3 * prim_id + 2)).x;

                    const vec2 uv0 = unpackHalf2x16(floatBitsToUint(texelFetch(g_vtx_data0, int(geo.vertices_start + i0)).w));
                    const vec2 uv1 = unpackHalf2x16(floatBitsToUint(texelFetch(g_vtx_data0, int(geo.vertices_start + i1)).w));
                    const vec2 uv2 = unpackHalf2x16(floatBitsToUint(texelFetch(g_vtx_data0, int(geo.vertices_start + i2)).w));

                    const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
                    const float alpha = textureLodBindless(GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA]), uv, 0.0).x;
                    if (alpha >= 0.5) {
                        rayQueryConfirmIntersectionEXT(rq);
                    }
                }
            }
            const bool no_hit = (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT);
#else // HWRT
            hit_data_t inter;
            inter.mask = 0;
            inter.obj_index = inter.prim_index = 0;
            inter.geo_index_count = 0;
            inter.tmin = 0.0;
            inter.tmax = ls_dist - 0.001;
            inter.u = inter.v = 0.0;

            const vec3 inv_d = safe_invert(L);

            int transp_depth = 0;
            while (transp_depth++ < TRANSPARENCY_LIMIT) {
                Traverse_TLAS_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                        probe_pos, L, inv_d, (1u << RAY_TYPE_SHADOW), 0 /* root_node */, inter);
                if (inter.mask != 0) {
                    // perform alpha test
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
                        inter.tmax = (ls_dist - 0.001) - inter.tmin;
                        inter.mask = 0;
                        continue;
                    }
                #endif
                }
                break;
            }
            const bool no_hit = (inter.mask == 0);
#endif // HWRT
            if (no_hit) {
                out_dir = L;
                out_color = litem.col_and_type.xyz / (ls_pdf * M_PI);
            #if defined(BINDLESS_TEXTURES)
                out_color *= SRGBToLinear(YCoCg_to_RGB(textureLodBindless(GET_HANDLE(floatBitsToInt(litem.u_and_reg.w)), luv, 0.0)));
            #endif
            #ifdef MIS
                const float bsdf_pdf = 1.0 / (2.0 * M_PI);
                const float mis_weight = power_heuristic(PROBE_LIGHT_RAYS_FRACT * ls_pdf, bsdf_pdf);
                out_color *= mis_weight;
            #endif
            }
        }
    }

    const vec4 sh1_r = out_color.x * vec4(0.488603 * out_dir, 0.282095);
    const vec4 sh1_g = out_color.y * vec4(0.488603 * out_dir, 0.282095);
    const vec4 sh1_b = out_color.z * vec4(0.488603 * out_dir, 0.282095);

    g_sh1_r[gl_LocalInvocationIndex] = packHalf4x16(sh1_r);
    g_sh1_g[gl_LocalInvocationIndex] = packHalf4x16(sh1_g);
    g_sh1_b[gl_LocalInvocationIndex] = packHalf4x16(sh1_b);

    groupMemoryBarrier(); barrier();

    if (gl_LocalInvocationIndex == 0) {
        vec4 out_sh1_r = unpackHalf4x16(g_sh1_r[0]);
        vec4 out_sh1_g = unpackHalf4x16(g_sh1_g[0]);
        vec4 out_sh1_b = unpackHalf4x16(g_sh1_b[0]);

        for (int i = 1; i < GRP_SIZE2_X; ++i) {
            out_sh1_r += unpackHalf4x16(g_sh1_r[i]);
            out_sh1_g += unpackHalf4x16(g_sh1_g[i]);
            out_sh1_b += unpackHalf4x16(g_sh1_b[i]);
        }

        // Pre-scale for cosine convolution
        // NOTE: PI multiplier is applied at the end of irradiance evaluation
        const vec4 k = vec4(2.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 1.0) / GRP_SIZE2_X;

        out_sh1_r *= k;
        out_sh1_g *= k;
        out_sh1_b *= k;

        g_sh1_data[probe_index * 3 + 0] = packHalf4x16(out_sh1_r);
        g_sh1_data[probe_index * 3 + 1] = packHalf4x16(out_sh1_g);
        g_sh1_data[probe_index * 3 + 2] = packHalf4x16(out_sh1_b);
    }
}
