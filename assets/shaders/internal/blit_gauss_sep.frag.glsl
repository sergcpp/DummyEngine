#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    vertical : 4
*/
        
layout(binding = 0) uniform sampler2D s_texture;
layout(location = 1) uniform float vertical;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    vec4 col = texelFetch(s_texture, ivec2(aVertexUVs_), 0);

    /*if(vertical < 0.5) {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(4, 0), 0) * 0.0162162162;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(3, 0), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.2270270270;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(3, 0), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(4, 0), 0) * 0.0162162162;
    } else {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 4), 0) * 0.0162162162;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 3), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.2270270270;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 3), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 4), 0) * 0.0162162162;
    }*/

    float alpha_weightened = 0.0;

    outColor = vec4(0.0);

    if(vertical < 0.5) {
        vec4 c;
        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(4, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0162162162;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(3, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(s_texture, ivec2(aVertexUVs_), 0);
        outColor += c;
        alpha_weightened += c.a * 0.2270270270;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(3, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(4, 0), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0162162162;
    } else {
        vec4 c;
        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 4), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0162162162;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 3), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(s_texture, ivec2(aVertexUVs_), 0);
        outColor += c;
        alpha_weightened += c.a * 0.2270270270;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0);
        outColor += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 3), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 4), 0);
        outColor += c;
        alpha_weightened += c.a * 0.0162162162;
    }

    if (outColor.a < 0.9) {
        //outColor.a = 0.0;
    }

    //outColor = mix(col, outColor, outColor.a);
    //outColor = vec4(outColor.xyz / outColor.b, alpha_weightened);

    if (false && col.b < 0.5 && outColor.b > 0.01) {
        outColor.rgb /= outColor.b;
    } else {
        outColor.rgb = col.rgb;
    }

    outColor.a = alpha_weightened;

    if (col.b > 0.9) {
        //outColor.xyz = col.xyz;
    }
}
