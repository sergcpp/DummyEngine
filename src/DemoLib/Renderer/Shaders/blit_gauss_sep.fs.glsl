R"(
#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    vertical : 4
*/
        
uniform sampler2D s_texture;
uniform float vertical;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec4 col = texelFetch(s_texture, ivec2(aVertexUVs_), 0);

    if(vertical < 0.5) {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0) * 0.06136;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0) * 0.24477;
        outColor += col * 0.38774;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0) * 0.24477;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0) * 0.06136;
    } else {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0) * 0.06136;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0) * 0.24477;
        outColor += col * 0.38774;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0) * 0.24477;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0) * 0.06136;
    }

    outColor = mix(col, outColor, outColor.a);
}
)"