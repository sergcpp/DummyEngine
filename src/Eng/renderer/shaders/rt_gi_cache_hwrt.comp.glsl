#version 460
#extension GL_EXT_control_flow_attributes : require
#extension GL_EXT_ray_query : require
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#define ENABLE_SHEEN 0
#define ENABLE_CLEARCOAT 0
#define MIN_SPEC_ROUGHNESS 0.4

#define MAX_STACK_SIZE 10

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "gi_common.glsl"
#include "gi_cache_common.glsl"
#include "pmj_common.glsl"
#include "light_bvh_common.glsl"

#include "rt_gi_cache_interface.h"

#pragma multi_compile _ STOCH_LIGHTS STOCH_LIGHTS_MIS
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

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

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

layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer LightsData {
    _light_item_t g_lights[];
};

#if defined(STOCH_LIGHTS) || defined(STOCH_LIGHTS_MIS)
    layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;
    layout(binding = STOCH_LIGHTS_BUF_SLOT) uniform usamplerBuffer g_stoch_lights_buf;
    layout(binding = LIGHT_NODES_BUF_SLOT) uniform samplerBuffer g_light_nodes_buf;
#endif

layout(binding = CELLS_BUF_SLOT) uniform usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform usamplerBuffer g_items_buf;

layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2D g_shadow_color_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(binding = OUT_RAY_DATA_IMG_SLOT, rgba16f) uniform restrict writeonly image2DArray g_out_ray_data_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

vec3 LightVisibility(const _light_item_t litem, const vec3 P) {
    int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
    if (shadowreg_index == -1) {
        return vec3(1.0);
    }

    const vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
    shadowreg_index += cubemap_face(from_light, litem.dir_and_spot.xyz, normalize(litem.u_and_reg.xyz), normalize(litem.v_and_blend.xyz));
    vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

    vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(P, 1.0);
    pp /= pp.w;

    pp.xy = pp.xy * 0.5 + 0.5;
    pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
#if defined(VULKAN)
    pp.y = 1.0 - pp.y;
#endif // VULKAN

    return textureLod(g_shadow_depth_tex, pp.xyz, 0.0) * textureLod(g_shadow_color_tex, pp.xy, 0.0).xyz;
}

void main() {
    const int ray_index = int(gl_GlobalInvocationID.x);
    const int probe_plane_index = int(gl_GlobalInvocationID.y);
    const int plane_index = int(gl_GlobalInvocationID.z);

    int probe_index = (plane_index * PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z) + probe_plane_index;

    const ivec3 probe_coords = get_probe_coords(probe_index);
    probe_index = get_scrolling_probe_index(probe_coords, g_params.grid_scroll.xyz);

    const ivec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
    const bool is_inactive = ray_index >= PROBE_FIXED_RAYS_COUNT && texelFetch(g_offset_tex, tex_coords, 0).w < 0.5;
#ifdef PARTIAL
    const ivec3 oct_index = get_probe_coords(probe_index) & 1;
    const bool is_wrong_oct = (oct_index.x | (oct_index.y << 1) | (oct_index.z << 2)) != g_params.oct_index;
#else
    const bool is_wrong_oct = false;
#endif

    if (!IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz) && (is_inactive || is_wrong_oct)) {
        return;
    }

    const vec3 probe_pos = get_probe_pos_ws(g_params.volume_index, probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz, g_offset_tex);
    const vec3 probe_ray_dir = get_probe_ray_dir(ray_index, g_params.quat_rot);
    const ivec3 output_coords = get_ray_data_coords(ray_index, probe_index);

    rayQueryEXT rq;

