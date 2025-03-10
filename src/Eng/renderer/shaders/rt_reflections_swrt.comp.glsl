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

#define ENABLE_SHEEN 0

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "swrt_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "ssr_common.glsl"
#include "gi_cache_common.glsl"
#include "light_bvh_common.glsl"

#include "rt_reflections_interface.h"

#pragma multi_compile _ LAYERED STOCH_LIGHTS
#pragma multi_compile _ TWO_BOUNCES FOUR_BOUNCES
#pragma multi_compile _ GI_CACHE
#pragma multi_compile _ NO_SUBGROUP

#if defined(LAYERED) && (defined(TWO_BOUNCES) || defined(FOUR_BOUNCES))
    #pragma dont_compile
#endif

#if defined(FOUR_BOUNCES)
    #define NUM_BOUNCES 4
#elif defined(TWO_BOUNCES)
    #define NUM_BOUNCES 2
#else
    #define NUM_BOUNCES 1
#endif

#if defined(NO_SUBGROUP)
    #define subgroupMin(x) (x)
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = BLAS_BUF_SLOT) uniform samplerBuffer g_blas_nodes;
layout(binding = TLAS_BUF_SLOT) uniform samplerBuffer g_tlas_nodes;

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(binding = VTX_BUF1_SLOT) uniform samplerBuffer g_vtx_data0;
layout(binding = VTX_BUF2_SLOT) uniform usamplerBuffer g_vtx_data1;
layout(binding = NDX_BUF_SLOT) uniform usamplerBuffer g_vtx_indices;

layout(binding = PRIM_NDX_BUF_SLOT) uniform usamplerBuffer g_prim_indices;
layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;

layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer LightsData {
    light_item_t g_lights[];
};

layout(binding = CELLS_BUF_SLOT) uniform usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform usamplerBuffer g_items_buf;

#ifdef STOCH_LIGHTS
    layout(binding = STOCH_LIGHTS_BUF_SLOT) uniform samplerBuffer g_stoch_lights_buf;
    layout(binding = LIGHT_NODES_BUF_SLOT) uniform samplerBuffer g_light_nodes_buf;
#endif

layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2DShadow g_shadow_color_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

#ifdef GI_CACHE
    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(std430, binding = RAY_COUNTER_SLOT) readonly buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

#ifdef LAYERED
    layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;
#else
    layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;
#endif

#ifdef LAYERED
    layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_color_img[OIT_REFLECTION_LAYERS];
#else
    layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform restrict image2D g_out_color_img;
#endif

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

