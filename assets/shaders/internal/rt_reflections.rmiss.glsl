#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_reflections_interface.glsl"

layout(binding = ENV_TEX_SLOT) uniform samplerCube env_texture;

layout(location = 0) rayPayloadInEXT RayPayload pld;

void main() {
    pld.col = clamp(RGBMDecode(textureLod(env_texture, gl_WorldRayDirectionEXT, 0.0)), vec3(0.0), vec3(16.0));
}

