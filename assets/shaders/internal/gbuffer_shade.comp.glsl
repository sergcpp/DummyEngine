#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "gbuffer_shade_interface.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_texture;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_texture;
layout(binding = NORMAL_TEX_SLOT) uniform sampler2D g_normal_texture;
layout(binding = SPECULAR_TEX_SLOT) uniform sampler2D g_specular_texture;

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_texture;
layout(binding = SSAO_TEX_SLOT) uniform sampler2D g_ao_texture;
layout(binding = LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buffer;
layout(binding = DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buffer;
layout(binding = CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buffer;
layout(binding = ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buffer;

layout(binding = OUT_COLOR_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_color_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    highp float depth = texelFetch(g_depth_texture, icoord, 0).r;
    highp float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));

    int ix = int(gl_GlobalInvocationID.x), iy = int(gl_GlobalInvocationID.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    vec4 pos_cs = vec4(norm_uvs, depth, 1.0);
#if defined(VULKAN)
    pos_cs.xy = 2.0 * pos_cs.xy - 1.0;
    pos_cs.y = -pos_cs.y;
#else // VULKAN
    pos_cs.xyz = 2.0 * pos_cs.xyz - 1.0;
#endif // VULKAN

    vec4 pos_ws = g_shrd_data.inv_view_proj_matrix * pos_cs;
    pos_ws /= pos_ws.w;

    vec3 albedo = texelFetch(g_albedo_texture, icoord, 0).rgb;
    vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_texture, icoord, 0));

    //
    // Artificial lights
    //
    vec3 additional_light = vec3(0.0);
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buffer, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buffer, li * 3 + 0);
        highp vec4 col_and_index = texelFetch(g_lights_buffer, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buffer, li * 3 + 2);

        vec3 L = pos_and_radius.xyz - pos_ws.xyz;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;

        highp float denom = d / pos_and_radius.w + 1.0;
        highp float atten = 1.0 / (denom * denom);

        highp float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));

        highp float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);

        float _dot1 = clamp(dot(L, normal.xyz), 0.0, 1.0);
        float _dot2 = dot(L, dir_and_spot.xyz);

        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            highp int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                highp vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(pos_ws.xyz, 1.0);
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
                atten *= SampleShadowPCF5x5(g_shadow_texture, pp.xyz);
            }

            additional_light += col_and_index.xyz * atten * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }

    //
    // Indirect probes
    //
    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, pos_ws.xyz);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal.xyz, g_shrd_data.probes[pi].sh_coeffs[0],
                                                                      g_shrd_data.probes[pi].sh_coeffs[1],
                                                                      g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(1.0 * indirect_col, vec3(0.0));

    float lambert = clamp(dot(normal.xyz, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        vec4 g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2;

        const vec2 offsets[4] = vec2[4](
            vec2(0.0, 0.0),
            vec2(0.25, 0.0),
            vec2(0.0, 0.5),
            vec2(0.25, 0.5)
        );

        /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * pos_ws).xyz;
    #if defined(VULKAN)
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
    #else // VULKAN
            shadow_uvs = 0.5 * shadow_uvs + 0.5;
    #endif // VULKAN
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += offsets[i];
    #if defined(VULKAN)
            shadow_uvs.y = 1.0 - shadow_uvs.y;
    #endif // VULKAN
            g_vtx_sh_uvs0[i] = shadow_uvs[0];
            g_vtx_sh_uvs1[i] = shadow_uvs[1];
            g_vtx_sh_uvs2[i] = shadow_uvs[2];
        }

        visibility = GetSunVisibility(lin_depth, g_shadow_texture, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
    }

    vec2 ao_uvs = vec2(ix, iy) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_texture, ao_uvs, 0.0).r;
    vec3 diffuse_color = albedo * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                   ambient_occlusion * ambient_occlusion * indirect_col +
                                   additional_light);

    imageStore(g_out_color_img, icoord, vec4(diffuse_color, 0.0));
}
