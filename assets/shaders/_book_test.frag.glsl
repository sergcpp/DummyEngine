#version 430 core
//#extension GL_EXT_control_flow_attributes : enable

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = BIND_MAT_TEX0) uniform sampler2D g_diff_tex;
layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
layout(binding = BIND_MAT_TEX2) uniform sampler2D g_spec_tex;
layout(binding = BIND_SHAD_TEX) uniform sampler2DShadow g_shadow_tex;
layout(binding = BIND_DECAL_TEX) uniform sampler2D g_decals_tex;
layout(binding = BIND_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = BIND_LIGHT_BUF) uniform samplerBuffer g_lights_buf;
layout(binding = BIND_DECAL_BUF) uniform samplerBuffer g_decals_buf;
layout(binding = BIND_CELLS_BUF) uniform usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform usamplerBuffer g_items_buf;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(VULKAN)
layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
layout(location = 3) in vec3 g_vtx_tangent;
layout(location = 4) in vec3 g_vtx_sh_uvs[4];
#else
in vec3 g_vtx_pos;
in vec2 g_vtx_uvs;
in vec3 g_vtx_normal;
in vec3 g_vtx_tangent;
in vec3 g_vtx_sh_uvs[4];
#endif

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
layout(location = LOC_OUT_NORM) out vec4 g_out_normal;
layout(location = LOC_OUT_SPEC) out vec4 g_out_specular;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(void) {
    float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                    bitfieldExtract(cell_data.x, 24, 8));
    uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                    bitfieldExtract(cell_data.y, 8, 8));

    vec3 albedo_color = texture(g_diff_tex, g_vtx_uvs).rgb;

    vec2 duv_dx = dFdx(g_vtx_uvs), duv_dy = dFdy(g_vtx_uvs);
    vec3 normal_color = texture(g_norm_tex, g_vtx_uvs).wyz;
    vec4 specular_color = texture(g_spec_tex, g_vtx_uvs);

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(g_vtx_tangent, cross(g_vtx_normal, g_vtx_tangent),
                            g_vtx_normal) * normal);

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        vec4 col_and_index = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);

        vec3 L = pos_and_radius.xyz - g_vtx_pos;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;

        float denom = d / pos_and_radius.w + 1.0;
        float atten = 1.0 / (denom * denom);

        float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));

        float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);

        float _dot1 = clamp(dot(L, normal), 0.0, 1.0);
        float _dot2 = dot(L, dir_and_spot.xyz);

        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world *
                                    vec4(g_vtx_pos, 1.0);
                pp /= pp.w;
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;

                atten *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
            }

            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }

    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, g_vtx_pos);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          g_shrd_data.probes[pi].sh_coeffs[0],
                                                          g_shrd_data.probes[pi].sh_coeffs[1],
                                                          g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(4.0 * indirect_col, vec3(0.0));

    float lambert = clamp(dot(normal, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, g_vtx_sh_uvs);
    }

    vec4 text_color = textureLod(g_spec_tex, vec2(g_vtx_uvs.x, 1.0 - g_vtx_uvs.y), 0.0);

    {   // SDF drawing
        float sig_dist = median(text_color.r, text_color.g, text_color.b);

        float s = sig_dist - 0.5;
        float v = s / fwidth(s);

        albedo_color = mix(albedo_color, vec3(0.0), clamp(v + 0.5, 0.0, 1.0));
    }

    vec2 ao_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_tex, ao_uvs, 0.0).r;
    vec3 diffuse_color = albedo_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                         ambient_occlusion * indirect_col + additional_light);

    g_out_color = vec4(diffuse_color, 1.0);
    g_out_normal = vec4(normal * 0.5 + 0.5, 0.0);
    g_out_specular = vec4(0.0);
}
