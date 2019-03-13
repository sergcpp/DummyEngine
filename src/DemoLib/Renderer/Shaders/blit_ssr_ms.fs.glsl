R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

#define BSEARCH_STEPS 4

layout(binding = 0) uniform mediump sampler2DMS depth_texture;
layout(binding = 1) uniform mediump sampler2DMS norm_texture;
layout(binding = 2) uniform mediump sampler2DMS spec_texture;
layout(binding = 3) uniform mediump sampler2D prev_texture;
layout(binding = 4) uniform mediump samplerCube env_texture;

layout(location = 0) uniform mat4 uProjMatrix;      // projection matrix for current frame
layout(location = 1) uniform mat4 uInvProjMatrix;   // inverse projection matrix for current frame
layout(location = 2) uniform mat4 uDeltaMatrix;     // 'delta' matrix to transform points from current frame to previous
layout(location = 3) uniform vec2 uZBufferSize;     // depth buffer resolution
layout(location = 4) uniform mat4 uInvViewMatrix;   // inverse view matrix for current frame
layout(location = 5) uniform vec4 uClipInfo;        // camera clip info

in vec2 aVertexUVs_;

out vec4 outColor;

float distance2(in vec2 P0, in vec2 P1) {
    vec2 d = P1 - P0;
    return d.x * d.x + d.y * d.y;
}

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 RGBMDecode(vec4 rgbm) {
    return 6.0 * rgbm.rgb * rgbm.a;
}

float LinearDepthTexelFetch(ivec2 hit_pixel) {
    float depth = texelFetch(depth_texture, hit_pixel, 0).r;
    return uClipInfo[0] / (depth * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
}

bool IntersectRay(in vec3 ray_origin_vs, in vec3 ray_dir_vs, out vec2 hit_pixel, out vec3 hit_point) {
    const float max_dist = 100.0;

    // from "Efficient GPU Screen-Space Ray Tracing"

    // Clip ray length to camera near plane
    float ray_length = (ray_origin_vs.z + ray_dir_vs.z * max_dist) > -uClipInfo[1] ?
                       (-ray_origin_vs.z - uClipInfo[1]) / ray_dir_vs.z :
                       max_dist;

    vec3 ray_end_vs = ray_origin_vs + ray_length * ray_dir_vs;

    // Project into screen space
    vec4 H0 = uProjMatrix * vec4(ray_origin_vs, 1.0),
         H1 = uProjMatrix * vec4(ray_end_vs, 1.0);
    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

    vec3 Q0 = ray_origin_vs * k0,
         Q1 = ray_end_vs * k1;

    // Screen-space endpoints
    vec2 P0 = H0.xy * k0, P1 = H1.xy * k1;

    P1 += vec2((distance2(P0, P1) < 0.0001) ? 0.01 : 0.0);

    P0 = 0.5 * P0 + 0.5;
    P1 = 0.5 * P1 + 0.5;

    P0 *= uZBufferSize;
    P1 *= uZBufferSize;

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
    vec2 dP = vec2(step_dir, delta.y * inv_dx);

    vec3 dQ = (Q1 - Q0) * inv_dx;
    float dk = (k1 - k0) * inv_dx;

    float stride = 0.025 * uZBufferSize.x;
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
    float prev_zmax_estimate = ray_origin_vs.z + 0.1;
    hit_pixel = vec2(-1.0, -1.0);

    const float max_steps = 12.0;
        
    for (vec2 P = P0;
        ((P.x * step_dir) <= end) && (step_count < max_steps);
         P += dP, Q.z += dQ.z, k += dk, step_count += 1.0) {

        float ray_zmin = prev_zmax_estimate;
        // take half of step forward
        float ray_zmax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k) + 0.1;
        prev_zmax_estimate = ray_zmax;

        if(ray_zmin > ray_zmax) {
            float temp = ray_zmin; ray_zmin = ray_zmax; ray_zmax = temp;
        }

        const float z_thickness = 1.0;

        vec2 pixel = permute ? P.yx : P;

        float scene_zmax = -LinearDepthTexelFetch(ivec2(pixel));
        float scene_zmin = scene_zmax - z_thickness;

        if ((ray_zmax >= scene_zmin) && (ray_zmin <= scene_zmax)) {
            hit_pixel = P;
            break;
        }
    }

    vec2 test_pixel = permute ? hit_pixel.yx : hit_pixel;
    bool res = all(lessThanEqual(abs(test_pixel - (uZBufferSize * 0.5)), uZBufferSize * 0.5));

#if BSEARCH_STEPS != 0
    if (res) {
        Q.xy += dQ.xy * step_count;

        // perform binary search to find intersection more accurately
        for (int i = 0; i < BSEARCH_STEPS; i++) {
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
#else
    hit_pixel = permute ? hit_pixel.yx : hit_pixel;
    hit_point = Q * (1.0 / k);
#endif
    
    return res;
}

vec3 DecodeNormal(vec2 enc) {
    vec4 nn = vec4(2.0 * enc, 0.0, 0.0) + vec4(-1.0, -1.0, 1.0, -1.0);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(max(l, 0.0));
    return 2.0 * nn.xyz + vec3(0.0, 0.0, -1.0);
}

void main() {
    vec4 specular = texelFetch(spec_texture, ivec2(aVertexUVs_), 0);
    if ((specular.x + specular.y + specular.z) < 0.0001) return;

    float depth = texelFetch(depth_texture, ivec2(aVertexUVs_), 0).r;
    depth = 2.0 * depth - 1.0;

    vec3 normal = DecodeNormal(texelFetch(norm_texture, ivec2(aVertexUVs_), 0).xy);

    vec4 ray_origin_cs = vec4(aVertexUVs_.xy / uZBufferSize, depth, 1.0f);
    ray_origin_cs.xy = 2.0 * ray_origin_cs.xy - 1.0;

    vec4 ray_origin_vs = uInvProjMatrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 refl_ray_vs = reflect(view_ray_vs, normal);

    const float R0 = 0.25f;
    float ssr_factor = pow(1.0 - dot(normal, -view_ray_vs), 5.0);
    float fresnel = R0 + (1.0 - R0) * ssr_factor;
    vec3 infl = vec3(fresnel) * specular.xyz;

    float tex_lod = 8.0 * (1.0 - specular.w);

    vec3 refl_ray_ws = normalize((uInvViewMatrix * vec4(refl_ray_vs, 0.0)).xyz);
    outColor = vec4(infl * clamp(RGBMDecode(textureLod(env_texture, refl_ray_ws, tex_lod)), vec3(0.0), vec3(10.0)), 1.0);

    vec2 hit_pixel;
    vec3 hit_point;
    
    if (IntersectRay(ray_origin_vs.xyz, refl_ray_vs, hit_pixel, hit_point)) {
        hit_pixel /= uZBufferSize;

        // reproject hitpoint in view space of previous frame
        vec4 hit_prev = uDeltaMatrix * vec4(hit_point, 1.0);
        hit_prev = uProjMatrix * hit_prev;
        hit_prev /= hit_prev.w;
        hit_prev.xy = 0.5 * hit_prev.xy + 0.5;
            
        vec4 tex_color = textureLod(prev_texture, hit_prev.xy, 0.05 * tex_lod * distance(ray_origin_vs.xyz, hit_point));

        float mix_factor = max(1.0 - 2.0 * distance(hit_pixel, vec2(0.5, 0.5)), 0.0);
        outColor.xyz = mix(outColor.xyz, tex_color.xyz, mix_factor);
    }
}
)"