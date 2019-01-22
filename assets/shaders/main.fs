#version 310 es
#extension GL_EXT_texture_buffer : enable

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#define GRID_RES_X 16
#define GRID_RES_Y 8
#define GRID_RES_Z 24

#define FLT_EPS 0.0000001f
#define LIGHT_ATTEN_CUTOFF 0.001f

layout(binding = 0) uniform sampler2D diffuse_texture;
layout(binding = 1) uniform sampler2D normals_texture;
layout(binding = 2) uniform sampler2DShadow shadow_texture;
layout(binding = 3) uniform sampler2D lm_direct_texture;
layout(binding = 4) uniform sampler2D lm_indirect_texture;
layout(binding = 5) uniform sampler2D lm_indirect_sh_texture[4];
layout(binding = 9) uniform highp samplerBuffer lights_buffer;
layout(binding = 10) uniform highp usamplerBuffer cells_buffer;
layout(binding = 11) uniform highp usamplerBuffer items_buffer;

layout (std140) uniform MatricesBlock {
    mat4 uMVPMatrix;
    mat4 uVPMatrix;
    mat4 uMMatrix;
    mat4 uShadowMatrix[4];
};

layout(location = 12) uniform vec3 sun_dir;
layout(location = 13) uniform vec3 sun_col;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform int lights_count;
layout(location = 16) uniform int resx;
layout(location = 17) uniform int resy;

in vec3 aVertexPos_;
in mat3 aVertexTBN_;
in vec2 aVertexUVs1_;
in vec2 aVertexUVs2_;

in vec3 aVertexShUVs_[4];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outSpecular;

vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

void main(void) {
    const vec2 poisson_disk[32] = vec2[32](
        vec2(-0.5, 0.0),
        vec2(0.0, 0.5),
        vec2(0.5, 0.0),
        vec2(0.0, -0.5),
    
        vec2(0.0, 0.0),
        vec2(-0.1, -0.32),
        vec2(0.17, 0.31),
        vec2(0.35, 0.04),
        
        vec2(0.07, 0.7),
        vec2(-0.72, 0.09),
        vec2(0.73, 0.05),
        vec2(0.1, -0.71),
        
        vec2(0.72, 0.8),
        vec2(-0.75, 0.74),
        vec2(-0.8, -0.73),
        vec2(0.75, -0.81),
        
        vec2(-0.26, 0.75),
        vec2(0.79, 0.36),
        vec2(0.79, -0.42),
        vec2(-0.36, -0.76),
        
        vec2(-0.83, -0.34),
        vec2(0.43, -0.9),
        vec2(0.35, 0.82),
        vec2(-0.8, 0.4),
        
        vec2(-0.42, 0.18),
        vec2(0.23, 0.36),
        vec2(0.3, -0.33),
        vec2(-0.41, -0.27),
        
        vec2(-0.28, 0.97),
        vec2(0.95, -0.19),
        vec2(-0.11, -0.94),
        vec2(-0.99, 0.1)
    );

    vec3 normal = texture(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
    normal = aVertexTBN_ * normal;
    
    vec2 lm_uvs = vec2(aVertexUVs2_.x, 1.0 - aVertexUVs2_.y);

    const float shadow_softness = 2.0 / 2048.0;

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    float lambert = max(dot(normal, sun_dir), 0.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        float frag_depth = gl_FragCoord.z / gl_FragCoord.w;
        if (frag_depth < 8.0) {
            for (int i = 0; i < 16; i++) {
                visibility += texture(shadow_texture, aVertexShUVs_[0] + vec3(poisson_disk[i] * shadow_softness, 0.0)) / 16.0;
            }
        } else if (frag_depth < 24.0) {
            for (int i = 0; i < 8; i++) {
                visibility += texture(shadow_texture, aVertexShUVs_[1] + vec3(poisson_disk[i] * shadow_softness * 0.25, 0.0)) / 8.0;
            }
        } else if (frag_depth < 56.0) {
            for (int i = 0; i < 4; i++) {
                visibility += texture(shadow_texture, aVertexShUVs_[2] + vec3(poisson_disk[i] * shadow_softness * 0.125, 0.0)) / 4.0;
            }
        } else if (frag_depth < 120.0) {
            visibility += texture(shadow_texture, aVertexShUVs_[3]);
        } else {
            // use directional lightmap
            additional_light = texture(lm_direct_texture, lm_uvs).rgb;
        }
    }
    
    vec3 indirect_col = texture(lm_indirect_texture, lm_uvs).rgb;
    
    vec3 sh_l_00 = texture(lm_indirect_sh_texture[0], lm_uvs).rgb;
    vec3 sh_l_10 = texture(lm_indirect_sh_texture[1], lm_uvs).rgb;
    vec3 sh_l_11 = texture(lm_indirect_sh_texture[2], lm_uvs).rgb;
    vec3 sh_l_12 = texture(lm_indirect_sh_texture[3], lm_uvs).rgb;
    
    indirect_col += sh_l_00 + sh_l_10 * normal.y + sh_l_11 * normal.z + sh_l_12 * normal.x;
    
    //indirect_col *= 0.001;
    //visibility *= 0.001;
    
    float depth = 1.0 / gl_FragCoord.w;
    
    const float n = 0.5;
    const float f = 10000.0;
    
    float k = log2(depth / n) / log2(1.0 + f / n);
    int slice = int(k * 24.0);
    
    int ix = int(gl_FragCoord.x);
    int iy = int(gl_FragCoord.y);
    int cell_index = slice * GRID_RES_X * GRID_RES_Y + (iy / (resy / GRID_RES_Y)) * GRID_RES_X + (ix / (resx / GRID_RES_X));
    
    uvec2 offset_and_count = texelFetch(cells_buffer, cell_index).xy;
    
    for (uint i = offset_and_count.x; i < offset_and_count.x + offset_and_count.y; i++) {
        int li = int(texelFetch(items_buffer, int(i)).x);
        
        vec4 pos_and_radius = texelFetch(lights_buffer, li * 3 + 0);
        vec4 col_and_brightness = texelFetch(lights_buffer, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(lights_buffer, li * 3 + 2);
        
        vec3 L = pos_and_radius.xyz - aVertexPos_;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;
        
        float denom = d / pos_and_radius.w + 1.0;
        float atten = 1.0 / (denom * denom);
        
        atten = (atten - LIGHT_ATTEN_CUTOFF / col_and_brightness.w) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);
        
        float _dot1 = max(dot(L, aVertexTBN_[2]), 0.0);
        float _dot2 = dot(L, dir_and_spot.xyz);
        
        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (col_and_brightness.w * atten) > FLT_EPS) {
            additional_light += col_and_brightness.xyz * atten;
        }
    }
    
    vec3 albedo_color = pow(texture(diffuse_texture, aVertexUVs1_).rgb, vec3(gamma));
    vec3 diffuse_color = albedo_color * (sun_col * lambert * visibility + indirect_col + additional_light);
    
    outColor = vec4(diffuse_color, 1.0);
    outNormal.xy += (uVPMatrix * vec4(normal, 0.0)).xy;
    outSpecular = vec4(0.0, 0.5, 0.5, 1.0);
}
