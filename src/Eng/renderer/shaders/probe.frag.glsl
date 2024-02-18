#version 320 es
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = BIND_BASE0_TEX) uniform mediump samplerCubeArray g_env_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 64) float g_mip_level;
                        int g_probe_index;
};
#else
layout(location = 1) uniform float g_mip_level;
layout(location = 2) uniform int g_probe_index;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec3 g_vtx_pos;
#else
in vec3 g_vtx_pos;
#endif

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    vec3 view_dir_ws = normalize(g_vtx_pos - g_shrd_data.probes[g_probe_index].pos_and_radius.xyz);

    if (g_mip_level < 5.0) {
        // debug environment map
        g_out_color.rgb = RGBMDecode(textureLod(g_env_tex, vec4(view_dir_ws, g_shrd_data.probes[g_probe_index].unused_and_layer.w), g_mip_level));
    } else {
        g_out_color.rgb = EvalSHIrradiance_NonLinear(view_dir_ws,
                                                  g_shrd_data.probes[g_probe_index].sh_coeffs[0],
                                                  g_shrd_data.probes[g_probe_index].sh_coeffs[1],
                                                  g_shrd_data.probes[g_probe_index].sh_coeffs[2]);
    }

    g_out_color.a = 1.0;
}
