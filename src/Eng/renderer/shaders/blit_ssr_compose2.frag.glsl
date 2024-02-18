#version 320 es
#extension GL_ARB_texture_multisample : enable
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_ssr_compose2_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = SPEC_TEX_SLOT) uniform highp sampler2D g_spec_tex;
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform highp sampler2D g_norm_tex;
layout(binding = REFL_TEX_SLOT) uniform highp sampler2D g_refl_tex;
layout(binding = BRDF_TEX_SLOT) uniform sampler2D g_brdf_lut_tex;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    g_out_color = vec4(0.0);

    ivec2 icoord = ivec2(gl_FragCoord.xy);

    vec4 specular = texelFetch(g_spec_tex, icoord, 0);
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    float d0 = LinearizeDepth(depth, g_shrd_data.clip_info);

    vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0)).xyz;

    float tex_lod = 6.0 * specular.a;
    float N_dot_V;

    vec3 c0 = vec3(0.0);
    vec2 brdf;

    {
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

        N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
        brdf = texture(g_brdf_lut_tex, vec2(N_dot_V, specular.a)).xy;
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);
    vec3 refl_color = texelFetch(g_refl_tex, icoord, 0).rgb;

    g_out_color = vec4(refl_color * (kS * brdf.x + brdf.y), 1.0);
}
