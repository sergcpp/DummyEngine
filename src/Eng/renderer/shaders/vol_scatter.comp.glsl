#version 430 core
#extension GL_EXT_control_flow_attributes : require
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#include "_cs_common.glsl"
#include "_fs_common.glsl"
#include "gi_cache_common.glsl"
#include "pmj_common.glsl"
#include "vol_common.glsl"

#include "vol_interface.h"

#pragma multi_compile _ ALL_CASCADES
#pragma multi_compile _ GI_CACHE
#pragma multi_compile _ NO_SUBGROUP

#if defined(NO_SUBGROUP)
    #define subgroupMin(x) (x)
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2D g_shadow_color_tex;

layout(binding = STBN_TEX_SLOT) uniform sampler2DArray g_stbn_tex;

layout(binding = FR_SCATTER_TEX_SLOT) uniform sampler3D g_fr_scatter_hist_tex;
layout(binding = FR_EMISSION_TEX_SLOT) uniform sampler3D g_fr_emission_hist_tex;

layout(binding = LIGHT_BUF_SLOT) uniform samplerBuffer g_lights_buf;
layout(binding = DECAL_BUF_SLOT) uniform samplerBuffer g_decals_buf;
layout(binding = CELLS_BUF_SLOT) uniform usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform usamplerBuffer g_items_buf;
layout(binding = ENVMAP_TEX_SLOT) uniform samplerCube g_envmap_tex;

#ifdef GI_CACHE
    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(binding = OUT_FR_EMISSION_IMG_SLOT, rgba16f) uniform image3D g_out_fr_emission_img;
