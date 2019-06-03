R"(
#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform mediump samplerCubeArray s_texture;
layout(location = 1) uniform float src_layer;
layout(location = 2) uniform int src_face;
layout(location = 3) uniform float src_level;

in vec2 aVertexUVs_;

out vec4 outColor;

vec3 gen_cubemap_coord(in vec2 txc, in int face) {
    vec3 v;
    switch(face)
    {
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
    outColor = textureLod(s_texture, vec4(gen_cubemap_coord(aVertexUVs_, src_face), src_layer), src_level);
}
)"