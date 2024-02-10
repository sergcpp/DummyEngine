#version 460
#extension GL_EXT_ray_tracing : require

#include "_rt_common.glsl"
#include "_fs_common.glsl"
#include "_texturing.glsl"
#include "_principled.glsl"
#include "rt_debug_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = REN_UB_SHARED_DATA_LOC, std140) uniform SharedDataBlock {
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

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts[8];

layout(binding = LMAP_TEX_SLOTS) uniform sampler2D g_lm_textures[5];

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

hitAttributeEXT vec2 bary_coord;

void main() {
    RTGeoInstance geo = g_geometries[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = g_materials[geo.material_index];

    uint i0 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 0];
    uint i1 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 1];
    uint i2 = g_indices[geo.indices_start + 3 * gl_PrimitiveID + 2];

    vec3 p0 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i0].xyz);
    vec3 p1 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i1].xyz);
    vec3 p2 = uintBitsToFloat(g_vtx_data0[geo.vertices_start + i2].xyz);

    vec2 uv0 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i0].w);
    vec2 uv1 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i1].w);
    vec2 uv2 = unpackHalf2x16(g_vtx_data0[geo.vertices_start + i2].w);

    vec2 uv = uv0 * (1.0 - bary_coord.x - bary_coord.y) + uv1 * bary_coord.x + uv2 * bary_coord.y;

    vec2 tex_res = textureSize(SAMPLER2D(mat.texture_indices[0]), 0).xy;
    float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

    vec3 tri_normal = cross(p1 - p0, p2 - p0);
    float pa = length(tri_normal);
    tri_normal /= pa;

    float cone_width = g_params.pixel_spread_angle * gl_HitTEXT;

    float tex_lod = 0.5 * log2(ta/pa);
    tex_lod += log2(cone_width);
    tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
    tex_lod -= log2(abs(dot(gl_ObjectRayDirectionEXT, tri_normal)));
    vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(mat.texture_indices[0]), uv, tex_lod)));

    if ((geo.flags & RTGeoLightmappedBit) != 0u) {
        vec2 lm_uv0 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i0].w);
        vec2 lm_uv1 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i1].w);
        vec2 lm_uv2 = unpackHalf2x16(g_vtx_data1[geo.vertices_start + i2].w);

        vec2 lm_uv = lm_uv0 * (1.0 - bary_coord.x - bary_coord.y) + lm_uv1 * bary_coord.x + lm_uv2 * bary_coord.y;
        lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

        vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
        vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));
        g_pld.col = base_color * (direct_lm + indirect_lm);
    } else {
        vec3 normal0 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i0].y).x);
        vec3 normal1 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i1].y).x);
        vec3 normal2 = vec3(unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].x), unpackSnorm2x16(g_vtx_data1[geo.vertices_start + i2].y).x);

        vec3 N = normal0 * (1.0 - bary_coord.x - bary_coord.y) + normal1 * bary_coord.x + normal2 * bary_coord.y;
        if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
            N = -N;
        }

        vec3 P = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3 I = -gl_WorldRayDirectionEXT;//normalize(g_shrd_data.cam_pos_and_gamma.xyz - P);
        float N_dot_V = saturate(dot(N, I));

        vec3 tint_color = vec3(0.0);

        const float base_color_lum = lum(base_color);
        if (base_color_lum > 0.0) {
            tint_color = base_color / base_color_lum;
        }

        float roughness = mat.params[0].w;
        float sheen = mat.params[1].x;
        float sheen_tint = mat.params[1].y;
        float specular = mat.params[1].z;
        float specular_tint = mat.params[1].w;
        float metallic = mat.params[2].x;
        float transmission = mat.params[2].y;
        float clearcoat = mat.params[2].z;
        float clearcoat_roughness = mat.params[2].w;

        vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
        spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

        float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
        float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

        // Approximation of FH (using shading normal)
        float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

        vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
        float spec_color_lum = lum(approx_spec_col);

        lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

        vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

        float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
        float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
        float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

        // Approximation of FH (using shading normal)
        float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);

        vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

        //
        // Fetch LTC data
        //

        vec2 ltc_uv = LTC_Coords(N_dot_V, roughness);
        vec2 coat_ltc_uv = LTC_Coords(N_dot_V, clearcoat_roughness2);

        ltc_params_t ltc;
        ltc.diff_t1 = textureLod(g_ltc_luts[0], ltc_uv, 0.0);
        ltc.diff_t2 = textureLod(g_ltc_luts[1], ltc_uv, 0.0).xy;
        ltc.sheen_t1 = textureLod(g_ltc_luts[2], ltc_uv, 0.0);
        ltc.sheen_t2 = textureLod(g_ltc_luts[3], ltc_uv, 0.0).xy;
        ltc.spec_t1 = textureLod(g_ltc_luts[4], ltc_uv, 0.0);
        ltc.spec_t2 = textureLod(g_ltc_luts[5], ltc_uv, 0.0).xy;
        ltc.coat_t1 = textureLod(g_ltc_luts[6], coat_ltc_uv, 0.0);
        ltc.coat_t2 = textureLod(g_ltc_luts[7], coat_ltc_uv, 0.0).xy;

        vec3 light_total = vec3(0.0);

        for (int li = 0; li < int(g_shrd_data.item_counts.x); ++li) {
            light_item_t litem = g_lights[li];

            vec3 light_contribution = EvaluateLightSource(litem, P, I, N, lobe_weights, ltc,
                                                          g_ltc_luts[1], g_ltc_luts[3], g_ltc_luts[5], g_ltc_luts[7],
                                                          sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
            if (all(equal(light_contribution, vec3(0.0)))) {
                continue;
            }

            int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
            if (shadowreg_index != -1) {
                vec3 to_light = normalize(P - litem.pos_and_radius.xyz);
                shadowreg_index += cubemap_face(to_light, litem.dir_and_spot.xyz, normalize(litem.u_and_reg.xyz), normalize(litem.v_and_unused.xyz));
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

        g_pld.col = light_total;
    }
}