float LightVisibility(const light_item_t litem, const vec3 P) {
    int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
    if (shadowreg_index == -1) {
        return 1.0;
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

    return SampleShadowPCF5x5(g_shadow_depth_tex, pp.xyz);
}

void main() {
    const uint ray_index = gl_WorkGroupID.x * LOCAL_GROUP_SIZE_X + gl_LocalInvocationIndex;
    if (ray_index >= g_ray_counter[7]) return;

#ifndef LAYERED
    const uint packed_coords = g_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    const ivec2 icoord = ivec2(ray_coords);
    const float depth = texelFetch(g_depth_tex, icoord, 0).r;
    const vec4 normal_roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x);
    const vec3 normal_ws = normal_roughness.xyz;
    const vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    const float first_roughness = normal_roughness.w * normal_roughness.w;

    const vec2 px_center = vec2(icoord) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);

    const vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;

    const vec3 view_ray_vs = normalize(ray_origin_vs);
    const vec4 u = texelFetch(g_noise_tex, icoord % 128, 0);
    const vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, first_roughness, u.xy);
    vec3 refl_ray_ws = (g_shrd_data.world_from_view * vec4(refl_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.world_from_view * vec4(ray_origin_vs, 1.0);
    ray_origin_ws /= ray_origin_ws.w;

    ray_origin_ws.xyz += (NormalBiasConstant + abs(ray_origin_ws.xyz) * NormalBiasPosAddition + view_z * NormalBiasViewAddition) * normal_ws;
#else
    const uint packed_coords = g_ray_list[2 * ray_index + 0];
    const uint packed_dir = g_ray_list[2 * ray_index + 1];

    uvec2 ray_coords;
    uint layer_index;
    UnpackRayCoords(packed_coords, ray_coords, layer_index);

    const vec2 oct_dir = vec2(packed_dir & 0xffffu, (packed_dir >> 16) & 0xffffu) / 65535.0;
    vec3 refl_ray_ws = UnpackUnitVector(oct_dir);

    int frag_index = int(layer_index) * g_shrd_data.ires_and_ifres.x * g_shrd_data.ires_and_ifres.y;
    frag_index += int(ray_coords.y) * g_shrd_data.ires_and_ifres.x + int(ray_coords.x);
    const float depth = uintBitsToFloat(texelFetch(g_oit_depth_buf, frag_index).x);
    const float first_roughness = 0.0;

    const ivec2 icoord = ivec2(ray_coords);
    const vec2 norm_uvs = (vec2(ray_coords) + 0.5) / g_shrd_data.res_and_fres.xy;
    const vec3 ray_origin_ss = vec3(norm_uvs, depth);
    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;

    vec4 ray_origin_ws = g_shrd_data.world_from_view * vec4(ray_origin_vs, 1.0);
    ray_origin_ws /= ray_origin_ws.w;

    vec4 u = vec4(0.0);
#endif

    const float t_min = 0.001;
    const float t_max = 100.0;

    const float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);
    const float portals_specular_ltc_weight = smoothstep(0.0, 0.25, first_roughness);

    float first_ray_len = 100.0, total_ray_len = 0.0;
    vec3 throughput = vec3(1.0);
    vec3 final_color = vec3(0.0);

    for (int j = 0; j < NUM_BOUNCES; ++j) {
        const bool is_last_bounce = (j == NUM_BOUNCES - 1);

        hit_data_t inter;
        inter.mask = 0;
        inter.obj_index = inter.prim_index = 0;
        inter.geo_index_count = 0;
        inter.t = t_max;
        inter.u = inter.v = 0.0;

        vec3 ro = ray_origin_ws.xyz + t_min * refl_ray_ws;
        const vec3 inv_d = safe_invert(refl_ray_ws);

        int transp_depth = 0;
        while (true) {
            Traverse_TLAS_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                    ro, refl_ray_ws, inv_d, (1u << RAY_TYPE_SPECULAR), 0 /* root_node */, inter);
            if (inter.mask != 0 && transp_depth++ < 4) {
                // perform alpha test, account for alpha blending
                const bool backfacing = (inter.prim_index < 0);
                const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

                int i = int(inter.geo_index_count & 0x00ffffffu);
                for (; i < int(inter.geo_index_count & 0x00ffffffu) + int((inter.geo_index_count >> 24) & 0xffu); ++i) {
                    const int tri_start = int(g_geometries[i].indices_start) / 3;
                    if (tri_start > tri_index) {
                        break;
                    }
                }

                const int geo_index = i - 1;

                const RTGeoInstance geo = g_geometries[geo_index];
                const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
                if ((mat_index & MATERIAL_SOLID_BIT) != 0) {
                    break;
                }
                const MaterialData mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

                const uint i0 = texelFetch(g_vtx_indices, 3 * tri_index + 0).x;
                const uint i1 = texelFetch(g_vtx_indices, 3 * tri_index + 1).x;
                const uint i2 = texelFetch(g_vtx_indices, 3 * tri_index + 2).x;

                const vec4 p0 = texelFetch(g_vtx_data0, int(geo.vertices_start + i0));
                const vec4 p1 = texelFetch(g_vtx_data0, int(geo.vertices_start + i1));
                const vec4 p2 = texelFetch(g_vtx_data0, int(geo.vertices_start + i2));

                const vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
                const vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
                const vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

                const vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;
    #if defined(BINDLESS_TEXTURES)
                const float alpha = (1.0 - mat.params[3].x) * textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA])), uv, 0.0).r;
                if (alpha < 0.5) {
                    ro += (inter.t + 0.0005) * refl_ray_ws;
                    inter.mask = 0;
                    inter.t = 100.0;
                    continue;
                }
                if (mat.params[2].y > 0) {
                    const vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_BASECOLOR])), uv, 0.0)));
                    throughput = min(throughput, mix(vec3(1.0), 0.8 * mat.params[2].y * base_color, alpha));
                    if (dot(throughput, vec3(0.333)) > 0.1) {
                        ro += (inter.t + 0.0005) * refl_ray_ws;
                        inter.mask = 0;
                        inter.t = 100.0;
                        continue;
                    }
                }
    #endif
            }
            break;
        }

        if (inter.mask == 0) {
            // Check portal lights intersection (rough rays are blocked by them)
            for (int i = 0; i < MAX_PORTALS_TOTAL && g_shrd_data.portals[i / 4][i % 4] != 0xffffffff; ++i) {
                const light_item_t litem = g_lights[g_shrd_data.portals[i / 4][i % 4]];

                const vec3 light_pos = litem.pos_and_radius.xyz;
                vec3 light_u = litem.u_and_reg.xyz, light_v = litem.v_and_blend.xyz;
                const vec3 light_forward = normalize(cross(light_u, light_v));

                const float plane_dist = dot(light_forward, light_pos);
                const float cos_theta = dot(refl_ray_ws, light_forward);
                const float t = (plane_dist - dot(light_forward, ray_origin_ws.xyz)) / min(cos_theta, -FLT_EPS);

                if (cos_theta < 0.0 && t > 0.0) {
                    light_u /= dot(light_u, light_u);
                    light_v /= dot(light_v, light_v);

                    const vec3 p = ray_origin_ws.xyz + refl_ray_ws * t;
                    const vec3 vi = p - light_pos;
                    const float a1 = dot(light_u, vi);
                    if (a1 >= -1.0 && a1 <= 1.0) {
                        const float a2 = dot(light_v, vi);
                        if (a2 >= -1.0 && a2 <= 1.0) {
                            throughput *= (1.0 - portals_specular_ltc_weight);
                            break;
                        }
                    }
                }
            }

            const vec3 rotated_dir = rotate_xz(refl_ray_ws, g_shrd_data.env_col.w);
            const float lod = 8.0 * first_roughness;
            final_color += throughput * g_shrd_data.env_col.xyz * textureLod(g_env_tex, rotated_dir, lod).rgb;
            break;
        } else {
            const bool backfacing = (inter.prim_index < 0);
            const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

            int i = int(inter.geo_index_count & 0x00ffffffu);
            for (; i < int(inter.geo_index_count & 0x00ffffffu) + int((inter.geo_index_count >> 24) & 0xffu); ++i) {
                const int tri_start = int(g_geometries[i].indices_start) / 3;
                if (tri_start > tri_index) {
                    break;
                }
            }

            const int geo_index = i - 1;

            const RTGeoInstance geo = g_geometries[geo_index];
            const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
            const MaterialData mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

            const uint i0 = texelFetch(g_vtx_indices, 3 * tri_index + 0).x;
            const uint i1 = texelFetch(g_vtx_indices, 3 * tri_index + 1).x;
            const uint i2 = texelFetch(g_vtx_indices, 3 * tri_index + 2).x;

            vec4 p0 = texelFetch(g_vtx_data0, int(geo.vertices_start + i0));
            vec4 p1 = texelFetch(g_vtx_data0, int(geo.vertices_start + i1));
            vec4 p2 = texelFetch(g_vtx_data0, int(geo.vertices_start + i2));

            const vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
            const vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
            const vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

            const vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;

            const mat4x3 world_from_object = transpose(mat3x4(texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 4)),
                                                              texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 5)),
                                                              texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 6))));
            p0.xyz = (world_from_object * vec4(p0.xyz, 1.0)).xyz;
            p1.xyz = (world_from_object * vec4(p1.xyz, 1.0)).xyz;
            p2.xyz = (world_from_object * vec4(p2.xyz, 1.0)).xyz;

            vec3 tri_normal = cross(p1.xyz - p0.xyz, p2.xyz - p0.xyz);
            const float pa = length(tri_normal);
            tri_normal /= pa;
            if (backfacing) {
                tri_normal = -tri_normal;
            }

    #if defined(BINDLESS_TEXTURES)
            const vec2 tex_res = textureSize(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_BASECOLOR])), 0).xy;
            const float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

            total_ray_len += inter.t;
            float cone_width = _cone_width + g_params.pixel_spread_angle * total_ray_len;

            float tex_lod = 0.5 * log2(ta / pa);
            tex_lod += log2(cone_width);
            tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
            tex_lod -= log2(abs(dot(refl_ray_ws, tri_normal)));
            tex_lod += mix(TEX_LOD_OFFSET_MIN, TEX_LOD_OFFSET_MAX, first_roughness);
            vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_BASECOLOR])), uv, tex_lod)));
    #else
            // TODO: Fallback to shared texture atlas
            vec3 base_color = vec3(1.0);
            float tex_lod = 0.0;
    #endif

            const uvec2 packed0 = texelFetch(g_vtx_data1, int(geo.vertices_start + i0)).xy;
            const uvec2 packed1 = texelFetch(g_vtx_data1, int(geo.vertices_start + i1)).xy;
            const uvec2 packed2 = texelFetch(g_vtx_data1, int(geo.vertices_start + i2)).xy;

            const vec3 normal0 = vec3(unpackSnorm2x16(packed0.x), unpackSnorm2x16(packed0.y).x);
            const vec3 normal1 = vec3(unpackSnorm2x16(packed1.x), unpackSnorm2x16(packed1.y).x);
            const vec3 normal2 = vec3(unpackSnorm2x16(packed2.x), unpackSnorm2x16(packed2.y).x);

            vec3 N = normal0 * (1.0 - inter.u - inter.v) + normal1 * inter.u + normal2 * inter.v;
            if (backfacing) {
                N = -N;
            }
            N = normalize((world_from_object * vec4(N, 0.0)).xyz);

            const vec3 P = ray_origin_ws.xyz + refl_ray_ws * (t_min + inter.t);
            const vec3 I = -refl_ray_ws;
            const float N_dot_V = saturate(dot(N, I));

            vec3 tint_color = vec3(0.0);

            const float base_color_lum = lum(base_color);
            if (base_color_lum > 0.0) {
                tint_color = base_color / base_color_lum;
            }

