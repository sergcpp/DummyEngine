#version 430 core

#include "_fs_common.glsl"

#pragma multi_compile FLOAT UINT DEPTH

#if defined(FLOAT)
    layout(location = 0) out vec4 g_out_color;
#elif defined(UINT)
    layout(location = 0) out uvec4 g_out_color;
#endif

void main() {
#if defined(FLOAT)
    g_out_color = vec4(0.0);
#elif defined(UINT)
    g_out_color = uvec4(0.0);
#endif
}
