#version 430 core
#extension GL_ARB_texture_multisample : enable

#include "_fs_common.glsl"
#include "blit_ssr_compose2_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = SPEC_TEX_SLOT) uniform sampler2D g_spec_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = BRDF_TEX_SLOT) uniform sampler2D g_brdf_lut_tex;

layout(location = 0) in vec2 g_vtx_uvs;

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
        const vec4 ray_origin_cs = vec4(2.0 * g_vtx_uvs.xy - 1.0, depth, 1.0);
        const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);

        const vec3 view_ray_ws = normalize((g_shrd_data.world_from_view * vec4(ray_origin_vs, 0.0)).xyz);
        const vec3 refl_ray_ws = reflect(view_ray_ws, normal);

        vec4 ray_origin_ws = g_shrd_data.world_from_view * vec4(ray_origin_vs, 1.0);
        ray_origin_ws /= ray_origin_ws.w;

        N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
        brdf = texture(g_brdf_lut_tex, vec2(N_dot_V, specular.a)).xy;
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);
    vec3 refl_color = texelFetch(g_refl_tex, icoord, 0).rgb;

    g_out_color = vec4(refl_color * (kS * brdf.x + brdf.y), 1.0);
}