#if defined(STOCH_LIGHTS) || defined(STOCH_LIGHTS_MIS)
    { // direct light sampling
        vec3 out_color = vec3(0.0), out_dir = vec3(0.0);

        const uint px_hash = g_params.pass_hash;
        const float light_pick_rand = get_scrambled_2d_rand(g_random_seq, RAND_DIM_LIGHT_PICK, px_hash, ray_index).x;

        float pdf_factor;
        const int li = PickLightSource(probe_pos, g_light_nodes_buf, g_params.stoch_lights_count, light_pick_rand, pdf_factor);
        if (li != -1) {
            const _light_item_t litem = FetchLightItem(g_stoch_lights_buf, li);
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
                while(rayQueryProceedEXT(rq) && transp_depth++ < 4) {
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

                        const uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
                        const uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
                        const uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

                        const vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
                        const vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
                        const vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

                        const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;
                        const float alpha = textureLodBindless(mat.texture_indices[MAT_TEX_ALPHA], uv, 0.0).x;
                        if (alpha >= 0.5) {
                            rayQueryConfirmIntersectionEXT(rq);
                        }
                    }
                }

                if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
                    out_dir = L;
                    out_color = litem.col_and_type.xyz / (ls_pdf * M_PI);
                    out_color *= SRGBToLinear(YCoCg_to_RGB(textureLodBindless(floatBitsToInt(litem.u_and_reg.w), luv, 0.0)));
                #ifdef STOCH_LIGHTS_MIS
                    const float bsdf_pdf = 1.0 / (2.0 * M_PI);
                    const float mis_weight = power_heuristic(ls_pdf, bsdf_pdf);
                    out_color *= mis_weight;
                #endif
                }
            }
        }

        imageStore(g_out_ray_data_img, output_coords + ivec3(0, 0, 2 * PROBE_VOLUME_RES_Y), vec4(compress_hdr(out_color, g_shrd_data.cam_pos_and_exp.w), 0));
        imageStore(g_out_ray_data_img, output_coords + ivec3(0, 0, 3 * PROBE_VOLUME_RES_Y), vec4(out_dir, 0));
    }
#endif

    vec3 ro = probe_pos;

    const uint ray_flags = 0;//gl_RayFlagsCullBackFacingTrianglesEXT;
    const float t_min = 0.0;
    const float t_max = 100.0;

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

            const uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
            const uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
            const uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

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

    vec3 final_diffuse_only = vec3(0.0), final_total = vec3(0.0);
    float final_distance = 0.0;

    if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        // Check portal lights intersection
        for (int i = 0; i < MAX_PORTALS_TOTAL && g_shrd_data.portals[i / 4][i % 4] != 0xffffffff; ++i) {
            const _light_item_t litem = g_lights[g_shrd_data.portals[i / 4][i % 4]];

            const vec3 light_pos = litem.pos_and_radius.xyz;
            vec3 light_u = litem.u_and_reg.xyz, light_v = litem.v_and_blend.xyz;
            const vec3 light_forward = normalize(cross(light_u, light_v));

            const float plane_dist = dot(light_forward, light_pos);
            const float cos_theta = dot(probe_ray_dir, light_forward);
            const float t = (plane_dist - dot(light_forward, probe_pos)) / min(cos_theta, -FLT_EPS);

            if (cos_theta < 0.0 && t > 0.0) {
                light_u /= dot(light_u, light_u);
                light_v /= dot(light_v, light_v);

                const vec3 p = probe_pos + probe_ray_dir * t;
                const vec3 vi = p - light_pos;
                const float a1 = dot(light_u, vi);
                if (a1 >= -1.0 && a1 <= 1.0) {
                    const float a2 = dot(light_v, vi);
                    if (a2 >= -1.0 && a2 <= 1.0) {
                        throughput = vec3(0.0);
                        break;
                    }
                }
            }
        }

        const vec3 rotated_dir = rotate_xz(probe_ray_dir, g_shrd_data.env_col.w);
        const float env_mip_count = g_shrd_data.ambient_hack.w;

        final_diffuse_only = final_total = throughput * g_shrd_data.env_col.xyz * textureLod(g_env_tex, rotated_dir, env_mip_count - 4.0).xyz;
        final_distance = 1e27;
    } else {
        const int custom_index = rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true);
        const int geo_index = rayQueryGetIntersectionGeometryIndexEXT(rq, true);
        const float hit_t = rayQueryGetIntersectionTEXT(rq, true);
        const int prim_id = rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
        const vec2 bary_coord = rayQueryGetIntersectionBarycentricsEXT(rq, true);
        const bool backfacing = !rayQueryGetIntersectionFrontFaceEXT(rq, true);
        const mat4x3 world_from_object = rayQueryGetIntersectionObjectToWorldEXT(rq, true);

        const rt_geo_instance_t geo = g_geometries[custom_index + geo_index];
        const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
        const material_data_t mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

        const uint i0 = g_indices[geo.indices_start + 3 * prim_id + 0];
        const uint i1 = g_indices[geo.indices_start + 3 * prim_id + 1];
        const uint i2 = g_indices[geo.indices_start + 3 * prim_id + 2];

        vec4 p0 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i0]);
        vec4 p1 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i1]);
        vec4 p2 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i2]);
        p0.xyz = (world_from_object * vec4(p0.xyz, 1.0)).xyz;
        p1.xyz = (world_from_object * vec4(p1.xyz, 1.0)).xyz;
        p2.xyz = (world_from_object * vec4(p2.xyz, 1.0)).xyz;

        const vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
        const vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
        const vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

        const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;

        const vec2 tex_res = textureSizeBindless(mat.texture_indices[MAT_TEX_BASECOLOR], 0).xy;
        const float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

        vec3 tri_normal = cross(p1.xyz - p0.xyz, p2.xyz - p0.xyz);
        if (backfacing) {
            tri_normal = -tri_normal;
        }
        float pa = length(tri_normal);
        tri_normal /= pa;

        float cone_width = 0.0;//_cone_width + g_params.pixel_spread_angle * hit_t;

        float tex_lod = 0.5 * log2(ta / pa);
        tex_lod += log2(cone_width);
        tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
        tex_lod -= log2(abs(dot(probe_ray_dir, tri_normal)));
        tex_lod += TEX_LOD_OFFSET;
        vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLodBindless(mat.texture_indices[MAT_TEX_BASECOLOR], uv, tex_lod)));

        // Use triangle normal for simplicity
        const vec3 N = tri_normal;

        const vec3 P = ro + probe_ray_dir * hit_t;
        const vec3 I = -probe_ray_dir;
        const float N_dot_V = saturate(dot(N, I));

        vec3 tint_color = vec3(0.0);

        const float base_color_lum = lum(base_color);
        if (base_color_lum > 0.0) {
            tint_color = base_color / base_color_lum;
        }

