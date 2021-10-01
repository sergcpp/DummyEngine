#version 460
#extension GL_EXT_ray_tracing : require

#include "_rt_common.glsl"
#include "rt_reflections_interface.glsl"

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

layout(binding = SPEC_TEX_SLOT) uniform sampler2D s_spec_texture;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D s_depth_texture;
layout(binding = NORM_TEX_SLOT) uniform sampler2D s_norm_texture;

layout(binding = SSR_TEX_SLOT) uniform sampler2D ssr_texture;

layout(binding = PREV_TEX_SLOT) uniform sampler2D prev_texture;
layout(binding = BRDF_TEX_SLOT) uniform sampler2D brdf_lut_texture;

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT tlas;
layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D out_image;

layout(location = 0) rayPayloadEXT RayPayload pld;

void main() {
    ivec2 icoord = ivec2(gl_LaunchIDEXT.xy);
    vec4 specular = texelFetch(s_spec_texture, icoord, 0);
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    float depth = texelFetch(s_depth_texture, icoord, 0).r;
 
    vec3 ssr_uvs = texelFetch(ssr_texture, icoord, 0).rgb;
    vec3 normal = 2.0 * texelFetch(s_norm_texture, icoord, 0).xyz - 1.0;
    
    const vec2 px_center = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(gl_LaunchSizeEXT.xy);
    
#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);    
#endif // VULKAN

    vec4 ray_origin_vs = shrd_data.uInvProjMatrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_ws = normalize((shrd_data.uInvViewMatrix * vec4(ray_origin_vs.xyz, 0.0)).xyz);
    vec3 refl_ray_ws = reflect(view_ray_ws, normal);

    vec4 ray_origin_ws = shrd_data.uInvViewMatrix * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    float N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
    vec2 brdf = texture(brdf_lut_texture, vec2(N_dot_V, specular.a)).xy;

    pld.col = vec4(0.0);

    if (ssr_uvs.b < 0.99) {   // trace through bvh if required
        const uint ray_flags = /*gl_RayFlagsOpaqueEXT |*/ gl_RayFlagsCullBackFacingTrianglesEXT;
        const float t_min = 0.001;
        const float t_max = 1000.0;
        
        traceRayEXT(tlas,           // topLevel
                ray_flags,          // rayFlags
                0xff,               // cullMask
                0,                  // sbtRecordOffset
                0,                  // sbtRecordStride
                0,                  // missIndex
                ray_origin_ws.xyz,  // origin
                t_min,              // Tmin
                refl_ray_ws.xyz,    // direction
                t_max,              // Tmax
                0                   // payload
                );
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);

    vec3 c0 = mix(pld.col.rgb, textureLod(prev_texture, ssr_uvs.rg, 0.0).xyz, ssr_uvs.b);
    
    vec3 out_color = imageLoad(out_image, ivec2(gl_LaunchIDEXT.xy)).rgb;
    imageStore(out_image, ivec2(gl_LaunchIDEXT.xy), vec4(out_color + c0 * (kS * brdf.x + brdf.y), 1.0));
}
