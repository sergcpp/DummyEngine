#version 320 es
#extension GL_EXT_control_flow_attributes : require
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "_fs_common.glsl"
#include "_swrt_common.glsl"
#include "_texturing.glsl"
#include "_principled.glsl"

#include "ssr_common.glsl"
#include "rt_reflections_interface.h"

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
layout(binding = MESHES_BUF_SLOT) uniform usamplerBuffer g_meshes;
layout(binding = MESH_INSTANCES_BUF_SLOT) uniform samplerBuffer g_mesh_instances;

layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer LightsData {
    light_item_t g_lights[];
};

layout(binding = CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

layout(binding = LMAP_TEX_SLOTS) uniform sampler2D g_lm_textures[5];

layout(std430, binding = RAY_COUNTER_SLOT) readonly buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

layout(binding = NOISE_TEX_SLOT) uniform lowp sampler2D g_noise_tex;

layout(binding = OUT_REFL_IMG_SLOT, r11f_g11f_b10f) uniform writeonly restrict image2D g_out_color_img;
layout(binding = OUT_RAYLEN_IMG_SLOT, r16f) uniform writeonly restrict image2D g_out_raylen_img;

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_index = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex;
    if (ray_index >= g_ray_counter[5]) return;
    const uint packed_coords = g_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    const ivec2 icoord = ivec2(ray_coords);
    const float depth = texelFetch(g_depth_tex, icoord, 0).r;
    const vec4 normal_roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x);
    const vec3 normal_ws = normal_roughness.xyz;
    const vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    const float roughness = normal_roughness.w * normal_roughness.w;

    const vec2 px_center = vec2(icoord) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

    vec4 ray_origin_vs = g_shrd_data.view_from_clip * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    const vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    const vec2 u = texelFetch(g_noise_tex, icoord % 128, 0).rg;
    const vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, roughness, u);
    const vec3 refl_ray_ws = (g_shrd_data.world_from_view * vec4(refl_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.world_from_view * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    float ray_len = 0.0;
    vec3 throughput = vec3(1.0);
    vec3 final_color;

    vec3 ro = ray_origin_ws.xyz + 0.001 * refl_ray_ws.xyz;
    vec3 inv_d = safe_invert(refl_ray_ws.xyz);

    hit_data_t inter;
    inter.mask = 0;
    inter.obj_index = inter.prim_index = 0;
    inter.geo_index = inter.geo_count = 0;
    inter.t = 1000.0;
    inter.u = inter.v = 0.0;

    int transp_depth = 0;
    while (transp_depth++ < 4) {
        Traverse_MacroTree_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_meshes, g_vtx_data0, g_vtx_indices, g_prim_indices,
                                     ro, refl_ray_ws.xyz, inv_d, (1u << RAY_TYPE_SPECULAR), 0 /* root_node */, inter);
        if (inter.mask != 0) {
            // perform alpha test
            const bool backfacing = (inter.prim_index < 0);
            const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

            int i = inter.geo_index;
            for (; i < inter.geo_index + inter.geo_count; ++i) {
                const int tri_start = int(g_geometries[i].indices_start) / 3;
                if (tri_start > tri_index) {
                    break;
                }
            }

            const int geo_index = i - 1;

            const RTGeoInstance geo = g_geometries[geo_index];
            const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
            const MaterialData mat = g_materials[mat_index];

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
            const float alpha = textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[4])), uv, 0.0).r;
            if (alpha < 0.5) {
                ro += (inter.t + 0.001) * refl_ray_ws.xyz;
                inter.mask = 0;
                inter.t = 1000.0;
                continue;
            }
