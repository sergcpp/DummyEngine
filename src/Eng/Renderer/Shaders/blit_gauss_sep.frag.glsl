#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

/*
UNIFORMS
    g_tex : 3
    vertical : 4
*/

layout(binding = 0) uniform sampler2D g_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float vertical;
};
#else
layout(location = 1) uniform float vertical;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    vec4 col = texelFetch(g_tex, ivec2(g_vtx_uvs), 0);

    /*if(vertical < 0.5) {
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(4, 0), 0) * 0.0162162162;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(3, 0), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(2, 0), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(1, 0), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs), 0) * 0.2270270270;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(1, 0), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(2, 0), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(3, 0), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(4, 0), 0) * 0.0162162162;
    } else {
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 4), 0) * 0.0162162162;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 3), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 2), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 1), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs), 0) * 0.2270270270;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 1), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 2), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 3), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 4), 0) * 0.0162162162;
    }*/

    float alpha_weightened = 0.0;

    g_out_color = vec4(0.0);

    if(vertical < 0.5) {
        vec4 c;
        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(4, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0162162162;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(3, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(2, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(1, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.2270270270;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(1, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(2, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(3, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(4, 0), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0162162162;
    } else {
        vec4 c;
        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 4), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0162162162;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 3), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 2), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 1), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.2270270270;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 1), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1945945946;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 2), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.1216216216;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 3), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0540540541;

        c = texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 4), 0);
        g_out_color += c;
        alpha_weightened += c.a * 0.0162162162;
    }

    if (g_out_color.a < 0.9) {
        //g_out_color.a = 0.0;
    }

    //g_out_color = mix(col, g_out_color, g_out_color.a);
    //g_out_color = vec4(g_out_color.xyz / g_out_color.b, alpha_weightened);

    if (false && col.b < 0.5 && g_out_color.b > 0.01) {
        g_out_color.rgb /= g_out_color.b;
    } else {
        g_out_color.rgb = col.rgb;
    }

    g_out_color.a = alpha_weightened;

    if (col.b > 0.9) {
        //g_out_color.xyz = col.xyz;
    }
}
