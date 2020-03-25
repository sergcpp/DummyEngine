R"(#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision mediump float;
#endif

)"
#include "_fs_common.glsl"
R"(

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform mediump samplerCubeArray env_texture;

layout(location = 1) uniform float mip_level;
layout(location = 2) uniform int probe_index;

in vec3 aVertexPos_;

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - shrd_data.uProbes[probe_index].pos_and_radius.xyz);

    if (mip_level < 5.0) {
        // debug environment map
        outColor.rgb = RGBMDecode(textureLod(env_texture, vec4(view_dir_ws, shrd_data.uProbes[probe_index].unused_and_layer.w), mip_level));
    } else {
        const float SH_A0 = 0.886226952; // PI / sqrt(4.0f * Pi)
        const float SH_A1 = 1.02332675;  // sqrt(PI / 3.0f)

        // debug sh coeffs
        vec4 vv = vec4(SH_A0, SH_A1 * view_dir_ws.yzx);

        outColor.r = dot(shrd_data.uProbes[probe_index].sh_coeffs[0], vv);
        outColor.g = dot(shrd_data.uProbes[probe_index].sh_coeffs[1], vv);
        outColor.b = dot(shrd_data.uProbes[probe_index].sh_coeffs[2], vv);

        outColor.rgb = max(outColor.rgb, vec3(0.0));
    }

    outColor.a = 1.0;
}
)"