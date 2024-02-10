#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_diff_tex;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D g_norm_tex;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D g_spec_tex;
layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D g_mask_tex;
#endif // BINDLESS_TEXTURES
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = REN_LMAP_SH_SLOT) uniform sampler2D g_lm_indirect_sh_texture[4];
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D g_decals_tex;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray g_env_tex;
layout(binding = REN_LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buf;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;
layout(binding = REN_CONE_RT_LUT_SLOT) uniform lowp sampler2D g_cone_rt_lut;

layout (binding = REN_UB_SHARED_DATA_LOC, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT(location = 0) in highp vec3 g_vtx_pos;
LAYOUT(location = 1) in mediump vec2 g_vtx_uvs;
LAYOUT(location = 2) in mediump vec3 g_vtx_normal;
LAYOUT(location = 3) in mediump vec3 g_vtx_tangent;
LAYOUT(location = 4) in highp vec4 g_vtx_sh_uvs0;
LAYOUT(location = 5) in highp vec4 g_vtx_sh_uvs1;
LAYOUT(location = 6) in highp vec4 g_vtx_sh_uvs2;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 7) in flat TEX_HANDLE g_diff_tex;
    LAYOUT(location = 8) in flat TEX_HANDLE g_norm_tex;
    LAYOUT(location = 9) in flat TEX_HANDLE g_spec_tex;
    LAYOUT(location = 10) in flat TEX_HANDLE g_mask_tex;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(REN_GRID_RES_Z)), 0, REN_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));

    vec3 diff_color = SRGBToLinear(YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), g_vtx_uvs)));
    float mask_value = texture(SAMPLER2D(g_mask_tex), g_vtx_uvs).r;

    vec2 duv_dx = dFdx(g_vtx_uvs), duv_dy = dFdy(g_vtx_uvs);
    vec3 normal_color = texture(SAMPLER2D(g_norm_tex), g_vtx_uvs).wyz;
    vec4 specular_color = texture(SAMPLER2D(g_spec_tex), g_vtx_uvs);

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent,
                            g_vtx_normal) * normal);

    if (!gl_FrontFacing) {
        normal = -normal;
    }

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        highp vec4 col_and_index = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);

        vec3 L = pos_and_radius.xyz - g_vtx_pos;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;

        highp float denom = d / pos_and_radius.w + 1.0;
        highp float atten = 1.0 / (denom * denom);

        highp float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));

        highp float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);

        float _dot1 = clamp(dot(L, normal), 0.0, 1.0);
        float _dot2 = dot(L, dir_and_spot.xyz);

        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                highp vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(g_vtx_pos, 1.0);
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
                atten *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
            }

            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }

    vec3 view_ray_ws = normalize(g_vtx_pos - g_shrd_data.cam_pos_and_gamma.xyz);
    vec3 refl_ray_ws = reflect(view_ray_ws, normal);

    float refl_lod = 6.0 * specular_color.a;

    vec3 indirect_col = vec3(0.0);
    vec3 reflected_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, g_vtx_pos);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          g_shrd_data.probes[pi].sh_coeffs[0],
                                                          g_shrd_data.probes[pi].sh_coeffs[1],
                                                          g_shrd_data.probes[pi].sh_coeffs[2]);
        reflected_col += fade * RGBMDecode(
                textureLod(g_env_tex, vec4(refl_ray_ws,
                                             g_shrd_data.probes[pi].unused_and_layer.w), refl_lod));
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(indirect_col, vec3(0.0));
    reflected_col /= max(total_fade, 1.0);

    float lambert = clamp(dot(normal, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
    }

    vec2 ao_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_tex, ao_uvs, 0.0).r;
    vec3 diffuse_color = diff_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                       ambient_occlusion * ambient_occlusion * indirect_col +
                                       additional_light);

    float N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, specular_color.rgb, specular_color.a);

    g_out_color = vec4(diffuse_color * kD, mask_value);
    //g_out_normal = vec4(normal * 0.5 + 0.5, 1.0);
    g_out_specular = vec4(0.0);
}