#if defined(BINDLESS_TEXTURES)
        const float roughness = mat.params[0].w * textureLodBindless(mat.texture_indices[MAT_TEX_ROUGHNESS], uv, tex_lod).x;
#else
        const float roughness = mat.params[0].w;
#endif
        const float sheen = mat.params[1].x;
        const float sheen_tint = mat.params[1].y;
        const float specular = mat.params[1].z;
        const float specular_tint = mat.params[1].w;
#if defined(BINDLESS_TEXTURES)
        const float metallic = mat.params[2].x * textureLodBindless(mat.texture_indices[MAT_TEX_METALLIC], uv, tex_lod).x;
#else
        const float metallic = mat.params[2].x;
#endif
        const float transmission = mat.params[2].y;
        const float clearcoat = mat.params[2].z;
        const float clearcoat_roughness = mat.params[2].w;
        vec3 emission_color = vec3(0.0);
#if defined(STOCH_LIGHTS_MIS)
        if (transmission < 0.001) {
#if defined(BINDLESS_TEXTURES)
            emission_color = mat.params[3].yzw * SRGBToLinear(YCoCg_to_RGB(textureLodBindless(mat.texture_indices[MAT_TEX_EMISSION], uv, tex_lod)));
#else
            emission_color = mat.params[3].yzw;
#endif
        }
