#version 320 es
#extension GL_ARB_texture_multisample : enable
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_ssr_compose_interface.h"

/*
PERM @MSAA_4
PERM @HALFRES
PERM @HALFRES;MSAA_4
*/

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(MSAA_4)
layout(binding = BIND_REFL_SPEC_TEX) uniform mediump sampler2DMS g_spec_tex;
layout(binding = BIND_REFL_DEPTH_TEX) uniform highp sampler2DMS g_depth_tex;
layout(binding = BIND_REFL_NORM_TEX) uniform highp sampler2DMS g_norm_tex;
#else
layout(binding = BIND_REFL_SPEC_TEX) uniform highp sampler2D g_spec_tex;
layout(binding = BIND_REFL_DEPTH_TEX) uniform highp sampler2D g_depth_tex;
layout(binding = BIND_REFL_NORM_TEX) uniform highp sampler2D g_norm_tex;
#endif
layout(binding = BIND_REFL_DEPTH_LOW_TEX) uniform highp sampler2D g_depth_low_tex;
layout(binding = BIND_REFL_SSR_TEX) uniform highp sampler2D g_ssr_tex;

layout(binding = BIND_REFL_PREV_TEX) uniform highp sampler2D g_prev_tex;
layout(binding = BIND_REFL_BRDF_TEX) uniform sampler2D g_brdf_lut_tex;
layout(binding = BIND_ENV_TEX) uniform mediump samplerCubeArray g_probe_textures;
layout(binding = BIND_CELLS_BUF) uniform highp usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform highp usamplerBuffer g_items_buf;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    g_out_color = vec4(0.0);

    ivec2 icoord = ivec2(gl_FragCoord.xy);

#if defined(MSAA_4)
    vec4 specular = 0.25 * (texelFetch(g_spec_tex, icoord, 0) +
                            texelFetch(g_spec_tex, icoord, 1) +
                            texelFetch(g_spec_tex, icoord, 2) +
                            texelFetch(g_spec_tex, icoord, 3));
#else
    vec4 specular = texelFetch(g_spec_tex, icoord, 0);
#endif
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    float d0 = LinearizeDepth(depth, g_shrd_data.clip_info);

#if defined(HALFRES)
    ivec2 icoord_low = ivec2(gl_FragCoord.xy) / 2;

    float d1 = abs(d0 - texelFetch(g_depth_low_tex, icoord_low + ivec2(0, 0), 0).r);
    float d2 = abs(d0 - texelFetch(g_depth_low_tex, icoord_low + ivec2(0, 1), 0).r);
    float d3 = abs(d0 - texelFetch(g_depth_low_tex, icoord_low + ivec2(1, 0), 0).r);
    float d4 = abs(d0 - texelFetch(g_depth_low_tex, icoord_low + ivec2(1, 1), 0).r);

    float dmin = min(min(d1, d2), min(d3, d4));

    vec3 ssr_uvs;
    if (dmin < 0.05) {
        ssr_uvs = textureLod(g_ssr_tex, g_vtx_uvs * g_shrd_data.res_and_fres.xy / g_shrd_data.res_and_fres.zw, 0.0).rgb;
    } else {
        if (dmin == d1) {
            ssr_uvs = texelFetch(g_ssr_tex, icoord_low + ivec2(0, 0), 0).rgb;
        } else if (dmin == d2) {
            ssr_uvs = texelFetch(g_ssr_tex, icoord_low + ivec2(0, 1), 0).rgb;
        } else if (dmin == d3) {
            ssr_uvs = texelFetch(g_ssr_tex, icoord_low + ivec2(1, 0), 0).rgb;
        } else {
            ssr_uvs = texelFetch(g_ssr_tex, icoord_low + ivec2(1, 1), 0).rgb;
        }
    }
#else // HALFRES
    vec3 ssr_uvs = texelFetch(g_ssr_tex, icoord, 0).rgb;
#endif // HALFRES

    vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0)).xyz;

    float tex_lod = 6.0 * specular.a;
    float N_dot_V;

    vec3 c0 = vec3(0.0);
    vec2 brdf;

    {   // apply cubemap contribution
#if defined(VULKAN)
        vec4 ray_origin_cs = vec4(2.0 * g_vtx_uvs.xy - 1.0, depth, 1.0);
        ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
        vec4 ray_origin_cs = vec4(2.0 * vec3(g_vtx_uvs.xy, depth) - 1.0, 1.0);
#endif // VULKAN

        vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_ws = normalize((g_shrd_data.inv_view_matrix * vec4(ray_origin_vs.xyz, 0.0)).xyz);
        vec3 refl_ray_ws = reflect(view_ray_ws, normal);

        vec4 ray_origin_ws = g_shrd_data.inv_view_matrix * ray_origin_vs;
        ray_origin_ws /= ray_origin_ws.w;

        highp float k = log2(d0 / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
        int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

        int ix = icoord.x, iy = icoord.y;
        int cell_index = slice * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + (iy * ITEM_GRID_RES_Y / int(g_shrd_data.res_and_fres.y)) * ITEM_GRID_RES_X + (ix * ITEM_GRID_RES_X / int(g_shrd_data.res_and_fres.x));

        highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
        highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
        highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);

        float total_fade = 0.0;

        for (uint i = offset; i < offset + pcount; i++) {
            highp uint item_data = texelFetch(g_items_buf, int(i)).x;
            int pi = int(bitfieldExtract(item_data, 24, 8));

            float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, ray_origin_ws.xyz);
            float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);
            c0 += fade * RGBMDecode(textureLod(g_probe_textures, vec4(refl_ray_ws, g_shrd_data.probes[pi].unused_and_layer.w), tex_lod));
            total_fade += fade;
        }

        c0 /= max(total_fade, 1.0);

        N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
        brdf = texture(g_brdf_lut_tex, vec2(N_dot_V, specular.a)).xy;
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);

    c0 = mix(c0, textureLod(g_prev_tex, ssr_uvs.rg, 0.0).xyz, ssr_uvs.b);
    g_out_color = vec4(c0 * (kS * brdf.x + brdf.y), 1.0);
}
