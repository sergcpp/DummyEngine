#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_reflections_interface.glsl"

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

void main() {
    g_pld.col = clamp(RGBMDecode(textureLod(g_env_tex, gl_WorldRayDirectionEXT, 0.0)), vec3(0.0), vec3(4.0)); // clamp is temporary workaround
}
