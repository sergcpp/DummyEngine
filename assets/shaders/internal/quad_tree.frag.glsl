#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

layout(location = 1) uniform float uSrcLevel;
layout(location = 2) uniform highp vec2 uInvSrcRes;

in vec2 aVertexUVs_;

out vec4 outColor;

vec4 min3(vec4 v0, vec4 v1, vec4 v2) {
    return min(v0, min(v1, v2));
}

void main() {
    vec4 cmin;

    { // initial quad (4 fetches)
      /*
       _|_____|_____|_____|_____|_
        |     |     |     |     |
        |     |     |     |     |
       _|_____|_____|_____|_____|_
        |     |     |     |     |
        |     |  *  |  *  |     |
       _|_____|____   ____|_____|_
        |     |           |     |
        |     |  *  |  *  |     |
       _|_____|_____|_____|_____|_
        |     |     |     |     |
        |     |     |     |     |
       _|_____|_____|_____|_____|_
        |     |     |     |     |
      */

        ivec2 icoord = 2 * ivec2(gl_FragCoord.xy);
        vec4 c00 = texelFetch(s_texture, icoord + ivec2(0, 0), int(uSrcLevel));
        vec4 c01 = texelFetch(s_texture, icoord + ivec2(0, 1), int(uSrcLevel));
        vec4 c10 = texelFetch(s_texture, icoord + ivec2(1, 0), int(uSrcLevel));
        vec4 c11 = texelFetch(s_texture, icoord + ivec2(1, 1), int(uSrcLevel));
        cmin = min(min(c00, c01), min(c10, c11));
    }

    { // interpolated pairs of pixels (8 reads, 12 interpolated values)
      /*
       _|_____|_____|_____|_____|_
        |     |     |     |     |
        |     |     |     |     |
       _|_____|__*__|__*__|_____|_
        |     |  *  |  *  |     |
        |    * *    |    * *    |
       _|_____|____   ____|_____|_
        |     |           |     |
        |    * *    |    * *    |
       _|_____|__*__|__*__|_____|_
        |     |  *  |  *  |     |
        |     |     |     |     |
       _|_____|_____|_____|_____|_
        |     |     |     |     |
      */

        vec4 c0, c1;
        c0 = textureLod(s_texture, aVertexUVs_ + vec2(-0.5 * uInvSrcRes.x, -uInvSrcRes.y), uSrcLevel);
        c1 = textureLod(s_texture, aVertexUVs_ + vec2(-uInvSrcRes.x, -0.5 * uInvSrcRes.y), uSrcLevel);
        cmin = min3(cmin, c0, c1);

        c0 = textureLod(s_texture, aVertexUVs_ + vec2(-uInvSrcRes.x, 0.5 * uInvSrcRes.y), uSrcLevel);
        c1 = textureLod(s_texture, aVertexUVs_ + vec2(-0.5 * uInvSrcRes.x, uInvSrcRes.y), uSrcLevel);
        cmin = min3(cmin, c0, c1);

        c0 = textureLod(s_texture, aVertexUVs_ + vec2(0.5 * uInvSrcRes.x, uInvSrcRes.y), uSrcLevel);
        c1 = textureLod(s_texture, aVertexUVs_ + vec2(uInvSrcRes.x, 0.5 * uInvSrcRes.y), uSrcLevel);
        cmin = min3(cmin, c0, c1);

        c0 = textureLod(s_texture, aVertexUVs_ + vec2(uInvSrcRes.x, -0.5 * uInvSrcRes.y), uSrcLevel);
        c1 = textureLod(s_texture, aVertexUVs_ + vec2(0.5 * uInvSrcRes.x, -uInvSrcRes.y), uSrcLevel);
        cmin = min3(cmin, c0, c1);
    }

    { // interpolated quads of pixels (7 reads, 16 interpolated values)
      /*
       _|_____|_____|_____|_____|_
        |     |     |     |     |
        |     |     |     |     |
       _|____* *___* *___* *____|_
        |    * *   * *   * *    |
        |     |     |     |     |
       _|____* *___   ___* *____|_
        |    * *         * *    |
        |     |     |     |     |
       _|____* *___* *___* *____|_
        |    * *   * *   * *    |
        |     |     |     |     |
       _|_____|_____|_____|_____|_
        |     |     |     |     |
      */

        vec4 c0, c1, c2;
        c0 = textureLod(s_texture, aVertexUVs_ + vec2(-uInvSrcRes.x, -uInvSrcRes.y), uSrcLevel);
        c1 = textureLod(s_texture, aVertexUVs_ + vec2(0.0, -uInvSrcRes.y), uSrcLevel);
        c2 = textureLod(s_texture, aVertexUVs_ + vec2(+uInvSrcRes.x, -uInvSrcRes.y), uSrcLevel);
        cmin = min(cmin, min3(c0, c1, c2));

        c0 = textureLod(s_texture, aVertexUVs_ + vec2(-uInvSrcRes.x, 0.0), uSrcLevel);
        // (skip center)
        c2 = textureLod(s_texture, aVertexUVs_ + vec2(+uInvSrcRes.x, 0.0), uSrcLevel);
        cmin = min3(cmin, c0, c2);

        c0 = textureLod(s_texture, aVertexUVs_ + vec2(-uInvSrcRes.x, uInvSrcRes.y), uSrcLevel);
        c1 = textureLod(s_texture, aVertexUVs_ + vec2(0.0, uInvSrcRes.y), uSrcLevel);
        c2 = textureLod(s_texture, aVertexUVs_ + vec2(+uInvSrcRes.x, uInvSrcRes.y), uSrcLevel);
        cmin = min(cmin, min3(c0, c1, c2));
    }

    outColor = cmin;
}
