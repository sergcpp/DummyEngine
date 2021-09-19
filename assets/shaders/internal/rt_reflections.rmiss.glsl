#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_reflections_interface.glsl"

layout(binding = ENV_TEX_SLOT) uniform samplerCube env_texture;

layout(location = 0) rayPayloadInEXT RayPayload pld;

void main() {
    vec3 env_col = clamp(RGBMDecode(texture(env_texture, gl_WorldRayDirectionEXT)), vec3(0.0), vec3(16.0));
    pld.col = vec4(env_col, 1.0);
}