#if defined(BINDLESS_TEXTURES)
            const float roughness = mat.params[0].w * textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_ROUGHNESS])), uv, tex_lod).r;
#else
            const float roughness = mat.params[0].w;
#endif
            const float sheen = mat.params[1].x;
            const float sheen_tint = mat.params[1].y;
            const float specular = mat.params[1].z;
            const float specular_tint = mat.params[1].w;
#if defined(BINDLESS_TEXTURES)
            const float metallic = mat.params[2].x * textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_METALLIC])), uv, tex_lod).r;
#else
            const float metallic = mat.params[2].x;
#endif
            const float transmission = mat.params[2].y;
            const float clearcoat = mat.params[2].z;
            const float clearcoat_roughness = mat.params[2].w;
            vec3 emission_color = vec3(0.0);
            if (transmission < 0.001) {
#if defined(BINDLESS_TEXTURES)
                emission_color = mat.params[3].yzw * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[MAT_TEX_EMISSION])), uv, tex_lod)));
#else
                emission_color = mat.params[3].yzw;
#endif
            }

            vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
            spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

            const float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
            const float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

            // Approximation of FH (using shading normal)
            const float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

            const vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
            const float spec_color_lum = lum(approx_spec_col);

            const lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

            const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

            const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
            const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
            const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

            // Approximation of FH (using shading normal)
            const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
            const vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

            const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

