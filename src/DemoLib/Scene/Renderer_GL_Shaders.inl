
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

const char blit_ssr_ms_fs[] = R"(
#version 310 es

#ifdef GL_ES
	precision mediump float;
#endif

layout(binding = 0) uniform mediump sampler2DMS depth_texture;
layout(binding = 1) uniform mediump sampler2DMS norm_texture;
layout(binding = 2) uniform mediump sampler2DMS spec_texture;
layout(binding = 3) uniform mediump sampler2D prev_texture;

layout(location = 0) uniform mat4 proj_matrix;
layout(location = 1) uniform mat4 inv_proj_matrix;
layout(location = 2) uniform vec2 zbuffer_size;

in vec2 aVertexUVs_;

out vec4 outColor;

float distance2(in vec2 P0, in vec2 P1) {
    vec2 d = P1 - P0;
    return d.x * d.x + d.y * d.y;
}

float rand(vec2 co) {
  return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float LinearDepthTexelFetch(ivec2 hit_pixel) {
    const float n = 0.5;
    const float f = 10000.0;

    float depth = texelFetch(depth_texture, hit_pixel, 0).r;
    depth = 2.0 * depth - 1.0;
    depth = 2.0 * n * f / (f + n - depth * (f - n));
    return depth;
}

bool IntersectRay(in vec3 ray_origin_vs, in vec3 ray_dir_vs, out vec2 hit_pixel, out vec3 hit_point) {
    const float n = 0.5;
    const float max_dist = 100.0;

    // Clip ray length to camera near plane
    float ray_length = (ray_origin_vs.z + ray_dir_vs.z * max_dist) > -n ?
                       (-ray_origin_vs.z + n) / ray_dir_vs.z :
                       max_dist;

    vec3 ray_end_vs = ray_origin_vs + ray_length * ray_dir_vs;

    // Project into screen space
    vec4 H0 = proj_matrix * vec4(ray_origin_vs, 1.0),
         H1 = proj_matrix * vec4(ray_end_vs, 1.0);
    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

    vec3 Q0 = ray_origin_vs * k0,
         Q1 = ray_end_vs * k1;

    // Screen-space endpoints
    vec2 P0 = H0.xy * k0, P1 = H1.xy * k1;

    P1 += vec2((distance2(P0, P1) < 0.0001) ? 0.01 : 0.0);

    P0 = 0.5 * P0 + 0.5;
    P1 = 0.5 * P1 + 0.5;

    P0 *= zbuffer_size;
    P1 *= zbuffer_size;

    vec2 delta = P1 - P0;

    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    float step_dir = sign(delta.x);
    float inv_dx = step_dir / delta.x;

    vec3 dQ = (Q1 - Q0) * inv_dx;
    float dk = (k1 - k0) * inv_dx;
    vec2 dP = vec2(step_dir, delta.y * inv_dx);

        float stride = 0.05 * zbuffer_size.x; //16.0;
        dP *= stride;
        dQ *= stride;
        dk *= stride;

    ivec2 c = ivec2(gl_FragCoord.xy);
    float jitter = rand(gl_FragCoord.xy); //float((c.x + c.y) & 1) * 0.5;    

    P0 += dP * (1.0 + jitter);
    Q0 += dQ * (1.0 + jitter);
    k0 += dk * (1.0 + jitter);

    vec3 Q = Q0;
    float k = k0;
    float step_count = 0.0f;
    float end = P1.x * step_dir;
    float prev_zmax_estimate = ray_origin_vs.z;
    hit_pixel = vec2(-1.0, -1.0);

    const float max_steps = 24.0;
        
    for (vec2 P = P0;
        ((P.x * step_dir) <= end) && (step_count < max_steps);
         P += dP, Q.z += dQ.z, k += dk, step_count += 1.0) {

        float ray_zmin = prev_zmax_estimate;
        float ray_zmax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
        prev_zmax_estimate = ray_zmax;

        if(ray_zmin > ray_zmax) {
            float temp = ray_zmin; ray_zmin = ray_zmax; ray_zmax = temp;
        }

        const float z_thickness = 1.0;

        vec2 pixel = permute ? P.yx : P;
        float scene_zmax = -LinearDepthTexelFetch(ivec2(pixel));
        float scene_zmin = scene_zmax - z_thickness;

        if (((ray_zmax >= scene_zmin) && (ray_zmin <= scene_zmax)) || scene_zmax >= -n) {
            hit_pixel = P;
            break;
        }
    }

    vec2 test_pixel = permute ? hit_pixel.yx : hit_pixel;
    bool res = all(lessThanEqual(abs(test_pixel - (zbuffer_size * 0.5)), zbuffer_size * 0.5));

    if (res) {
        Q.xy += dQ.xy * step_count;

        // perform binary search to find intersection more accurately
        for (int i = 0; i < 8; i++) {
            vec2 pixel = permute ? hit_pixel.yx : hit_pixel;
            float scene_z = -LinearDepthTexelFetch(ivec2(pixel));
            float ray_z = Q.z / k;
    
            float depth_diff = ray_z - scene_z;
        
            dQ *= 0.5;
            dP *= 0.5;
            dk *= 0.5;
            if (depth_diff > 0.0) {
                Q += dQ;
                hit_pixel += dP;
                k += dk;
            } else {
                Q -= dQ;
                hit_pixel -= dP;
                k -= dk;
            }
        }

        hit_pixel = permute ? hit_pixel.yx : hit_pixel;
        hit_point = Q * (1.0 / k);
    }
    
    return res;
}

vec3 DecodeNormal(vec2 enc) {
    vec4 nn = vec4(2.0 * enc, 0.0, 0.0) + vec4(-1.0, -1.0, 1.0, -1.0);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return 2.0 * nn.xyz + vec3(0.0, 0.0, -1.0);
}

void main() {
    const float n = 0.5;
    const float f = 10000.0;

    vec4 prev_color = vec4(0.0);
    float prev_depth = -2.0f;

    for (int i = 0; i < 4; i++) {
        vec4 specular = texelFetch(spec_texture, ivec2(aVertexUVs_), i);
        if (specular.w > 0.5) continue;

        float depth = texelFetch(depth_texture, ivec2(aVertexUVs_), i).r;
        depth = 2.0 * depth - 1.0;
        //depth = 2.0 * n * f / (f + n - depth * (f - n));

        if (abs(depth - prev_depth) < 0.005) {
            outColor += 0.25 * prev_color;
            continue;
        }

        vec3 normal = DecodeNormal(texelFetch(norm_texture, ivec2(aVertexUVs_), i).xy);

        vec4 ray_origin_cs = vec4(aVertexUVs_.xy / zbuffer_size, depth, 1.0f);
        ray_origin_cs.xy = 2.0 * ray_origin_cs.xy - 1.0;

        vec4 ray_origin_vs = inv_proj_matrix * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
        vec3 refl_ray_vs = reflect(view_ray_vs, normal);

        vec2 hit_pixel;
        vec3 hit_point;
    
        if (IntersectRay(ray_origin_vs.xyz, refl_ray_vs, hit_pixel, hit_point)) {
            hit_pixel /= zbuffer_size;
            vec4 tex_color = texture(prev_texture, hit_pixel);

            const float R0 = 0.0f;
            float fresnel = R0 + (1.0 - R0) * pow(1.0 - dot(normal, -view_ray_vs), 5.0);;

            vec3 infl = fresnel * specular.xyz;
            infl *= max(1.0 - 2.0 * distance(hit_pixel, vec2(0.5, 0.5)), 0.0);

            prev_depth = depth;
            prev_color = vec4(infl * tex_color.xyz, 1.0);
            outColor += 0.25 * prev_color;
        }
    }
}
)";