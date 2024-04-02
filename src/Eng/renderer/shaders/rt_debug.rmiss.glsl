#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "_principled.glsl"
#include "rt_debug_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer LightsData {
    light_item_t g_lights[];
};

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(location = 0) rayPayloadInEXT RayPayload g_pld;

void main() {
    g_pld.col = texture(g_env_tex, gl_WorldRayDirectionEXT).rgb;

    for (int i = 0; i < MAX_PORTALS_TOTAL && g_shrd_data.portals[i] != 0xffffffff; ++i) {
        const light_item_t litem = g_lights[g_shrd_data.portals[i]];

        const vec3 light_pos = litem.pos_and_radius.xyz;
        vec3 light_u = litem.u_and_reg.xyz, light_v = litem.v_and_blend.xyz;
        const vec3 light_forward = normalize(cross(light_u, light_v));

        const float plane_dist = dot(light_forward, light_pos);
        const float cos_theta = dot(gl_WorldRayDirectionEXT, light_forward);
        const float t = (plane_dist - dot(light_forward, gl_WorldRayOriginEXT)) / min(cos_theta, -FLT_EPS);

        if (cos_theta < 0.0 && t > 0.0) {
            light_u /= dot(light_u, light_u);
            light_v /= dot(light_v, light_v);

            const vec3 p = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * t;
            const vec3 vi = p - light_pos;
            const float a1 = dot(light_u, vi);
            if (a1 >= -1.0 && a1 <= 1.0) {
                const float a2 = dot(light_v, vi);
                if (a2 >= -1.0 && a2 <= 1.0) {
                    g_pld.col *= vec3(1.0, 0.0, 0.0);
                    break;
                }
            }
        }
    }
}
