
const char fillz_vs[] = R"(
#version 300 es

/*
UNIFORMS
	uMVPMatrix : 0
*/

layout(location = 0) in vec3 aVertexPosition;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
)";

const char fillz_fs[] = R"(
#version 300 es

#ifdef GL_ES
	precision mediump float;
#endif

void main() {
}
)";

const char shadow_vs[] = R"(
#version 300 es

/*
UNIFORMS
	uMVPMatrix : 0
*/

layout(location = 0) in vec3 aVertexPosition;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
)";

const char shadow_fs[] = R"(
#version 300 es
#ifdef GL_ES
    precision mediump float;
#endif

void main() {
    //gl_FragDepth = gl_FragCoord.z;
}
)";

const char blit_vs[] = R"(
#version 300 es

layout(location = 0) in vec2 aVertexPosition;
layout(location = 3) in vec2 aVertexUVs;

out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = aVertexUVs;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
)";

const char blit_fs[] = R"(
#version 300 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    multiplier : 4
*/
        
uniform sampler2D s_texture;
uniform float multiplier;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    outColor = vec4(multiplier, multiplier, multiplier, 1.0) * texelFetch(s_texture, ivec2(aVertexUVs_), 0);
}
)";

const char blit_ms_vs[] = R"(
#version 310 es

layout(location = 0) in vec2 aVertexPosition;
layout(location = 3) in vec2 aVertexUVs;

out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = aVertexUVs;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
)";

const char blit_ms_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    multiplier : 4
    uTexSize : 5
*/
        
layout(location = 14) uniform mediump sampler2DMS s_texture;
uniform float multiplier;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    outColor = vec4(multiplier, multiplier, multiplier, 1.0) * texelFetch(s_texture, ivec2(aVertexUVs_), 0);
}
    )";

const char blit_combine_fs[] = R"(
#version 310 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    s_blured_texture : 4
    uTexSize : 5
*/
        
uniform sampler2D s_texture;
uniform sampler2D s_blured_texture;
uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
    vec3 c1 = 0.1 * texture(s_blured_texture, aVertexUVs_ / uTexSize).xyz;
            
    c0 += c1;
    c0 = vec3(1.0) - exp(-c0 * exposure);
    c0 = pow(c0, vec3(1.0/gamma));

    outColor = vec4(c0, 1.0);
}
)";

const char blit_combine_ms_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    s_blured_texture : 4
    uTexSize : 5
*/
        
uniform mediump sampler2DMS s_texture;
uniform sampler2D s_blured_texture;
uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
	vec3 c1 = texelFetch(s_texture, ivec2(aVertexUVs_), 1).xyz;
	vec3 c2 = texelFetch(s_texture, ivec2(aVertexUVs_), 2).xyz;
	vec3 c3 = texelFetch(s_texture, ivec2(aVertexUVs_), 3).xyz;
    vec3 c4 = 0.1 * texture(s_blured_texture, aVertexUVs_ / uTexSize).xyz;
            
    c0 += c4;
    c1 += c4;
    c2 += c4;
    c3 += c4;

    //c0 = exposure * c0 / (c0 + vec3(1.0));
    //c1 = exposure * c1 / (c1 + vec3(1.0));
    //c2 = exposure * c2 / (c2 + vec3(1.0));
    //c3 = exposure * c3 / (c3 + vec3(1.0));

    c0 = vec3(1.0) - exp(-c0 * exposure);
    c1 = vec3(1.0) - exp(-c1 * exposure);
    c2 = vec3(1.0) - exp(-c2 * exposure);
    c3 = vec3(1.0) - exp(-c3 * exposure);

    c0 = pow(c0, vec3(1.0/gamma));
    c1 = pow(c1, vec3(1.0/gamma));
    c2 = pow(c2, vec3(1.0/gamma));
    c3 = pow(c3, vec3(1.0/gamma));

    outColor = vec4(0.25 * (c0 + c1 + c2 + c3), 1.0);
}
)";

const char blit_reduced_fs[] = R"(
#version 300 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    uOffset : 4
*/
        
uniform sampler2D s_texture;
uniform vec2 uOffset;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + uOffset).xyz;
    outColor.r = 0.299 * c0.r + 0.587 * c0.g + 0.114 * c0.b;
}
)";

const char blit_down_fs[] = R"(
#version 300 es

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
*/
        
uniform sampler2D s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 col = vec3(0.0);
    for (float j = -1.5; j < 2.0; j += 1.0) {
        for (float i = -1.5; i < 2.0; i += 1.0) {
            col += texelFetch(s_texture, ivec2(aVertexUVs_ + vec2(i, j)), 0).xyz;
        }
    }
    outColor = vec4((1.0/16.0) * col, 1.0);
}
    )";

const char blit_down_ms_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
*/
        
uniform mediump sampler2DMS s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 col = vec3(0.0);
    for (float j = -1.5; j < 2.0; j += 1.0) {
        for (float i = -1.5; i < 2.0; i += 1.0) {
            col += texelFetch(s_texture, ivec2(aVertexUVs_ + vec2(i, j)), 0).xyz;
        }
    }
    outColor = vec4((1.0/16.0) * col, 1.0);
}
    )";