#endif
        }
        break;
    }

    const bool ignore_portals_specular = true;

    if (inter.mask == 0) {
        // Check portal lights intersection (rough rays are blocked by them)
        for (int i = 0; i < MAX_PORTALS_TOTAL && g_shrd_data.portals[i] != 0xffffffff &&
             !ignore_portals_specular; ++i) {
            const light_item_t litem = g_lights[g_shrd_data.portals[i]];

            const vec3 light_pos = litem.pos_and_radius.xyz;
            vec3 light_u = litem.u_and_reg.xyz, light_v = litem.v_and_blend.xyz;
            const vec3 light_forward = normalize(cross(light_u, light_v));

            const float plane_dist = dot(light_forward, light_pos);
            const float cos_theta = dot(refl_ray_ws.xyz, light_forward);
            const float t = (plane_dist - dot(light_forward, ray_origin_ws.xyz)) / min(cos_theta, -FLT_EPS);

            if (cos_theta < 0.0 && t > 0.0) {
                light_u /= dot(light_u, light_u);
                light_v /= dot(light_v, light_v);

                const vec3 p = ray_origin_ws.xyz + refl_ray_ws.xyz * t;
                const vec3 vi = p - light_pos;
                const float a1 = dot(light_u, vi);
                if (a1 >= -0.5 && a1 <= 0.5) {
                    const float a2 = dot(light_v, vi);
                    if (a2 >= -0.5 && a2 <= 0.5) {
                        throughput *= 0.0;
                        break;
                    }
                }
            }
        }

        const vec3 rotated_dir = rotate_xz(refl_ray_ws.xyz, g_shrd_data.env_col.w);
        const float lod = 8.0 * roughness;
        final_color = throughput * g_shrd_data.env_col.xyz * textureLod(g_env_tex, rotated_dir, lod).rgb;
    } else {
        const bool backfacing = (inter.prim_index < 0);
        const int tri_index = backfacing ? -inter.prim_index - 1 : inter.prim_index;

        int i = inter.geo_index;
        for (; i < inter.geo_index + inter.geo_count; ++i) {
            const int tri_start = int(g_geometries[i].indices_start) / 3;
            if (tri_start > tri_index) {
                break;
            }
        }

        const int geo_index = i - 1;

        const RTGeoInstance geo = g_geometries[geo_index];
        const uint mat_index = backfacing ? (geo.material_index >> 16) : (geo.material_index & 0xffff);
        const MaterialData mat = g_materials[mat_index];

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
        const mat4x3 inv_transform = transpose(mat3x4(texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 3)),
                                                      texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 4)),
                                                      texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 5))));
        const vec3 direction_obj_space = (inv_transform * vec4(refl_ray_ws.xyz, 0.0)).xyz;

        const float _cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

        const vec2 tex_res = textureSize(SAMPLER2D(GET_HANDLE(mat.texture_indices[0])), 0).xy;
        const float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

        vec3 tri_normal = cross(p1.xyz - p0.xyz, p2.xyz - p0.xyz);
        const float pa = length(tri_normal);
        tri_normal /= pa;

        float cone_width = g_params.pixel_spread_angle * inter.t;

        float tex_lod = 0.5 * log2(ta / pa);
        tex_lod += log2(cone_width);
        tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
        tex_lod -= log2(abs(dot(direction_obj_space, tri_normal)));

        vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[0])), uv, tex_lod)));
#else
        // TODO: Fallback to shared texture atlas
        vec3 base_color = vec3(1.0);
        float tex_lod = 0.0;
#endif

        if ((geo.flags & RTGeoLightmappedBit) != 0u) {
            const vec2 lm_uv0 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i0)).w);
            const vec2 lm_uv1 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i1)).w);
            const vec2 lm_uv2 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i2)).w);

            vec2 lm_uv = lm_uv0 * (1.0 - inter.u - inter.v) + lm_uv1 * inter.u + lm_uv2 * inter.v;
            lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

            const vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
            const vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));

            final_color = base_color * (direct_lm + indirect_lm);
        } else {
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
            const mat4x3 transform = transpose(mat3x4(texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 6)),
                                                      texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 7)),
                                                      texelFetch(g_mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * inter.obj_index + 8))));
            N = normalize((transform * vec4(N, 0.0)).xyz);

            const vec3 P = ray_origin_ws.xyz + refl_ray_ws.xyz * inter.t;
            const vec3 I = -refl_ray_ws.xyz;
            const float N_dot_V = saturate(dot(N, I));

            vec3 tint_color = vec3(0.0);

            const float base_color_lum = lum(base_color);
            if (base_color_lum > 0.0) {
                tint_color = base_color / base_color_lum;
            }

#if defined(BINDLESS_TEXTURES)
            const float roughness = mat.params[0].w * textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[2])), uv, tex_lod).r;
#else
            const float roughness = mat.params[0].w;
#endif
            const float sheen = mat.params[1].x;
            const float sheen_tint = mat.params[1].y;
            const float specular = mat.params[1].z;
            const float specular_tint = mat.params[1].w;
#if defined(BINDLESS_TEXTURES)
            const float metallic = mat.params[2].x * textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[3])), uv, tex_lod).r;
#else
            const float metallic = mat.params[2].x;
