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

#include "rt_debug_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

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

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

layout(binding = LMAP_TEX_SLOTS) uniform sampler2D g_lm_textures[5];

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_image;

int hash(const int x) {
    uint ret = uint(x);
    ret = ((ret >> 16) ^ ret) * 0x45d9f3b;
    ret = ((ret >> 16) ^ ret) * 0x45d9f3b;
    ret = (ret >> 16) ^ ret;
    return int(ret);
}

float construct_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    const float  f = uintBitsToFloat(m);   // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const vec2 px_center = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);
    vec2 d = in_uv * 2.0 - 1.0;
#if defined(VULKAN)
    d.y = -d.y;
#endif

    vec4 origin = g_shrd_data.inv_view_matrix * vec4(0, 0, 0, 1);
    origin /= origin.w;
    vec4 target = g_shrd_data.inv_proj_matrix * vec4(d.xy, 1, 1);
    target /= target.w;
    vec4 direction = g_shrd_data.inv_view_matrix * vec4(normalize(target.xyz), 0);

    vec3 inv_d = safe_invert(direction.xyz);

    hit_data_t inter;
    inter.mask = 0;
    inter.obj_index = inter.prim_index = 0;
    inter.geo_index = inter.geo_count = 0;
    inter.t = MAX_DIST;
    inter.u = inter.v = 0.0;

    Traverse_MacroTree_WithStack(g_tlas_nodes, g_blas_nodes, g_mesh_instances, g_meshes, g_vtx_data0,
                                 g_vtx_indices, g_prim_indices, origin.xyz, direction.xyz, inv_d, 0 /* root_node */, inter);

    vec3 final_color;
    if (inter.mask == 0) {
        final_color = RGBMDecode(texture(g_env_tex, direction.xyz));
    } else {
        bool is_backfacing = (inter.prim_index < 0);
        int tri_index = is_backfacing ? -inter.prim_index - 1 : inter.prim_index;

        int i = inter.geo_index;
        for (; i < inter.geo_index + inter.geo_count; ++i) {
            int tri_start = int(g_geometries[i].indices_start) / 3;
            if (tri_start > tri_index) {
                break;
            }
        }

        int geo_index = i - 1;

        RTGeoInstance geo = g_geometries[geo_index];
        MaterialData mat = g_materials[geo.material_index];

        uint i0 = texelFetch(g_vtx_indices, 3 * tri_index + 0).x;
        uint i1 = texelFetch(g_vtx_indices, 3 * tri_index + 1).x;
        uint i2 = texelFetch(g_vtx_indices, 3 * tri_index + 2).x;

        vec4 p0 = texelFetch(g_vtx_data0, int(geo.vertices_start + i0));
        vec4 p1 = texelFetch(g_vtx_data0, int(geo.vertices_start + i1));
        vec4 p2 = texelFetch(g_vtx_data0, int(geo.vertices_start + i2));

        vec2 uv0 = unpackHalf2x16(floatBitsToUint(p0.w));
        vec2 uv1 = unpackHalf2x16(floatBitsToUint(p1.w));
        vec2 uv2 = unpackHalf2x16(floatBitsToUint(p2.w));

        vec2 uv = uv0 * (1.0 - inter.u - inter.v) + uv1 * inter.u + uv2 * inter.v;
#if defined(BINDLESS_TEXTURES)
        mat4x3 inv_transform = transpose(mat3x4(texelFetch(g_mesh_instances, int(5 * inter.obj_index + 2)),
                                                texelFetch(g_mesh_instances, int(5 * inter.obj_index + 3)),
                                                texelFetch(g_mesh_instances, int(5 * inter.obj_index + 4))));
        vec3 direction_obj_space = (inv_transform * vec4(direction.xyz, 0.0)).xyz;

        vec2 tex_res = textureSize(SAMPLER2D(GET_HANDLE(mat.texture_indices[0])), 0).xy;
        float ta = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y));

        vec3 tri_normal = cross(p1.xyz - p0.xyz, p2.xyz - p0.xyz);
        float pa = length(tri_normal);
        tri_normal /= pa;

        float cone_width = g_params.pixel_spread_angle * inter.t;

        float tex_lod = 0.5 * log2(ta/pa);
        tex_lod += log2(cone_width);
        tex_lod += 0.5 * log2(tex_res.x * tex_res.y);
        tex_lod -= log2(abs(dot(direction_obj_space, tri_normal)));

        vec3 base_color = mat.params[0].xyz * SRGBToLinear(YCoCg_to_RGB(textureLod(SAMPLER2D(GET_HANDLE(mat.texture_indices[0])), uv, tex_lod)));
#else
        // TODO: Fallback to shared texture atlas
        float tex_lod = 0.0;
        vec3 base_color = vec3(1.0);
#endif

        if ((geo.flags & RTGeoLightmappedBit) != 0u) {
            vec2 lm_uv0 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i0)).w);
            vec2 lm_uv1 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i1)).w);
            vec2 lm_uv2 = unpackHalf2x16(texelFetch(g_vtx_data1, int(geo.vertices_start + i2)).w);

            vec2 lm_uv = lm_uv0 * (1.0 - inter.u - inter.v) + lm_uv1 * inter.u + lm_uv2 * inter.v;
            lm_uv = geo.lmap_transform.xy + geo.lmap_transform.zw * lm_uv;

            vec3 direct_lm = RGBMDecode(textureLod(g_lm_textures[0], lm_uv, 0.0));
            vec3 indirect_lm = 2.0 * RGBMDecode(textureLod(g_lm_textures[1], lm_uv, 0.0));
            final_color = base_color * (direct_lm + indirect_lm);
        } else {
            const uvec2 packed0 = texelFetch(g_vtx_data1, int(geo.vertices_start + i0)).xy;
            const uvec2 packed1 = texelFetch(g_vtx_data1, int(geo.vertices_start + i1)).xy;
            const uvec2 packed2 = texelFetch(g_vtx_data1, int(geo.vertices_start + i2)).xy;

            const vec3 normal0 = vec3(unpackSnorm2x16(packed0.x), unpackSnorm2x16(packed0.y).x);
            const vec3 normal1 = vec3(unpackSnorm2x16(packed1.x), unpackSnorm2x16(packed1.y).x);
            const vec3 normal2 = vec3(unpackSnorm2x16(packed2.x), unpackSnorm2x16(packed2.y).x);

            vec3 N = normal0 * (1.0 - inter.u - inter.v) + normal1 * inter.u + normal2 * inter.v;
            if (is_backfacing) {
                N = -N;
            }

            const vec3 P = origin.xyz + direction.xyz * inter.t;
            const vec3 I = -direction.xyz;
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

            const lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular,
                                                                 metallic, transmission, clearcoat);

            const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

            const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
            const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
            const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

            // Approximation of FH (using shading normal)
            const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
            const vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

            const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

            vec3 light_total = vec3(0.0);

            for (int li = 0; li < int(g_shrd_data.item_counts.x); ++li) {
                light_item_t litem = g_lights[li];

                vec3 light_contribution = EvaluateLightSource(litem, P, I, N, lobe_weights, ltc, g_ltc_luts,
                                                              sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                if (all(equal(light_contribution, vec3(0.0)))) {
                    continue;
                }

                int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
                [[dont_flatten]] if (shadowreg_index != -1) {
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

            final_color = light_total;
        }
    }

    imageStore(g_out_image, ivec2(gl_GlobalInvocationID.xy), vec4(final_color, 1.0));
}