const char blit_gauss_fs[] = R"(
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
    if(vertical < 1.0) {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(4, 0), 0) * 0.05;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(3, 0), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.16;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(3, 0), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(4, 0), 0) * 0.05;
    } else {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 4), 0) * 0.05;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 3), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.16;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 3), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 4), 0) * 0.05;
    }
}
)";

const char blit_debug_fs[] = R"(
#version 310 es
#extension GL_EXT_texture_buffer : enable

#ifdef GL_ES
	precision mediump float;
#endif

#define GRID_RES_X )" AS_STR(REN_GRID_RES_X) R"(
#define GRID_RES_Y )" AS_STR(REN_GRID_RES_Y) R"(
#define GRID_RES_Z )" AS_STR(REN_GRID_RES_Z) R"(
        
layout(binding = 0) uniform mediump sampler2D s_texture;
layout(binding = 12) uniform highp usamplerBuffer cells_buffer;
layout(binding = 13) uniform highp usamplerBuffer items_buffer;

layout(location = 16) uniform int resx;
layout(location = 17) uniform int resy;
layout(location = 18) uniform int mode;

in vec2 aVertexUVs_;

out vec4 outColor;

vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

void main() {
    const float n = 0.5;
    const float f = 10000.0;

    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    depth = 2.0 * depth - 1.0;
    depth = 2.0 * n * f / (f + n - depth * (f - n));
    
    float k = log2(depth / n) / log2(f / n);
    int slice = int(k * 24.0);
    
    int ix = int(gl_FragCoord.x);
    int iy = int(gl_FragCoord.y);
    int cell_index = slice * GRID_RES_X * GRID_RES_Y + (iy * GRID_RES_Y / resy) * GRID_RES_X + ix * GRID_RES_X / resx;
    
    uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    uvec2 offset_and_lcount = uvec2(cell_data.x & 0x00ffffffu, cell_data.x >> 24);
    uvec2 dcount_and_pcount = uvec2(cell_data.y & 0x000000ffu, 0);

    if (mode == 0) {
        outColor = vec4(heatmap(float(offset_and_lcount.y) * (1.0 / 8.0)), 0.85);
    } else if (mode == 1) {
        outColor = vec4(heatmap(float(dcount_and_pcount.x) * (1.0 / 8.0)), 0.85);
    }

    int xy_cell = (iy * GRID_RES_Y / resy) * GRID_RES_X + ix * GRID_RES_X / resx;
    int xy_cell_right = (iy * GRID_RES_Y / resy) * GRID_RES_X + (ix + 1) * GRID_RES_X / resx;
    int xy_cell_up = ((iy + 1) * GRID_RES_Y / resy) * GRID_RES_X + ix * GRID_RES_X / resx;

    if (xy_cell_right != xy_cell || xy_cell_up != xy_cell) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
)";

const char blit_debug_ms_fs[] = R"(
#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

#define GRID_RES_X )" AS_STR(REN_GRID_RES_X) R"(
#define GRID_RES_Y )" AS_STR(REN_GRID_RES_Y) R"(
#define GRID_RES_Z )" AS_STR(REN_GRID_RES_Z) R"(
        
layout(binding = 0) uniform mediump sampler2DMS s_texture;
layout(binding = 12) uniform highp usamplerBuffer cells_buffer;
layout(binding = 13) uniform highp usamplerBuffer items_buffer;

layout(location = 16) uniform int resx;
layout(location = 17) uniform int resy;
layout(location = 18) uniform int mode;

in vec2 aVertexUVs_;

out vec4 outColor;

vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

void main() {
    const float n = 0.5;
    const float f = 10000.0;

    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    depth = 2.0 * depth - 1.0;
    depth = 2.0 * n * f / (f + n - depth * (f - n));
    
    float k = log2(depth / n) / log2(f / n);
    int slice = int(k * 24.0);
    
    int ix = int(gl_FragCoord.x);
    int iy = int(gl_FragCoord.y);
    int cell_index = slice * GRID_RES_X * GRID_RES_Y + (iy * GRID_RES_Y / resy) * GRID_RES_X + ix * GRID_RES_X / resx;
    
    uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    uvec2 offset_and_lcount = uvec2(cell_data.x & 0x00ffffffu, cell_data.x >> 24);
    uvec2 dcount_and_pcount = uvec2(cell_data.y & 0x000000ffu, 0);

    if (mode == 0) {
        outColor = vec4(heatmap(float(offset_and_lcount.y) * (1.0 / 8.0)), 0.85);
    } else if (mode == 1) {
        outColor = vec4(heatmap(float(dcount_and_pcount.x) * (1.0 / 8.0)), 0.85);
    }

    int xy_cell = (iy * GRID_RES_Y / resy) * GRID_RES_X + ix * GRID_RES_X / resx;
    int xy_cell_right = (iy * GRID_RES_Y / resy) * GRID_RES_X + (ix + 1) * GRID_RES_X / resx;
    int xy_cell_up = ((iy + 1) * GRID_RES_Y / resy) * GRID_RES_X + ix * GRID_RES_X / resx;

    if (xy_cell_right != xy_cell || xy_cell_up != xy_cell) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
)";