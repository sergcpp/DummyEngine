#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_debug_interface.h"

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

void main() {
    g_pld.col = clamp(RGBMDecode(texture(g_env_tex, gl_WorldRayDirectionEXT)), vec3(0.0), vec3(16.0));
}