#endif
            const float transmission = mat.params[2].y;
            const float clearcoat = mat.params[2].z;
            const float clearcoat_roughness = mat.params[2].w;

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

            vec3 light_total = vec3(0.0);

            vec4 projected_p = g_shrd_data.rt_clip_from_world * vec4(P, 1.0);
            projected_p /= projected_p[3];
            #if defined(VULKAN)
                projected_p.xy = projected_p.xy * 0.5 + 0.5;
            #else // VULKAN
                projected_p.xyz = projected_p.xyz * 0.5 + 0.5;
            #endif // VULKAN

            const highp float lin_depth = LinearizeDepth(projected_p.z, g_shrd_data.rt_clip_info);
            const highp float k = log2(lin_depth / g_shrd_data.rt_clip_info[1]) / g_shrd_data.rt_clip_info[3];
            const int tile_x = clamp(int(projected_p.x * ITEM_GRID_RES_X), 0, ITEM_GRID_RES_X - 1),
                      tile_y = clamp(int(projected_p.y * ITEM_GRID_RES_Y), 0, ITEM_GRID_RES_Y - 1),
                      tile_z = clamp(int(k * ITEM_GRID_RES_Z), 0, ITEM_GRID_RES_Z - 1);

            const int cell_index = tile_z * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + tile_y * ITEM_GRID_RES_X + tile_x;

            const highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
            const highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
            const highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

            for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
                const highp uint item_data = texelFetch(g_items_buf, int(i)).x;
                const int li = int(bitfieldExtract(item_data, 0, 12));

                const light_item_t litem = g_lights[li];

                const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
                const bool is_last_bounce = true;
                if (!is_last_bounce && is_portal && lobe_weights.diffuse < FLT_EPS) {
                    continue;
                }

                lobe_weights_t _lobe_weights = lobe_weights;
                if (!is_last_bounce && is_portal) {
                    // Portal lights affect only diffuse
                    _lobe_weights.specular = _lobe_weights.clearcoat = 0.0;
                }
                vec3 light_contribution = EvaluateLightSource(litem, P, I, N, _lobe_weights, ltc, g_ltc_luts,
                                                              sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                if (all(equal(light_contribution, vec3(0.0)))) {
                    continue;
                }
                if (is_portal) {
                    // Sample environment to create slight color variation
                    const vec3 rotated_dir = rotate_xz(normalize(litem.pos_and_radius.xyz - P), g_shrd_data.env_col.w);
                    light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).rgb;
                }

                int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
                [[dont_flatten]] if (shadowreg_index != -1) {
                    const vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
                    shadowreg_index += cubemap_face(from_light, litem.dir_and_spot.xyz, normalize(litem.u_and_reg.xyz), normalize(litem.v_and_blend.xyz));
                    vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                    vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(P, 1.0);
                    pp /= pp.w;

                    #if defined(VULKAN)
                        pp.xy = pp.xy * 0.5 + vec2(0.5);
                    #else // VULKAN
                        pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                    #endif // VULKAN
                    pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
                    #if defined(VULKAN)
                        pp.y = 1.0 - pp.y;
                    #endif // VULKAN

                    light_contribution *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
                }

                light_total += light_contribution;
            }

            if (dot(g_shrd_data.sun_col.xyz, g_shrd_data.sun_col.xyz) > 0.0) {
                vec4 pos_ws = vec4(P, 1.0);

                const vec2 shadow_offsets = get_shadow_offsets(saturate(dot(N, g_shrd_data.sun_dir.xyz)));
                pos_ws.xyz += 0.01 * shadow_offsets.x * N;
                pos_ws.xyz += 0.002 * shadow_offsets.y * g_shrd_data.sun_dir.xyz;

                vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[3].clip_from_world * pos_ws).xyz;
        #if defined(VULKAN)
                shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
        #else // VULKAN
                shadow_uvs = 0.5 * shadow_uvs + 0.5;
        #endif // VULKAN
                shadow_uvs.xy *= vec2(0.25, 0.5);
                shadow_uvs.xy += vec2(0.25, 0.5);
        #if defined(VULKAN)
                shadow_uvs.y = 1.0 - shadow_uvs.y;
        #endif // VULKAN

                const float sun_visibility = SampleShadowPCF5x5(g_shadow_tex, shadow_uvs);
                if (sun_visibility > 0.0) {
                    light_total += sun_visibility * EvaluateSunLight(g_shrd_data.sun_col.xyz, g_shrd_data.sun_dir.xyz, g_shrd_data.sun_dir.w, P, I, N, lobe_weights, ltc, g_ltc_luts,
                                                                     sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                }
            }

            final_color = throughput * light_total;
            final_color += throughput * lobe_weights.diffuse_mul * base_color * g_shrd_data.ambient_hack.rgb;
        }

        ray_len = inter.t;
    }

    imageStore(g_out_color_img, icoord, vec4(final_color, 1.0));
    imageStore(g_out_raylen_img, icoord, vec4(ray_len));

    ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        ivec2 copy_coords = ivec2(copy_target.x, icoord.y);
        imageStore(g_out_color_img, copy_coords, vec4(final_color, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(ray_len));
    }
    if (copy_vertical) {
        ivec2 copy_coords = ivec2(icoord.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, vec4(final_color, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(ray_len));
    }
    if (copy_diagonal) {
        ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, vec4(final_color, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(ray_len));
    }
}