#ifdef STOCH_LIGHTS
            if (j == 0 && hsum(emission_color) > 1e-7 && first_roughness > 1e-7) {
                const float pdf_factor = EvalTriLightFactor(P, g_light_nodes_buf, g_stoch_lights_buf, g_params.lights_count, uint(tri_index), ray_origin_ws.xyz);

                const vec3 e1 = p1.xyz - p0.xyz, e2 = p2.xyz - p0.xyz;
                float light_fwd_len;
                vec3 light_forward = normalize_len(cross(e1, e2), light_fwd_len);
                const float cos_theta = -dot(I, light_forward);

                const mat3 tbn_transform = CreateTBN(normal_ws);
                const vec3 view_ray_ws = (g_shrd_data.world_from_view * vec4(view_ray_vs.xyz, 0.0)).xyz;
                const vec3 view_dir_ts = tbn_transform * (-view_ray_ws);
                const vec3 sampled_normal_ts = tbn_transform * normalize(refl_ray_ws - view_ray_ws);

                const float D = D_GGX(sampled_normal_ts, vec2(first_roughness));
                const float bsdf_pdf = GGX_VNDF_Reflection_Bounded_PDF(D, view_dir_ts, vec2(first_roughness));
                const float ls_pdf = pdf_factor * (inter.t * inter.t) / (0.5 * light_fwd_len * cos_theta);
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

                    const light_item_t litem = g_lights[s_li];

                    const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
                    const bool is_diffuse = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DIFFUSE_BIT) != 0;
                    const bool is_specular = (floatBitsToUint(litem.col_and_type.w) & LIGHT_SPECULAR_BIT) != 0;

                    lobe_weights_t _lobe_weights = lobe_weights;
                    if (!is_last_bounce && is_portal) {
                        // Portal lights affect only diffuse
                        _lobe_weights.specular *= portals_specular_ltc_weight;
                        _lobe_weights.clearcoat *= portals_specular_ltc_weight;
                    }
                    if (!is_diffuse) _lobe_weights.diffuse = 0.0;
                    if (!is_specular) _lobe_weights.specular = _lobe_weights.clearcoat = 0.0;
                    vec3 light_contribution = EvaluateLightSource_LTC(litem, P, I, N, _lobe_weights, ltc, g_ltc_luts,
                                                                      sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                    if (all(equal(light_contribution, vec3(0.0)))) {
                        continue;
                    }
                    if (is_portal) {
                        // Sample environment to create slight color variation
                        const vec3 rotated_dir = rotate_xz(normalize(litem.pos_and_radius.xyz - P), g_shrd_data.env_col.w);
                        light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).rgb;
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
                        const light_item_t litem = g_lights[s_li];

                        const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
                        const bool is_diffuse = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DIFFUSE_BIT) != 0;
                        const bool is_specular = (floatBitsToUint(litem.col_and_type.w) & LIGHT_SPECULAR_BIT) != 0;

                        lobe_weights_t _lobe_weights = lobe_weights;
                        if (!is_last_bounce && is_portal) {
                            // Portal lights affect only diffuse
                            _lobe_weights.specular *= portals_specular_ltc_weight;
                            _lobe_weights.clearcoat *= portals_specular_ltc_weight;
                        }
                        if (!is_diffuse) _lobe_weights.diffuse = 0.0;
                        if (!is_specular) _lobe_weights.specular = _lobe_weights.clearcoat = 0.0;
                        vec3 light_contribution = EvaluateLightSource_LTC(litem, P, I, N, _lobe_weights, ltc, g_ltc_luts,
                                                                          sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                        if (all(equal(light_contribution, vec3(0.0)))) {
                            continue;
                        }
                        if (is_portal) {
                            // Sample environment to create slight color variation
                            const vec3 rotated_dir = rotate_xz(normalize(litem.pos_and_radius.xyz - P), g_shrd_data.env_col.w);
                            light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).rgb;
                        }
                        light_total += light_contribution * LightVisibility(litem, P);
                    }
                }
            }

            if (dot(g_shrd_data.sun_col.xyz, g_shrd_data.sun_col.xyz) > 0.0 && g_shrd_data.sun_dir.y > 0.0) {
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

                const float sun_visibility = SampleShadowPCF5x5(g_shadow_depth_tex, shadow_uvs);
                if (sun_visibility > 0.0) {
                    light_total += sun_visibility * EvaluateSunLight_LTC(g_shrd_data.sun_col.xyz, g_shrd_data.sun_dir.xyz, g_shrd_data.sun_dir.w, P, I, N, lobe_weights, ltc, g_ltc_luts,
                                                                         sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                }
            }

#ifdef GI_CACHE
            for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
                const float weight = get_volume_blend_weight(P, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
                if (weight > 0.0) {
                    if (lobe_weights.diffuse > 0.0) {
                        vec3 irradiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(refl_ray_ws, g_shrd_data.probe_volumes[i].spacing.xyz), N,
                                                                    g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, !is_last_bounce);
                        irradiance *= base_color * ltc.diff_t2.x;
                        irradiance *= mix(1.0, saturate(inter.t / (0.5 * length(g_shrd_data.probe_volumes[i].spacing.xyz))), saturate(16.0 * first_roughness));
                        light_total += lobe_weights.diffuse_mul * (1.0 / M_PI) * irradiance;
                    }
                    if ((is_last_bounce || roughness > RECURSION_ROUGHNESS_THRES) && lobe_weights.specular > 0.0) {
                        const vec3 refl_dir = reflect(refl_ray_ws, N);
                        vec3 avg_radiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(refl_ray_ws, g_shrd_data.probe_volumes[i].spacing.xyz), refl_dir,
                                                                      g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, false);
                        avg_radiance *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
                        avg_radiance *= mix(1.0, saturate(inter.t / (0.5 * length(g_shrd_data.probe_volumes[i].spacing.xyz))), saturate(16.0 * first_roughness));
                        light_total += (1.0 / M_PI) * avg_radiance;
                    }
                    break;
                }
            }
