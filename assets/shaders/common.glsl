
vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

highp float rand(highp vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 RGBMDecode(vec4 rgbm) {
    return 6.0 * rgbm.rgb * rgbm.a;
}

const vec2 poisson_disk[16] = vec2[16](
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
    vec2(0.75, -0.81)
);

#define M_PI 3.1415926535897932384626433832795

float GetSunVisibility(float frag_depth, sampler2DShadow shadow_texture, vec3 aVertexShUVs[4]) {
    const vec2 shadow_softness = vec2(3.0 / $ShadRes.0, 1.5 / $ShadRes.0);
    
    float visibility = 0.0;

    highp float r = M_PI * (-1.0 + 2.0 * rand(gl_FragCoord.xy));
    highp vec2 rx = vec2(cos(r), sin(r));
    highp vec2 ry = vec2(rx.y, -rx.x);
    
    if (frag_depth < $ShadCasc0Dist) {
        const highp float weight = 1.0 / $ShadCasc0Samp.0;
        for (int i = 0; i < $ShadCasc0Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[0] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        visibility *= weight;
    } else if (frag_depth < $ShadCasc1Dist) {
        const highp float weight = 1.0 / $ShadCasc1Samp.0;
        for (int i = 0; i < $ShadCasc1Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[1] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        visibility *= weight;
    } else if (frag_depth < $ShadCasc2Dist) {
        const highp float weight = 1.0 / $ShadCasc2Samp.0;
        for (int i = 0; i < $ShadCasc2Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[2] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        visibility *= weight;
    } else if (frag_depth < $ShadCasc3Dist) {
        const highp float weight = 1.0 / $ShadCasc3Samp.0;
        for (int i = 0; i < $ShadCasc3Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[3] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        float t = smoothstep(0.95 * $ShadCasc3Dist, $ShadCasc3Dist, frag_depth);
        visibility = mix(visibility * weight, 1.0, t);
    } else {
        // use direct sun lightmap?
        visibility = 1.0;
    }
    
    return visibility;
}

vec2 EncodeNormal(vec3 n) {
    vec2 enc = normalize(n.xy) * (sqrt(-n.z * 0.5 + 0.5));
    enc = enc * 0.5 + 0.5;
    return enc;
}

vec3 DecodeNormal(vec2 enc) {
    vec4 nn = vec4(2.0 * enc, 0.0, 0.0) + vec4(-1.0, -1.0, 1.0, -1.0);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return 2.0 * nn.xyz + vec3(0.0, 0.0, -1.0);
}