layout(binding = OUT_FR_SCATTER_IMG_SLOT, rgba16f) uniform image3D g_out_fr_scatter_img;

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

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec3 icoord = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(icoord, g_params.froxel_res.xyz))) {
        return;
    }

    const vec2 norm_uvs = (vec2(icoord.xy) + 0.5) / g_params.froxel_res.xy;

    const uint px_hash = hash((gl_GlobalInvocationID.x << 16) | gl_GlobalInvocationID.y);
    const float offset_rand = textureLod(g_stbn_tex, vec3((vec2(icoord.xy) + 0.5) / 64.0, float(g_params.frame_index % 64)), 0.0).x;

    const vec3 pos_uvw = froxel_to_uvw(icoord, offset_rand, g_params.froxel_res.xyz);
    const vec4 pos_cs = vec4(2.0 * pos_uvw.xy - 1.0, pos_uvw.z, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 view_ray_ws = normalize(pos_ws - g_shrd_data.cam_pos_and_exp.xyz);
    const float view_dist = length(pos_ws - g_shrd_data.cam_pos_and_exp.xyz);

    vec3 light_total = vec3(0.0);

    vec4 emission_density = imageLoad(g_out_fr_emission_img, icoord);
    vec4 scatter_absorption = imageLoad(g_out_fr_scatter_img, icoord);

    if (emission_density.w > 0.0) {
        // Artificial lights
        const float lin_depth = LinearizeDepth(pos_uvw.z, g_shrd_data.clip_info);
        const float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
        const int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);
        const int cell_index = GetCellIndex(icoord.x, icoord.y, slice, g_params.froxel_res.xy);
    #if !defined(NO_SUBGROUP)
        [[dont_flatten]] if (subgroupAllEqual(cell_index)) {
            const int s_first_cell_index = subgroupBroadcastFirst(cell_index);
            const uvec2 s_cell_data = texelFetch(g_cells_buf, s_first_cell_index).xy;
            const uvec2 s_offset_and_lcount = uvec2(bitfieldExtract(s_cell_data.x, 0, 24), bitfieldExtract(s_cell_data.x, 24, 8));
            for (uint i = s_offset_and_lcount.x; i < s_offset_and_lcount.x + s_offset_and_lcount.y; ++i) {
                const uint s_item_data = texelFetch(g_items_buf, int(i)).x;
                const int s_li = int(bitfieldExtract(s_item_data, 0, 12));

                const _light_item_t litem = FetchLightItem(g_lights_buf, s_li);
                const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
                const bool is_volume = (floatBitsToUint(litem.col_and_type.w) & LIGHT_VOLUME_BIT) != 0;
                if (!is_volume) {
                    continue;
                }

                vec3 light_contribution = EvaluateLightSource_Vol(litem, pos_ws, view_ray_ws, g_params.anisotropy);
                if (all(equal(light_contribution, vec3(0.0)))) {
                    continue;
                }
                if (is_portal) {
                    // Sample environment to create slight color variation
                    const vec3 rotated_dir = rotate_xz(normalize(litem.shadow_pos_and_tri_index.xyz - pos_ws), g_shrd_data.env_col.w);
                    light_contribution *= textureLod(g_envmap_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).xyz;
                }
                light_total += light_contribution * LightVisibility(litem, pos_ws);
            }
        } else
    #endif // !defined(NO_SUBGROUP)
        {
            const uvec2 v_cell_data = texelFetch(g_cells_buf, cell_index).xy;
            const uvec2 v_offset_and_lcount = uvec2(bitfieldExtract(v_cell_data.x, 0, 24), bitfieldExtract(v_cell_data.x, 24, 8));
            for (uint i = v_offset_and_lcount.x; i < v_offset_and_lcount.x + v_offset_and_lcount.y; ) {
                const uint v_item_data = texelFetch(g_items_buf, int(i)).x;
                const int v_li = int(bitfieldExtract(v_item_data, 0, 12));
                const int s_li = subgroupMin(v_li);
                [[flatten]] if (s_li == v_li) {
                    ++i;
                    const _light_item_t litem = FetchLightItem(g_lights_buf, s_li);
                    const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
                    const bool is_volume = (floatBitsToUint(litem.col_and_type.w) & LIGHT_VOLUME_BIT) != 0;
                    if (!is_volume) {
                        continue;
                    }

                    vec3 light_contribution = EvaluateLightSource_Vol(litem, pos_ws, view_ray_ws, g_params.anisotropy);
                    if (all(equal(light_contribution, vec3(0.0)))) {
                        continue;
                    }
                    if (is_portal) {
                        // Sample environment to create slight color variation
                        const vec3 rotated_dir = rotate_xz(normalize(litem.shadow_pos_and_tri_index.xyz - pos_ws), g_shrd_data.env_col.w);
                        light_contribution *= textureLod(g_envmap_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).xyz;
                    }
                    light_total += light_contribution * LightVisibility(litem, pos_ws);
                }
            }
        }

        // Sun light contribution
        if (dot(g_shrd_data.sun_col_point_sh.xyz, g_shrd_data.sun_col_point_sh.xyz) > 0.0 && g_shrd_data.sun_dir.y > 0.0) {
            vec3 sun_visibility = vec3(1.0);
    #ifdef ALL_CASCADES
            mat4x3 sh_uvs;
            [[unroll]] for (int i = 0; i < 4; i++) {
                vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(pos_ws, 1.0)).xyz;
                shadow_uvs.xy = clamp(0.5 * shadow_uvs.xy + 0.5, vec2(0.0), vec2(1.0));
                shadow_uvs.xy *= vec2(0.25, 0.5);
                shadow_uvs.xy += SunCascadeOffsets[i];
        #if defined(VULKAN)
                shadow_uvs.y = 1.0 - shadow_uvs.y;
        #endif // VULKAN

                //vec3 temp = SampleShadowPCF5x5(g_shadow_depth_tex, g_shadow_color_tex, shadow_uvs);
                sh_uvs[i] = shadow_uvs;
            }
            sun_visibility = GetSunVisibilityPCF5x5(lin_depth, g_shadow_depth_tex, g_shadow_color_tex, sh_uvs);
    #else // ALL_CASCADES
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[3].clip_from_world * vec4(pos_ws, 1.0)).xyz;
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
            // NOTE: We have to check the bounds manually as we access outside of scene bounds
            if (all(lessThan(shadow_uvs.xy, vec2(0.999))) && all(greaterThan(shadow_uvs.xy, vec2(0.001)))) {
                shadow_uvs.xy *= vec2(0.25, 0.5);
                shadow_uvs.xy += SunCascadeOffsets[3];
    #if defined(VULKAN)
                shadow_uvs.y = 1.0 - shadow_uvs.y;
    #endif // VULKAN

                sun_visibility = SampleShadowPCF5x5(g_shadow_depth_tex, g_shadow_color_tex, shadow_uvs);
            }
    #endif // ALL_CASCADES
            light_total += sun_visibility * g_shrd_data.sun_col_point_sh.xyz * HenyeyGreenstein(dot(view_ray_ws, g_shrd_data.sun_dir.xyz), g_params.anisotropy);
        }

    #ifdef GI_CACHE
        for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
            const float weight = get_volume_blend_weight(pos_ws, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
            if (weight > 0.0 || i == PROBE_VOLUMES_COUNT - 1) {
                const vec3 avg_radiance = get_volume_irradiance(i, g_irradiance_tex, g_distance_tex, g_offset_tex, pos_ws, vec3(0.0), view_ray_ws,
                                                                g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, false, false);
                light_total += (1.0 / M_PI) * avg_radiance;

                break;
            }
        }
    #endif
    }

    light_total *= scatter_absorption.xyz;
    light_total *= emission_density.w;
    light_total = compress_hdr(light_total, g_shrd_data.cam_pos_and_exp.w);

    scatter_absorption.xyz = light_total;

    { // history accumulation
        const vec3 pos_uvw_no_offset = froxel_to_uvw(icoord, 0.5, g_params.froxel_res.xyz);
        const vec4 pos_cs_no_offset = vec4(2.0 * pos_uvw_no_offset.xy - 1.0, pos_uvw_no_offset.z, 1.0);
        const vec3 pos_ws_no_offset = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs_no_offset);

        const vec3 hist_pos_cs = TransformToClipSpace(g_shrd_data.prev_clip_from_world, pos_ws_no_offset);
        const float hist_lin_depth = LinearizeDepth(hist_pos_cs.z, g_shrd_data.clip_info);
        const vec3 hist_uvw = cs_to_uvw(hist_pos_cs.xy, hist_lin_depth);

        if (all(greaterThanEqual(hist_uvw, vec3(0.0))) && all(lessThanEqual(hist_uvw, vec3(1.0)))) {
            vec4 emission_density_hist = textureLod(g_fr_emission_hist_tex, hist_uvw, 0.0);
            emission_density_hist.xyz *= g_params.hist_weight;
            vec4 scatter_absorption_hist = textureLod(g_fr_scatter_hist_tex, hist_uvw, 0.0);
            scatter_absorption_hist.xyz *= g_params.hist_weight;

            const float HistoryWeightMin = 0.87;
            const float HistoryWeightMax = 0.95;

            const float lum_curr = lum(scatter_absorption.xyz + emission_density.xyz);
            const float lum_hist = lum(scatter_absorption_hist.xyz + emission_density_hist.xyz);

            const float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.001);
            const float unbiased_weight = 1.0 - unbiased_diff;
            const float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
            const float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

            scatter_absorption.xyz = mix(scatter_absorption.xyz, scatter_absorption_hist.xyz, history_weight);
            scatter_absorption.w = mix(scatter_absorption.w * emission_density.w, scatter_absorption_hist.w * emission_density_hist.w, history_weight);
            emission_density = mix(emission_density, emission_density_hist, history_weight);
            scatter_absorption.w = saturate(scatter_absorption.w / emission_density.w);
        }
    }

    imageStore(g_out_fr_emission_img, icoord, emission_density);
    imageStore(g_out_fr_scatter_img, icoord, scatter_absorption);
}
