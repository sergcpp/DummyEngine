#version 310 es
#extension GL_ARB_texture_multisample : enable
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_ssr_compose2_interface.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = SPEC_TEX_SLOT) uniform highp sampler2D spec_texture;
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D depth_texture;
layout(binding = NORM_TEX_SLOT) uniform highp sampler2D norm_texture;
layout(binding = REFL_TEX_SLOT) uniform highp sampler2D refl_texture;
layout(binding = BRDF_TEX_SLOT) uniform sampler2D brdf_lut_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.0);

    ivec2 icoord = ivec2(gl_FragCoord.xy);

    vec4 specular = texelFetch(spec_texture, icoord, 0);
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    float depth = texelFetch(depth_texture, icoord, 0).r;
    float d0 = LinearizeDepth(depth, shrd_data.uClipInfo);

    vec3 normal = UnpackNormalAndRoughness(texelFetch(norm_texture, icoord, 0)).xyz;

    float tex_lod = 6.0 * specular.a;
    float N_dot_V;

    vec3 c0 = vec3(0.0);
    vec2 brdf;

    {
#if defined(VULKAN)
        vec4 ray_origin_cs = vec4(2.0 * aVertexUVs_.xy - 1.0, depth, 1.0);
        ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
        vec4 ray_origin_cs = vec4(2.0 * vec3(aVertexUVs_.xy, depth) - 1.0, 1.0);
#endif // VULKAN

        vec4 ray_origin_vs = shrd_data.uInvProjMatrix * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_ws = normalize((shrd_data.uInvViewMatrix * vec4(ray_origin_vs.xyz, 0.0)).xyz);
        vec3 refl_ray_ws = reflect(view_ray_ws, normal);

        vec4 ray_origin_ws = shrd_data.uInvViewMatrix * ray_origin_vs;
        ray_origin_ws /= ray_origin_ws.w;

        N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
        brdf = texture(brdf_lut_texture, vec2(N_dot_V, specular.a)).xy;
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);
    vec3 refl_color = texelFetch(refl_texture, icoord, 0).rgb;

    outColor = vec4(refl_color * (kS * brdf.x + brdf.y), 1.0);
}