#endif // STOCH_LIGHTS_MIS

        vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
        spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

        const float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
        const float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

        // Approximation of FH (using shading normal)
        const float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

        const vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
        const float spec_color_lum = lum(approx_spec_col);

        const lobe_masks_t lobe_masks = get_lobe_masks(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

        const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

        const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
        const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
        const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

        // Approximation of FH (using shading normal)
        const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
        const vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

        const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

#if defined(STOCH_LIGHTS_MIS)
        if (max_component(emission_color) * g_shrd_data.cam_pos_and_exp.w > 1e-7) {
            const uint tri_index = (geo.indices_start / 3) + prim_id;
            const float pdf_factor = EvalTriLightFactor(P, g_light_nodes_buf, g_stoch_lights_buf, g_params.stoch_lights_count, tri_index, probe_pos);

            const vec3 e1 = p1.xyz - p0.xyz, e2 = p2.xyz - p0.xyz;
            float light_fwd_len;
            const vec3 light_forward = normalize_len(cross(e1, e2), light_fwd_len);
            const float cos_theta = -dot(I, light_forward);

            const float bsdf_pdf = 1.0 / (2.0 * M_PI);
            const float ls_pdf = pdf_factor * (hit_t * hit_t) / (0.5 * light_fwd_len * cos_theta);
            const float mis_weight = power_heuristic(bsdf_pdf, ls_pdf);
            emission_color *= mis_weight;
        }
#endif
        vec3 light_total = emission_color;

        vec4 projected_p = g_shrd_data.rt_clip_from_world * vec4(P, 1.0);
        projected_p /= projected_p[3];
        projected_p.xy = projected_p.xy * 0.5 + 0.5;

        const float lin_depth = LinearizeDepth(projected_p.z, g_shrd_data.rt_clip_info);
        const float k = log2(lin_depth / g_shrd_data.rt_clip_info[1]) / g_shrd_data.rt_clip_info[3];
        const int tile_x = clamp(int(projected_p.x * ITEM_GRID_RES_X), 0, ITEM_GRID_RES_X - 1),
                  tile_y = clamp(int(projected_p.y * ITEM_GRID_RES_Y), 0, ITEM_GRID_RES_Y - 1),
                  tile_z = clamp(int(k * ITEM_GRID_RES_Z), 0, ITEM_GRID_RES_Z - 1);

        const int cell_index = tile_z * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + tile_y * ITEM_GRID_RES_X + tile_x;
#if !defined(NO_SUBGROUP)
        [[dont_flatten]] if (subgroupAllEqual(cell_index)) {
            const int s_first_cell_index = subgroupBroadcastFirst(cell_index);
            const uvec2 s_cell_data = texelFetch(g_cells_buf, s_first_cell_index).xy;
            const uvec2 s_offset_and_lcount = uvec2(bitfieldExtract(s_cell_data.x, 0, 24), bitfieldExtract(s_cell_data.x, 24, 8));
            const uvec2 s_dcount_and_pcount = uvec2(bitfieldExtract(s_cell_data.y, 0, 8), bitfieldExtract(s_cell_data.y, 8, 8));
            for (uint i = s_offset_and_lcount.x; i < s_offset_and_lcount.x + s_offset_and_lcount.y; ++i) {
                const uint s_item_data = texelFetch(g_items_buf, int(i)).x;
                const int s_li = int(bitfieldExtract(s_item_data, 0, 12));

                const _light_item_t litem = g_lights[subgroupBroadcastFirst(s_li)];

                const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;

                vec3 light_contribution = EvaluateLightSource_Approx(litem, P, I, N, lobe_masks, roughness, base_color, approx_spec_col);
                light_contribution = max(light_contribution, vec3(0.0)); // ???
                if (max_component(light_contribution) < FLT_EPS) {
                    continue;
                }
                if (is_portal) {
                    // Sample environment to create slight color variation
                    const vec3 rotated_dir = rotate_xz(normalize(litem.pos_and_radius.xyz - P), g_shrd_data.env_col.w);
                    light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 4.0).xyz;
                }
                light_total += light_contribution * LightVisibility(litem, P);
            }
        } else
