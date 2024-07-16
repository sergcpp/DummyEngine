#version 430 core
#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "gi_cache_common.glsl"
#include "ssr_common.glsl"
#include "oit_schedule_rays_interface.h"

#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
    layout(binding = BIND_MAT_TEX4) uniform sampler2D g_alpha_tex;
#endif // !NO_BINDLESS

layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;

layout(std430, binding = RAY_COUNTER_SLOT) coherent buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = RAY_LIST_SLOT) writeonly buffer RayList {
    uint g_ray_list[];
};

layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
layout(location = 3) in vec3 g_vtx_tangent;
layout(location = 4) in float g_alpha;
#if !defined(NO_BINDLESS)
    layout(location = 5) in flat TEX_HANDLE g_norm_tex;
    layout(location = 6) in flat TEX_HANDLE g_alpha_tex;
#endif // !NO_BINDLESS
layout(location = 7) in flat vec4 g_mat_params0;

layout (early_fragment_tests) in;

void main() {
    const vec2 norm_color = texture(SAMPLER2D(g_norm_tex), g_vtx_uvs).xy;
    const float alpha = g_alpha * texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs).r;

    vec3 normal;
    normal.xy = norm_color * 2.0 - 1.0;
    normal.z = sqrt(saturate(1.0 - normal.x * normal.x - normal.y * normal.y));
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent, g_vtx_normal) * normal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }

    const vec3 P = g_vtx_pos;
    const vec3 I = normalize(P- g_shrd_data.cam_pos_and_exp.xyz);
    const vec3 N = normal.xyz;

    const float specular = g_mat_params0.z;
    if (specular > 0.0 && alpha > 0.0 && all(equal((ivec2(gl_FragCoord.xy) % 2), ivec2(0)))) {
        int frag_index = int(gl_FragCoord.y) * g_shrd_data.ires_and_ifres.x + int(gl_FragCoord.x);
        const uint ztest = floatBitsToUint(gl_FragCoord.z);
        int layer_index = -1;
        for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
            const uint zval = texelFetch(g_oit_depth_buf, frag_index).x;
            if (zval == ztest) {
                layer_index = i;
                break;
            }
            frag_index += g_shrd_data.ires_and_ifres.x * g_shrd_data.ires_and_ifres.y;
        }
        if (layer_index != -1) {
            const vec3 ray_dir = reflect(I, N);
            const vec2 oct_dir = PackUnitVector(ray_dir) * 65535.0;
            const uint packed_dir = (uint(oct_dir.y) << 16) | uint(oct_dir.x);

            const uint ray_index = atomicAdd(g_ray_counter[0], 1);
            g_ray_list[2 * ray_index + 0] = PackRay(uvec2(gl_FragCoord.xy), uint(layer_index));
            g_ray_list[2 * ray_index + 1] = packed_dir;
        }
    }
}
