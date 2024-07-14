#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_control_flow_attributes : require

#define ENABLE_SHEEN 0
#define ENABLE_CLEARCOAT 0
#define MIN_SPEC_ROUGHNESS 0.4

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "gi_cache_common.glsl"
#include "rt_debug_interface.h"

#pragma multi_compile _ GI_CACHE

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(std430, binding = GEO_DATA_BUF_SLOT) readonly buffer GeometryData {
    RTGeoInstance g_geometries[];
};

layout(std430, binding = MATERIAL_BUF_SLOT) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    uvec4 g_vtx_data0[];
};

layout(std430, binding = VTX_BUF2_SLOT) readonly buffer VtxData1 {
    uvec4 g_vtx_data1[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer NdxData {
    uint g_indices[];
};

layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer LightsData {
    light_item_t g_lights[];
};

layout(binding = CELLS_BUF_SLOT) uniform usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform usamplerBuffer g_items_buf;

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

#ifdef GI_CACHE
    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

hitAttributeEXT vec2 bary_coord;

void main() {
    const RTGeoInstance geo = g_geometries[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    uint mat_index = (geo.material_index & 0xffff);
    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
        mat_index = (geo.material_index >> 16) & 0xffff;
    }
    const MaterialData mat = g_materials[mat_index & MATERIAL_INDEX_BITS];

    const uint i0 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 0];
    const uint i1 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 1];
    const uint i2 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 2];

    const vec3 p0 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i0].xyz);
    const vec3 p1 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i1].xyz);
    const vec3 p2 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i2].xyz);

    const vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
    const vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
    const vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

    const vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;

    vec2 tex_res = textureSize(SAMPLER2D(mat.texture_indices[0]), 0).xy;
    float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

    vec3 tri_normal = cross(p1 - p0, p2 - p0);
    float pa = length(tri_normal);
    tri_normal /= pa;

    float cone_width = g_params.pixel_spread_angle * gl_HitTEXT;

    float tex_lod = 0.5 * log2(ta / pa);
    tex_lod += log2(cone_width);
    tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
    tex_lod -= log2(abs(dot(gl_ObjectRayDirectionEXT, tri_normal)));
    vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(mat.texture_indices[0]), uv, tex_lod)));

    const vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x),
                              unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
    const vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x),
                              unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
    const vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x),
                              unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

    vec3 N = normal0 * (1.0 - bary_coord.x - bary_coord.y) + normal1 * bary_coord.x + normal2 * bary_coord.y;
    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
        N = -N;
    }
    N = normalize((gl_ObjectToWorldEXT * vec4(N, 0.0)).xyz);

    const vec3 P = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    const vec3 I = -gl_WorldRayDirectionEXT;//normalize(g_shrd_data.cam_pos_and_exp.xyz - P);
    const float N_dot_V = saturate(dot(N, I));

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    const float roughness = mat.params[0].w * textureLod(SAMPLER2D(mat.texture_indices[2]), uv, tex_lod).r;
    const float sheen = mat.params[1].x;
    const float sheen_tint = mat.params[1].y;
    const float specular = mat.params[1].z;
    const float specular_tint = mat.params[1].w;
    const float metallic = mat.params[2].x * textureLod(SAMPLER2D(mat.texture_indices[3]), uv, tex_lod).r;
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

    const lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular,
                                                            metallic, transmission, clearcoat);

    const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

    const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
    const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
    const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

    // Approximation of FH (using shading normal)
    const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
    const vec3 approx_clearcoat_col = vec3(mix(0.04, 1.0, clearcoat_FN));

    const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

    vec3 light_total = vec3(0.0);

    vec4 projected_p = g_shrd_data.rt_clip_from_world * vec4(P, 1.0);
    projected_p /= projected_p[3];
    projected_p.xy = projected_p.xy * 0.5 + 0.5;

    const float lin_depth = LinearizeDepth(projected_p.z, g_shrd_data.rt_clip_info);
    const float k = log2(lin_depth / g_shrd_data.rt_clip_info[1]) / g_shrd_data.rt_clip_info[3];
    const int tile_x = clamp(int(projected_p.x * ITEM_GRID_RES_X), 0, ITEM_GRID_RES_X - 1),
              tile_y = clamp(int(projected_p.y * ITEM_GRID_RES_Y), 0, ITEM_GRID_RES_Y - 1),
              tile_z = clamp(int(k * ITEM_GRID_RES_Z), 0, ITEM_GRID_RES_Z - 1);

    const int cell_index = tile_z * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + tile_y * ITEM_GRID_RES_X + tile_x;

    const uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    const uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    const uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        const uint item_data = texelFetch(g_items_buf, int(i)).x;
        const int li = int(bitfieldExtract(item_data, 0, 12));

        const light_item_t litem = g_lights[li];

        const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
        //if (is_portal && lobe_weights.diffuse < FLT_EPS) {
        //    continue;
        //}

        lobe_weights_t _lobe_weights = lobe_weights;
        //if (is_portal) {
            // Portal lights affect only diffuse
        //    _lobe_weights.specular = _lobe_weights.clearcoat = 0.0;
        //}
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

            pp.xy = pp.xy * 0.5 + vec2(0.5);
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
        shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
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

#ifdef GI_CACHE
    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const float weight = get_volume_blend_weight(P, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
        if (weight > 0.0) {
            if (lobe_weights.diffuse > 0.0) {
                const vec3 irradiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(-I, g_shrd_data.probe_volumes[i].spacing.xyz), N,
                                                                  g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
                light_total += lobe_weights.diffuse_mul * (1.0 / M_PI) * base_color * irradiance;
            }
            if (lobe_weights.specular > 0.0) {
                const vec3 refl_dir = reflect(-I, N);
                vec3 avg_radiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(-I, g_shrd_data.probe_volumes[i].spacing.xyz), refl_dir,
                                                              g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
                avg_radiance *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
                light_total += avg_radiance * (1.0 / M_PI);
            }
            break;
        }
    }
#endif

    g_pld.col = light_total;
}