#endif // !defined(NO_SUBGROUP)
        {
            const uvec2 v_cell_data = texelFetch(g_cells_buf, cell_index).xy;
            const uvec2 v_offset_and_lcount = uvec2(bitfieldExtract(v_cell_data.x, 0, 24), bitfieldExtract(v_cell_data.x, 24, 8));
            const uvec2 v_dcount_and_pcount = uvec2(bitfieldExtract(v_cell_data.y, 0, 8), bitfieldExtract(v_cell_data.y, 8, 8));
            for (uint i = v_offset_and_lcount.x; i < v_offset_and_lcount.x + v_offset_and_lcount.y; ) {
                const uint v_item_data = texelFetch(g_items_buf, int(i)).x;
                const int v_li = int(bitfieldExtract(v_item_data, 0, 12));
                const int s_li = subgroupMin(v_li);
                [[flatten]] if (s_li == v_li) {
                    ++i;
                    const _light_item_t litem = g_lights[s_li];

                    const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;

                    vec3 light_contribution = EvaluateLightSource_Approx(litem, P, I, N, lobe_masks, roughness, base_color, approx_spec_col);
                    light_contribution = max(light_contribution, vec3(0.0)); // ???
                    if (max_component(light_contribution) < FLT_EPS) {
                        continue;
                    }
                    if (is_portal) {
                        // Sample environment to create slight color variation
                        const vec3 rotated_dir = rotate_xz(normalize(litem.pos_and_radius.xyz - P), g_shrd_data.env_col.w);
                        light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 4.0).xyz;
                    }
                    light_total += light_contribution * LightVisibility(litem, P);
                }
            }
        }

        if (dot(g_shrd_data.sun_col.xyz, g_shrd_data.sun_col.xyz) > 0.0) {
            vec4 pos_ws = vec4(P, 1.0);

            const vec2 shadow_offsets = get_shadow_offsets(saturate(dot(N, g_shrd_data.sun_dir.xyz)));
            pos_ws.xyz += 0.01 * shadow_offsets.x * N;
            pos_ws.xyz += 0.002 * shadow_offsets.y * g_shrd_data.sun_dir.xyz;

            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[3].clip_from_world * pos_ws).xyz;
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += vec2(0.25, 0.5);
    #if defined(VULKAN)
            shadow_uvs.y = 1.0 - shadow_uvs.y;
    #endif // VULKAN

            const vec3 sun_visibility = textureLod(g_shadow_depth_tex, shadow_uvs, 0.0) * textureLod(g_shadow_color_tex, shadow_uvs.xy, 0.0).xyz;
            if (max_component(sun_visibility) > 0.0) {
                light_total += sun_visibility * EvaluateSunLight_Approx(g_shrd_data.sun_col_point_sh.xyz, g_shrd_data.sun_dir.xyz, g_shrd_data.sun_dir.w,
                                                                        I, N, lobe_masks, roughness, clearcoat_roughness2,
                                                                        base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
            }
        }

        final_distance = backfacing ? -hit_t : hit_t;
        final_diffuse_only += light_total;
        final_total += light_total;

        for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
            const float weight = get_volume_blend_weight(P, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
            if (weight > 0.0) {
                if ((lobe_masks.bits & LOBE_DIFFUSE_BIT) != 0) {
                    // diffuse-only
                    vec3 irradiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(N, probe_ray_dir, g_shrd_data.probe_volumes[i].spacing.xyz), N,
                                                                g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, false);
                    irradiance *= base_color * ltc.diff_t2.x;
                    irradiance *= clamp(hit_t / (0.5 * length(g_shrd_data.probe_volumes[i].spacing.xyz)), 0.0, 1.0);
                    final_diffuse_only += lobe_masks.diffuse_mul * (1.0 / M_PI) * irradiance;
                    final_total += GI_CACHE_MULTIBOUNCE_FACTOR * lobe_masks.diffuse_mul * (1.0 / M_PI) * irradiance;
                }
                if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0) {
                    const vec3 refl_dir = reflect(probe_ray_dir, N);
                    vec3 avg_radiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(N, probe_ray_dir, g_shrd_data.probe_volumes[i].spacing.xyz), refl_dir,
                                                                  g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, false);
                    avg_radiance *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
                    avg_radiance *= saturate(hit_t / (0.5 * length(g_shrd_data.probe_volumes[i].spacing.xyz)));
                    final_total += GI_CACHE_MULTIBOUNCE_FACTOR * (1.0 / M_PI) * avg_radiance;
                }
                break;
            }
        }
    }

    final_diffuse_only = compress_hdr(final_diffuse_only, g_shrd_data.cam_pos_and_exp.w);
    final_total = compress_hdr(final_total, g_shrd_data.cam_pos_and_exp.w);

    imageStore(g_out_ray_data_img, output_coords, vec4(final_total, final_distance));
    imageStore(g_out_ray_data_img, output_coords + ivec3(0, 0, PROBE_VOLUME_RES_Y), vec4(final_diffuse_only, final_distance));
}