#version 430 core

layout(binding = 0) uniform samplerCubeArray g_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float src_layer;
                        int src_face;
                        float src_level;
};
#else
layout(location = 1) uniform float src_layer;
layout(location = 2) uniform int src_face;
layout(location = 3) uniform float src_level;
#endif

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

vec3 gen_cubemap_coord(in vec2 txc, in int face) {
    vec3 v;
    switch(face) {
        case 0: v = vec3( 1.0,   -txc.x,  txc.y); break; // +X
        case 1: v = vec3(-1.0,   -txc.x, -txc.y); break; // -X
        case 2: v = vec3(-txc.y,  1.0,    txc.x); break; // +Y
        case 3: v = vec3(-txc.y, -1.0,   -txc.x); break; // -Y
        case 4: v = vec3(-txc.y, -txc.x,  1.0); break;   // +Z
        case 5: v = vec3( txc.y, -txc.x, -1.0); break;   // -Z
    }
    return normalize(v);
}

void main() {
    g_out_color = textureLod(g_tex, vec4(gen_cubemap_coord(g_vtx_uvs, src_face), src_layer), src_level);
}
