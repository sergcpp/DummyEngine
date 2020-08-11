#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

in vec3 aVertexPos_;

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