#endif

            final_color += throughput * light_total;
            if (j == 0) {
                first_ray_len = inter.t;
            }
            if (lobe_weights.specular > 0.0) {
                throughput *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
            } else {
                break;
            }
            if (dot(throughput, throughput) < 0.001 || roughness > RECURSION_ROUGHNESS_THRES) {
                break;
            }

            // prepare next ray
            ray_origin_ws.xyz = P;
            ray_origin_ws.xyz += 0.001 * tri_normal;
            refl_ray_ws = SampleReflectionVector(refl_ray_ws, N, roughness, u.zw);
        }
    }

    final_color = compress_hdr(final_color, g_shrd_data.cam_pos_and_exp.w);
    first_ray_len = GetNormHitDist(first_ray_len, view_z, first_roughness);
#if 0
    int copy_count = 0;
    if (copy_horizontal) ++copy_count;
    if (copy_vertical) ++copy_count;
    if (copy_diagonal) ++copy_count;
    if (copy_count == 3) {
        final_color *= vec3(0.0, 1.0, 0.0);
    } else if (copy_count == 2 || copy_count == 1) {
        final_color *= vec3(0.0, 0.0, 1.0);
    } else {
        final_color *= vec3(1.0, 0.0, 0.0);
    }
#endif

#ifndef LAYERED
    const vec3 prev_color = imageLoad(g_out_color_img, icoord).xyz;
    imageStore(g_out_color_img, icoord, vec4(prev_color + final_color, first_ray_len));

    const ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        const ivec2 copy_coords = ivec2(copy_target.x, icoord.y);
        const vec3 prev_color = imageLoad(g_out_color_img, copy_coords).xyz;
        imageStore(g_out_color_img, copy_coords, vec4(prev_color + final_color, first_ray_len));
    }
    if (copy_vertical) {
        const ivec2 copy_coords = ivec2(icoord.x, copy_target.y);
        const vec3 prev_color = imageLoad(g_out_color_img, copy_coords).xyz;
        imageStore(g_out_color_img, copy_coords, vec4(prev_color + final_color, first_ray_len));
    }
    if (copy_diagonal) {
        const ivec2 copy_coords = copy_target;
        const vec3 prev_color = imageLoad(g_out_color_img, copy_coords).xyz;
        imageStore(g_out_color_img, copy_coords, vec4(prev_color + final_color, first_ray_len));
    }
#else
    [[dont_flatten]] if (layer_index == 0) {
        imageStore(g_out_color_img[0], icoord / 2, vec4(final_color, 1.0));
    } else {
        imageStore(g_out_color_img[1], icoord / 2, vec4(final_color, 1.0));
    }
#endif
}
